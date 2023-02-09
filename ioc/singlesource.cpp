/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <dbStaticLib.h>
#include <special.h>

#include <pvxs/log.h>
#include <pvxs/nt.h>
#include <pvxs/source.h>

#include "dbentry.h"
#include "dberrormessage.h"
#include "dblocker.h"
#include "iocsource.h"
#include "singlesource.h"
#include "singlesrcsubscriptionctx.h"
#include "credentials.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.single.source");

/**
 * Constructor for SingleSource registrar.
 */
SingleSource::SingleSource()
		:eventContext(db_init_events()) // Initialise event context
{
	auto names(std::make_shared<std::set<std::string >>());

	//  For each record type and for each record in that type, add record name to the list of all records
	DBEntry dbEntry;
	for (long status = dbFirstRecordType(dbEntry); !status; status = dbNextRecordType(dbEntry)) {
		for (status = dbFirstRecord(dbEntry); !status; status = dbNextRecord(dbEntry)) {
			names->insert(dbEntry->precnode->recordname);
		}
	}

	allRecords.names = names;

	// Start event pump
	if (!eventContext) {
		throw std::runtime_error("Single Source: Event Context failed to initialise: db_init_events()");
	}

	if (db_start_events(eventContext.get(), "qsrvSingle", nullptr, nullptr, epicsThreadPriorityCAServerLow - 1)) {
		throw std::runtime_error("Could not start event thread: db_start_events()");
	}
}

/**
 * Handle the create source operation.  This is called once when the source is created.
 * We will register all of the database records that have been loaded until this time as pv names in this
 * source.
 * @param channelControl
 */
void SingleSource::onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) {
	auto sourceName(channelControl->name().c_str());
	dbChannel* pDbChannel = dbChannelCreate(sourceName);
	if (!pDbChannel) {
		log_debug_printf(_logname, "Ignore requested source '%s'\n", sourceName);
		return;
	}
	log_debug_printf(_logname, "Accepting channel for '%s'\n", sourceName);

	// Set up a shared pointer to the database channel and provide a deleter lambda for when it will eventually be deleted
	std::shared_ptr<dbChannel> dbChannelSharedPtr(pDbChannel, [](dbChannel* ch) { dbChannelDelete(ch); });

	DBErrorMessage dbErrorMessage(dbChannelOpen(dbChannelSharedPtr.get()));
	if (dbErrorMessage) {
		log_debug_printf(_logname, "Error opening database channel for '%s: %s'\n", sourceName,
				dbErrorMessage.c_str());
		throw std::runtime_error(dbErrorMessage.c_str());
	}

	// Create callbacks for handling requests and channel subscriptions
	createRequestAndSubscriptionHandlers(std::move(channelControl), dbChannelSharedPtr);
}

SingleSource::List SingleSource::onList() {
	return allRecords;
}

/**
 * Respond to search requests.  For each matching pv, claim that pv
 *
 * @param searchOperation the search operation
 */
void SingleSource::onSearch(Search& searchOperation) {
	for (auto& pv: searchOperation) {
		if (!dbChannelTest(pv.name())) {
			pv.claim();
			log_debug_printf(_logname, "Claiming '%s'\n", pv.name());
		}
	}
}

/**
 * Respond to the show request by displaying a list of all the PVs hosted in this ioc
 *
 * @param outputStream the stream to show the list on
 */
void SingleSource::show(std::ostream& outputStream) {
	outputStream << "IOC";
	for (auto& name: *SingleSource::allRecords.names) {
		outputStream << "\n" << indent{} << name;
	}
}

/**
 * Create request and subscription handlers for single record sources
 *
 * @param channelControl the control channel pointer that we got from onCreate
 * @param dbChannelSharedPtr the pointer to the database channel to set up the handlers for
 */
void SingleSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>&& channelControl,
		const std::shared_ptr<dbChannel>& dbChannelSharedPtr) {
	auto subscriptionContext(std::make_shared<SingleSourceSubscriptionCtx>(dbChannelSharedPtr));

	Value valuePrototype = getValuePrototype(dbChannelSharedPtr);

	// Get and Put requests
	channelControl
			->onOp([dbChannelSharedPtr, valuePrototype](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
				onOp(dbChannelSharedPtr, valuePrototype, std::move(channelConnectOperation));
			});

	// Subscription requests
	// Shared ptr for one of captured vars below
	subscriptionContext->prototype = valuePrototype.cloneEmpty();
	channelControl
			->onSubscribe([this, subscriptionContext](std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
				onSubscribe(subscriptionContext, std::move(subscriptionOperation));
			});
}

/**
 * Create a Value Prototype for storing values returned by the given channel.
 *
 * @param dbChannelSharedPtr pointer to the channel
 * @return a value prototype for the given channel
 */
Value SingleSource::getValuePrototype(const std::shared_ptr<dbChannel>& dbChannelSharedPtr) {
	auto dbChannel(dbChannelSharedPtr.get());
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto valueType(IOCSource::getChannelValueType(dbChannelSharedPtr.get()));

	Value valuePrototype;
	// To control optional metadata set to true to include in the output
	bool display = true;
	bool control = true;
	bool valueAlarm = true;

	if (dbrType == DBR_ENUM) {
		valuePrototype = nt::NTEnum{}.create();
	} else {
		valuePrototype = nt::NTScalar{ valueType, display, control, valueAlarm }.create();
	}
	return valuePrototype;
}

/**
 * Handle the get operation
 *
 * @param channel the channel that the request comes in on
 * @param getOperation the current executing operation
 * @param valuePrototype a value prototype that is made based on the expected type to be returned
 */
void SingleSource::get(dbChannel* channel, std::unique_ptr<server::ExecOp>& getOperation,
		const Value& valuePrototype) {
	try {
		IOCSource::get(channel, valuePrototype.cloneEmpty(), true, true, [&getOperation](Value& value) {
			getOperation->reply(value);
		});
	} catch (const std::exception& getException) {
		getOperation->error(getException.what());
	}
}

/**
 * Called when a client pauses / stops a subscription it has been subscribed to
 *
 * @param subscriptionContext the subscription context
s */
void SingleSource::onDisableSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext) {
	db_event_disable(subscriptionContext->pValueEventSubscription.get());
	db_event_disable(subscriptionContext->pPropertiesEventSubscription.get());
}

/**
 * Handler for the onOp event raised by pvxs Sources when they are started, in order to define the get and put handlers
 * on a per source basis.
 * This is called after the event has been intercepted and we add the channel and value prototype to the call.
 *
 * @param dbChannelSharedPtr the channel to which the get/put operation pertains
 * @param valuePrototype the value prototype that is appropriate for the given channel
 * @param channelConnectOperation the channel connect operation object
 */
void SingleSource::onOp(const std::shared_ptr<dbChannel>& dbChannelSharedPtr, const Value& valuePrototype,
		std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
	// Announce the channel type with a `connect()` call.  This happens only once
	channelConnectOperation->connect(valuePrototype);

	// Set up handler for get requests
	channelConnectOperation
			->onGet([dbChannelSharedPtr, valuePrototype](std::unique_ptr<server::ExecOp>&& getOperation) {
				get(dbChannelSharedPtr.get(), getOperation, valuePrototype);
			});

	// Set up handler for put requests
	channelConnectOperation
			->onPut([dbChannelSharedPtr, valuePrototype](std::unique_ptr<server::ExecOp>&& putOperation,
					Value&& value) {
				try {
					Credentials credentials(*putOperation->credentials());

					auto pDbChannel = dbChannelSharedPtr.get();
					IOCSource::doPreProcessing(pDbChannel, credentials); // pre-process
					IOCSource::doFieldPreProcessing(pDbChannel, credentials); // pre-process field
					if (dbChannelFieldType(pDbChannel) >= DBF_INLINK && dbChannelFieldType(pDbChannel) <= DBF_FWDLINK) {
						IOCSource::put(pDbChannel, value); // put
					} else {
						DBLocker F(pDbChannel->addr.precord); // lock
						IOCSource::put(pDbChannel, value); // put
						IOCSource::doPostProcessing(pDbChannel); // post-process
					}
					putOperation->reply();
				} catch (std::exception& e) {
					putOperation->error(e.what());
				}
			});
}

