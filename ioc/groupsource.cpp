/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <pvxs/source.h>
#include <pvxs/log.h>

#include <dbEvent.h>
#include <dbChannel.h>

#include "groupsource.h"
#include "iocshcommand.h"
#include "groupsrcsubscriptionctx.h"
#include "iocsource.h"
#include "dberrormessage.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.group.source");

/**
 * Constructor for GroupSource registrar.
 */
GroupSource::GroupSource()
		:eventContext(db_init_events()) // Initialise event context
{
	// Get GroupPv configuration and register each pv name in the server
	runOnPvxsServer([this](IOCServer* pPvxsServer) {
		auto names(std::make_shared<std::set<std::string >>());

		// Lock map and get names
		{
			epicsGuard<epicsMutex> G(pPvxsServer->groupMapMutex);

			// For each defined group, add group name to the list of all records
			for (auto& groupMapEntry: pPvxsServer->groupMap) {
				auto& groupName = groupMapEntry.first;
				names->insert(groupName);
			}
		}

		allRecords.names = names;

		// Start event pump
		if (!eventContext) {
			throw std::runtime_error("Group Source: Event Context failed to initialise: db_init_events()");
		}

		if (db_start_events(eventContext.get(), "qsrvGroup", nullptr, nullptr, epicsThreadPriorityCAServerLow - 1)) {
			throw std::runtime_error("Could not start event thread: db_start_events()");
		}
	});
}

/**
 * Handle the create source operation.  This is called once when the source is created.
 * We will register all of the database records that have been loaded until this time as pv names in this
 * source.
 * @param channelControl
 */
void GroupSource::onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) {
	auto& sourceName = channelControl->name();
	log_debug_printf(_logname, "Accepting channel for '%s'\n", sourceName.c_str());

	runOnPvxsServer([&](IOCServer* pPvxsServer) {
		// Create callbacks for handling requests and group subscriptions
		auto& group = pPvxsServer->groupMap[sourceName];
		createRequestAndSubscriptionHandlers(channelControl, group);
	});

}

/**
 * Implementation of the onList() interface of pvxs::server::Source to return a list of all records
 * managed by this source.
 *
 * @return all records managed by this source
 */
GroupSource::List GroupSource::onList() {
	return allRecords;
}

/**
 * Respond to search requests.  For each matching pv, claim that pv
 *
 * @param searchOperation the search operation
 */
void GroupSource::onSearch(Search& searchOperation) {
	runOnPvxsServer([&](IOCServer* pPvxsServer) {
		for (auto& pv: searchOperation) {
			if (allRecords.names->count(pv.name()) == 1) {
				pv.claim();
				log_debug_printf(_logname, "Claiming '%s'\n", pv.name());
			}
		}
	});
}

/**
 * Respond to the show request by displaying a list of all the PVs hosted in this ioc
 *
 * @param outputStream the stream to show the list on
 */
void GroupSource::show(std::ostream& outputStream) {
	outputStream << "IOC";
	for (auto& name: *GroupSource::allRecords.names) {
		outputStream << "\n" << indent{} << name;
	}
}

/**
 * Create request and subscription handlers for group record sources
 *
 * @param channelControl the control channel pointer that we got from onCreate
 * @param pChannel the pointer to the database channel to set up the handlers for
 */
void GroupSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl,
		IOCGroup& group) {
	auto subscriptionContext(std::make_shared<GroupSourceSubscriptionCtx>(group));

	// Get and Put requests
	channelControl->onOp([&](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
		onOp(group, std::move(channelConnectOperation));
	});

	// TODO Subscription requests
}

/**
 * Handler for the onOp event raised by pvxs Sources when they are started, in order to define the get and put handlers
 * on a per source basis.
 * This is called after the event has been intercepted and we add the group to the call.
 *
 * @param group the group to which the get/put operation pertains
 * @param channelConnectOperation the channel connect operation object
 */
