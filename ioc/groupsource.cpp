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
	// Get and Put requests
	channelControl->onOp([&](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
		onOp(group, std::move(channelConnectOperation));
	});

	// TODO Subscription requests
	auto subscriptionContext(std::make_shared<GroupSourceSubscriptionCtx>(group));
	channelControl
			->onSubscribe([this,subscriptionContext](std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
				onSubscribe(subscriptionContext, std::move(subscriptionOperation));
			});
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

/**
 * Called when a client pauses / stops a subscription it has been subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void GroupSource::onDisableSubscription(const std::shared_ptr<GroupSourceSubscriptionCtx>& subscriptionContext) {
	db_event_disable(subscriptionContext->pValueEventSubscription.get());
	db_event_disable(subscriptionContext->pPropertiesEventSubscription.get());
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
 * Called by the framework when a client subscribes to a channel.  We intercept the call before this function is called
 * to add a new subscription context with a value prototype matching the channel definition.
 *
 * @param subscriptionContext a new subscription context with a value prototype matching the channel
 * @param subscriptionOperation the channel subscription operation
 */
void GroupSource::onSubscribe(const std::shared_ptr<GroupSourceSubscriptionCtx>& subscriptionContext,
		std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) const {
	// inform peer of data type and acquire control of the subscription queue
	subscriptionContext->subscriptionControl = subscriptionOperation->connect(subscriptionContext->prototype);

	// Two subscription are made for pvxs
	// first subscription is for Value changes
	addGroupSubscriptionEvent(Value, eventContext, subscriptionContext, DBE_VALUE | DBE_ALARM);
	// second subscription is for Property changes
	addGroupSubscriptionEvent(Properties, eventContext, subscriptionContext, DBE_PROPERTY);

	// If either fail to complete then raise an error (removes last ref to shared_ptr subscriptionContext)
	if (!subscriptionContext->pValueEventSubscription
			|| !subscriptionContext->pPropertiesEventSubscription) {
		throw std::runtime_error("Failed to create db subscription");
	}

	// If all goes well, Set up handlers for start and stop monitoring events
	subscriptionContext->subscriptionControl->onStart([&subscriptionContext](bool isStarting) {
		onStart(subscriptionContext, isStarting);
	});
}

/**
 * Called by the framework when the monitoring client issues a start or stop subscription
 *
 * @param subscriptionContext the subscription context
 * @param isStarting true if the client issued a start subscription request, false otherwise
 */
void GroupSource::onStart(const std::shared_ptr<GroupSourceSubscriptionCtx>& subscriptionContext, bool isStarting) {
	if (isStarting) {
		onStartSubscription(subscriptionContext);
	} else {
		onDisableSubscription(subscriptionContext);
	}
}

/**
 * Called when a client starts a subscription it has subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void GroupSource::onStartSubscription(const std::shared_ptr<GroupSourceSubscriptionCtx>& subscriptionContext) {
	db_event_enable(subscriptionContext->pValueEventSubscription.get());
	db_event_enable(subscriptionContext->pPropertiesEventSubscription.get());
	db_post_single_event(subscriptionContext->pValueEventSubscription.get());
	db_post_single_event(subscriptionContext->pPropertiesEventSubscription.get());
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
				// TODO set metadata
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
 * Used by both value and property subscriptions, this function will get and return the database value to the monitor.
 *
 * @param subscriptionContext the subscription context
 * @param pChannel the channel to get the database value
 * @param eventsRemaining the number of events remaining
 * @param pDbFieldLog the database field log
 */
void GroupSource::subscriptionCallback(GroupSourceSubscriptionCtx* subscriptionContext, dbChannel* pChannel,
		int eventsRemaining, struct db_field_log* pDbFieldLog) {
	// Make sure that the initial subscription update has occurred on both channels before continuing
	// As we make two initial updates when opening a new subscription, we need both to have completed before continuing
	if (!subscriptionContext->hadValueEvent || !subscriptionContext->hadPropertyEvent) {
		return;
	}

/*
	// Get and return the value to the monitor
	bool forValue = (subscriptionContext->pValueChannel.get() == pChannel);
	auto pdbChannel = forValue ? subscriptionContext->pValueChannel : subscriptionContext->pPropertiesChannel;
	IOCSource::get(pdbChannel, subscriptionContext->prototype, forValue, !forValue,
			[subscriptionContext](Value& value) {
				// Return value
				subscriptionContext->subscriptionControl->tryPost(value);
			}, [](const char* errorMessage) {
				throw std::runtime_error(errorMessage);
			});
*/
}

/**
 * This callback handles notifying of updates to subscribed-to pv properties.  The macro addSubscriptionEvent(...)
 * creates the call to this function, so your IDE may mark it as unused (don't believe it :) )
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param group the group whose value has been updated
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void GroupSource::subscriptionPropertiesCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (GroupSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadPropertyEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog);
}

/**
 * This callback handles notifying of updates to subscribed-to pv values.  The macro addSubscriptionEvent(...)
 * creates the call to this function, so your IDE may mark it as unused (don't believe it :) )
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param pChannel the group whose value has been updated
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void GroupSource::subscriptionValueCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (GroupSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadValueEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog);
}


} // ioc
} // pvxs