/**
 * Called by the framework when the monitoring client issues a start or stop subscription
 *
 * @param subscriptionContext the subscription context
 * @param isStarting true if the client issued a start subscription request, false otherwise
 */
void SingleSource::onStart(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext, bool isStarting) {
	if (isStarting) {
		onStartSubscription(subscriptionContext);
	} else {
		onDisableSubscription(subscriptionContext);
	}
}

/**
 * Called when a client starts a subscription it has subscribed to
 *
 * @param subscriptionContext the subscription context
 */
void SingleSource::onStartSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext) {
	db_event_enable(subscriptionContext->pValueEventSubscription.get());
	db_event_enable(subscriptionContext->pPropertiesEventSubscription.get());
	db_post_single_event(subscriptionContext->pValueEventSubscription.get());
	db_post_single_event(subscriptionContext->pPropertiesEventSubscription.get());
}

/**
 * Called by the framework when a client subscribes to a channel.  We intercept the call before this function is called
 * to add a new subscription context with a value prototype matching the channel definition.
 *
 * @param subscriptionContext a new subscription context with a value prototype matching the channel
 * @param subscriptionOperation the channel subscription operation
 */
void SingleSource::onSubscribe(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext,
		std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) const {
	// inform peer of data type and acquire control of the subscription queue
	subscriptionContext->subscriptionControl = subscriptionOperation->connect(subscriptionContext->prototype);

	// Two subscription are made for pvxs
	// first subscription is for Value changes
	addSubscriptionEvent(Value, eventContext, subscriptionContext, DBE_VALUE | DBE_ALARM);
	// second subscription is for Property changes
	addSubscriptionEvent(Properties, eventContext, subscriptionContext, DBE_PROPERTY);

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
 * Used by both value and property subscriptions, this function will get and return the database value to the monitor.
 *
 * @param subscriptionContext the subscription context
 * @param pTriggeringDbChannel the channel that triggered this subscription update
 * @param eventsRemaining the number of events remaining
 * @param pDbFieldLog the database field log
 */
void SingleSource::subscriptionCallback(SingleSourceSubscriptionCtx* subscriptionContext,
		struct dbChannel* pTriggeringDbChannel, struct db_field_log* pDbFieldLog) {
	// Make sure that the initial subscription update has occurred on both channels before continuing
	// As we make two initial updates when opening a new subscription, we need both to have completed before continuing
	if (!subscriptionContext->hadValueEvent || !subscriptionContext->hadPropertyEvent) {
		return;
	}

	// Get and return the value to the monitor
	bool forValue = (subscriptionContext->pValueChannel.get() == pTriggeringDbChannel);
	auto& pDbChannel = forValue ? subscriptionContext->pValueChannel : subscriptionContext->pPropertiesChannel;
	IOCSource::get(pDbChannel.get(), subscriptionContext->prototype.cloneEmpty(), forValue, !forValue,
			[subscriptionContext](Value& value) {
				// Return value
				subscriptionContext->subscriptionControl->tryPost(value);
			}, pDbFieldLog);
}

/**
 * This callback handles notifying of updates to subscribed-to pv properties.  The macro addSubscriptionEvent(...)
 * creates the call to this function, so your IDE may mark it as unused (don't believe it :) )
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param pDbChannel pointer to the channel whose properties have been updated
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void SingleSource::subscriptionPropertiesCallback(void* userArg, struct dbChannel* pDbChannel, int,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (SingleSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadPropertyEvent = true;
	}
	subscriptionCallback(subscriptionContext, pDbChannel, pDbFieldLog);
}

/**
 * This callback handles notifying of updates to subscribed-to pv values.  The macro addSubscriptionEvent(...)
 * creates the call to this function, so your IDE may mark it as unused (don't believe it :) )
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param pDbChannel pointer to the channel whose value has been updated
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void SingleSource::subscriptionValueCallback(void* userArg, struct dbChannel* pDbChannel, int,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (SingleSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadValueEvent = true;
	}
	subscriptionCallback(subscriptionContext, pDbChannel, pDbFieldLog);
}

} // ioc
} // pvxs