void GroupSource::onOp(IOCGroup& group,
		std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {// First stage for handling any request is to announce the channel type with a `connect()` call
// @note The type signalled here must match the eventual type returned by a pvxs get
	channelConnectOperation->connect(group.valueTemplate);

	// pvxs get
	channelConnectOperation->onGet([&](std::__1::unique_ptr<server::ExecOp>&& getOperation) {
		get(group, getOperation);
	});

	// pvxs put
	channelConnectOperation
			->onPut([&](std::__1::unique_ptr<server::ExecOp>&& putOperation, Value&& value) {
				putGroup(group, putOperation, value);
			});
}

/**
 * Handler invoked when a peer sends data on a PUT
 *
 * @param group
 * @param putOperation
 * @param value
 */
void GroupSource::putGroup(IOCGroup& group, std::unique_ptr<server::ExecOp>& putOperation, const Value& value) {
	// Loop through all fields
	// TODO putOrder
	for (auto& field: group.fields) {
		// find the leaf node in which to put the value
		try {
			auto leafNode = field.findIn(value);

			if (leafNode.valid()) {
				IOCSource::put((std::shared_ptr<dbChannel>)field.channel, leafNode);
			}
		} catch (std::exception& e) {
			putOperation->error(e.what());
			return;
		}
	}
	putOperation->reply();
}

/**
 * Handle the get operation
 *
 * @param group the group to get
 * @param getOperation the current executing operation
 */
void GroupSource::get(IOCGroup& group, std::unique_ptr<server::ExecOp>& getOperation) {
	groupGet(group, [&getOperation](Value& value) {
		getOperation->reply(value);
	}, [&getOperation](const char* errorMessage) {
		getOperation->error(errorMessage);
	});
}

/**
 * Get each field and make up the whole group structure
 * @param group group to base result on
 * @param forValues return values
 * @param forProperties return meta data
 * @param returnFn function to call with the result
 * @param errorFn function to call on errors
 */
void GroupSource::groupGet(IOCGroup& group, const std::function<void(Value&)>& returnFn,
		const std::function<void(const char*)>& errorFn) {

	// Make an empty value to return
	auto returnValue(group.valueTemplate.cloneEmpty());

	// TODO lock the records that are impacted during the read
	// For each field, get the value
	for (auto& field: group.fields) {
		// find the leaf node in which to set the value
		auto leafNode = field.findIn(returnValue);

		if (leafNode.valid()) {
			try {
				if (field.isMeta) {
					IOCSource::get((std::shared_ptr<dbChannel>)field.channel,
							leafNode, false,
							true,
							[&leafNode](Value& value) {
								auto alarm = value["alarm"];
								auto timestamp = value["timestamp"];
								auto display = value["display"];
								auto control = value["control"];
								auto valueAlarm = value["valueAlarm"];
								if (alarm.valid()) {
									leafNode["alarm"] = alarm;
								}
								if (timestamp.valid()) {
									leafNode["timestamp"] = timestamp;
								}
								if (display.valid()) {
									leafNode["display"] = display;
								}
								if (control.valid()) {
									leafNode["control"] = control;
								}
								if (valueAlarm.valid()) {
									leafNode["valueAlarm"] = valueAlarm;
								}
							}, [](const char* errorMessage) {
								throw std::runtime_error(errorMessage);
							});
				} else if (!field.fieldName.empty()) {
					IOCSource::get((std::shared_ptr<dbChannel>)field.channel,
							leafNode, true,
							false,
							[&leafNode](Value& value) {
								leafNode.assign(value);
							}, [](const char* errorMessage) {
								throw std::runtime_error(errorMessage);
							});
				}
			} catch (std::exception& e) {
				std::stringstream errorString;
				errorString << "Error retrieving value for pvName: " << group.name << (field.name.empty() ? "/" : ".")
				            << field.fullName << " : "
				            << e.what();
				errorFn(errorString.str().c_str());
				return;
			}
		}
	}

	// Send reply
	returnFn(returnValue);
}

} // ioc
} // pvxs
