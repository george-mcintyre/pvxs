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
#include "fieldsubscriptionctx.h"
#include "groupsrcsubscriptionctx.h"
#include "iocsource.h"
#include "dbmanylocker.h"
#include "dblocker.h"

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
 *
 * @param channelControl channel control object provided by the pvxs framework
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
 * @param group the group that we're creating the request and subscription handlers for
 */
void GroupSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl,
		IOCGroup& group) {
	// Get and Put requests
	channelControl->onOp([&](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
		onOp(group, std::move(channelConnectOperation));
	});

	auto subscriptionContext(std::make_shared<GroupSourceSubscriptionCtx>(group));
	// TODO evaluate lifetime of `this` pointer
	channelControl
			->onSubscribe([this, subscriptionContext](std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
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
 *
 * @param group group to base result on
 * @param returnFn function to call with the result
 * @param errorFn function to call on errors
 */
void GroupSource::groupGet(IOCGroup& group, const std::function<void(Value&)>& returnFn,
		const std::function<void(const char*)>& errorFn) {

	// Make an empty value to return
	auto returnValue(group.valueTemplate.cloneEmpty());

	// For each field, get the value
	for (auto& field: group.fields) {
		// find the leaf node in which to set the value
		auto leafNode = field.findIn(returnValue);

		if (leafNode.valid()) {
			try {
				if (field.isMeta) {
					IOCSource::get((dbChannel*)field.valueChannel,
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
					IOCSource::get((dbChannel*)field.valueChannel,
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
 * Called when a client pauses / stops a subscription it has been subscribed to.
 * This function loops over all fields event subscriptions the group subscription context and disables each of them.
 *
 * @param groupSubscriptionCtx the group subscription context
 */
void GroupSource::onDisableSubscription(const std::shared_ptr<GroupSourceSubscriptionCtx>& groupSubscriptionCtx) {
	for (auto& fieldSubscriptionCtx: groupSubscriptionCtx->fieldSubscriptionContexts) {
		auto pValueEventSubscription = fieldSubscriptionCtx.pValueEventSubscription.get();
		auto pPropertiesEventSubscription = fieldSubscriptionCtx.pPropertiesEventSubscription.get();
		db_event_disable(pValueEventSubscription);
		db_event_disable(pPropertiesEventSubscription);
	}
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
		std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
	// First stage for handling any request is to announce the channel type with a `connect()` call
	// @note The type signalled here must match the eventual type returned by a pvxs get
	channelConnectOperation->connect(group.valueTemplate);

	// register handler for pvxs group get
	channelConnectOperation->onGet([&](std::__1::unique_ptr<server::ExecOp>&& getOperation) {
		get(group, getOperation);
	});

	// register handler for pvxs group put
	channelConnectOperation
			->onPut([&](std::__1::unique_ptr<server::ExecOp>&& putOperation, Value&& value) {
				putGroup(group, putOperation, value);
			});
}

/**
 * Called by the framework when a client subscribes to a channel.  We intercept the call before this function is called
 * to add a new group subscription context containing a reference to the group.
 * This function must initialise all of the field's subscription contexts.
 *
 * @param groupSubscriptionCtx a new group subscription context
 * @param subscriptionOperation the group subscription operation
 */
void GroupSource::onSubscribe(const std::shared_ptr<GroupSourceSubscriptionCtx>& groupSubscriptionCtx,
		std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) const {
	// inform peer of data type and acquire control of the subscription queue
	groupSubscriptionCtx->subscriptionControl = subscriptionOperation
			->connect(groupSubscriptionCtx->group.valueTemplate);

	// Initialise the field subscription contexts.  One for each group field
	// This is stored in the group context
	groupSubscriptionCtx->fieldSubscriptionContexts.reserve(groupSubscriptionCtx->group.fields.size());
	for (auto& field: groupSubscriptionCtx->group.fields) {
		groupSubscriptionCtx->fieldSubscriptionContexts.emplace_back(field, groupSubscriptionCtx.get());
		auto& fieldSubscriptionContext = groupSubscriptionCtx->fieldSubscriptionContexts.back();

		// Two subscription are made for each group channel for pvxs
		fieldSubscriptionContext
				.subscribeField(eventContext.get(), subscriptionValueCallback, DBE_VALUE | DBE_ALARM);
		fieldSubscriptionContext
				.subscribeField(eventContext.get(), subscriptionPropertiesCallback, DBE_PROPERTY, false);
	}

	// If all goes well, set up handlers for start and stop monitoring events
	groupSubscriptionCtx->subscriptionControl->onStart([groupSubscriptionCtx](bool isStarting) {
		onStart(groupSubscriptionCtx, isStarting);
	});
}

/**
 * Called by the framework when the monitoring client issues a start or stop subscription.  We
 * intercept the framework's call prior to entering here, and add the group subscription context
 * containing a list of field contexts and their event subscriptions to manage.
 *
 * @param groupSubscriptionCtx the group subscription context
 * @param isStarting true if the client issued a start subscription request, false otherwise
 */
void GroupSource::onStart(const std::shared_ptr<GroupSourceSubscriptionCtx>& groupSubscriptionCtx, bool isStarting) {
	if (isStarting) {
		onStartSubscription(groupSubscriptionCtx);
	} else {
		onDisableSubscription(groupSubscriptionCtx);
	}
}

/**
 * Called when a client starts a subscription it has subscribed to.  For each field in the subscription,
 * enable events and post a single event to both the values and properties event channels to kick things off.
 *
 * @param groupSubscriptionCtx the group subscription context containing the field event subscriptions to start
 */
void GroupSource::onStartSubscription(const std::shared_ptr<GroupSourceSubscriptionCtx>& groupSubscriptionCtx) {
	for (auto& fieldSubscriptionCtx: groupSubscriptionCtx->fieldSubscriptionContexts) {
		auto pValueEventSubscription = fieldSubscriptionCtx.pValueEventSubscription.get();
		auto pPropertiesEventSubscription = fieldSubscriptionCtx.pPropertiesEventSubscription.get();
		db_event_enable(pValueEventSubscription);
		db_post_single_event(pValueEventSubscription);
		db_event_enable(pPropertiesEventSubscription);
		db_post_single_event(pPropertiesEventSubscription);
	}
}

/**
 * Handler invoked when a peer sends data on a PUT
 *
 * @param group the group to which the data is posted
 * @param putOperation the put operation object to use to interact with the client
 * @param value the value being posted
 */
void GroupSource::putGroup(IOCGroup& group, std::unique_ptr<server::ExecOp>& putOperation, const Value& value) {
	try {
		// If the group is configured for an atomic put operation,
		// then we need to put all the fields at once, so we lock them all together
		// and do the operation in one go
		if (group.atomicPutGet) {
			// Lock all the fields
			DBManyLocker G(group.lock);
			// Loop through all fields
			for (auto& field: group.fields) {
				// Put the field
				putField(value, field);
			}
			// Unlock the all group fields when the locker goes out of scope

		} else {
			// Otherwise, this is a non-atomic operation, and we need to `put` each field individually,
			// locking each of them independently of each other.

			// Loop through all fields
			for (auto& field: group.fields) {
				// Lock this field
				DBLocker F(((dbChannel*)field.valueChannel)->addr.precord);
				// Put the field
				putField(value, field);
				// Unlock this field when locker goes out of scope
			}
		}

	} catch (std::exception& e) {
		// Unlock all locked fields when lockers go out of scope
		// Post error message to put operation object
		putOperation->error(e.what());
		return;
	}

	// If all went ok then let the client know
	putOperation->reply();
}

/**
 * Called by putGroup() to perform the actual put of the given value into the group field specified.
 * The value will be the whole value template that the group represents but only the fields passed in by
 * the client will be set.  So we simply check to see whether the parts of value that are referenced by the
 * provided field parameter are included in the given value, and if so, we pull them out and do a low level
 * database put.
 *
 * @param value the sparsely populated value to put into the group's field
 * @param field the group field to check against
 */
void GroupSource::putField(const Value& value, const IOCGroupField& field) {
	// find the leaf node that the field refers to in the given value
	auto leafNode = field.findIn(value);

	// If the field references a valid part of the given value then we can send it to the database
	if (leafNode.valid()) {
		IOCSource::put((dbChannel*)field.valueChannel, leafNode);
	}
}

/**
 * Used by both value and property subscriptions, this function will get the database value and return it
 * to the monitor.  It is called whenever a field subscription event is received.
 *
 * @param fieldSubscriptionCtx the field subscription context
 * @param pChannel the channel to get the database value
 * @param eventsRemaining the number of events remaining
 * @param pDbFieldLog the database field log
 */
void GroupSource::subscriptionCallback(FieldSubscriptionCtx* fieldSubscriptionCtx,
		dbChannel* pChannel, int eventsRemaining, struct db_field_log* pDbFieldLog, bool forValues) {
	// TODO use eventsRemaining
	// TODO use pDbFieldLog

	// Make sure that the initial subscription update has occurred on all channels before continuing
	// As we make two initial updates when opening a new subscription, for each field,
	// we need both fields to have completed before continuing
	auto& pGroupCtx = fieldSubscriptionCtx->pGroupCtx;
	for (auto& fieldCtx: pGroupCtx->fieldSubscriptionContexts) {
		if (!fieldCtx.hadValueEvent || !fieldCtx.hadPropertyEvent) {
			return;
		}
	}

	// Get and return the value to the monitor
	auto& group = pGroupCtx->group;
	auto field = fieldSubscriptionCtx->field;
	auto pFinalChannel = (dbChannel*)(forValues ? field->valueChannel : field->propertiesChannel);
	auto returnValue = group.valueTemplate.cloneEmpty();

	// Lock all fields in the group when getting this subscribed fields value
	DBManyLocker G(group.lock);

	// Find leaf node within the return value.  This will be a reference into the returnValue.
	// So that if we assign the leafNode with the value we `get()` back, then returnValue will be updated
	auto leafNode = field->findIn(returnValue);
	IOCSource::get(pFinalChannel, leafNode, forValues, !forValues,
			[&returnValue, &leafNode, &fieldSubscriptionCtx](Value& value) {
				// Return value
				if (value.valid()) {
					leafNode.assign(value);
				}
				// Post a reply to the group subscription monitor if everything is OK
				fieldSubscriptionCtx->pGroupCtx->subscriptionControl->tryPost(returnValue);
			}, [](const char* errorMessage) {
				// Otherwise throw an error
				throw std::runtime_error(errorMessage);
			});
	// Unlock fields in group when locker goes out of scope
}

/**
 * This callback handles notifying of updates to subscribed-to pv properties.
 *
 * @param userArg the user argument passed to the callback function from the framework: a FieldSubscriptionCtx
 * @param pChannel the channel that property change notification came from
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes being notified
 */
void GroupSource::subscriptionPropertiesCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (FieldSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext->pGroupCtx)->eventLock);
		subscriptionContext->hadPropertyEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog, FOR_PROPERTIES);
}

/**
 * This callback handles notifying of updates to subscribed-to pv values.
 *
 * @param userArg the user argument passed to the callback function from the framework: a FieldSubscriptionCtx
 * @param pChannel the channel that value change notification came from
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes being notified
 */
void GroupSource::subscriptionValueCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (FieldSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext->pGroupCtx)->eventLock);
		subscriptionContext->hadValueEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog, FOR_VALUES);
}

} // ioc
} // pvxs
