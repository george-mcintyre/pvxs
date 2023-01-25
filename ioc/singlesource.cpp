/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cstring>
#include <algorithm>
#include <iostream>

#include <pvxs/source.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>

#include <epicsTypes.h>
#include <dbStaticLib.h>
#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <cmath>

#include "dbentry.h"
#include "dberrmsg.h"
#include "singlesource.h"
#include "typeutils.h"
#include "iocsource.h"

#include "singlesrcsubscriptionctx.h"

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
	dbChannel* pdbChannel = dbChannelCreate(sourceName);
	if (!pdbChannel) {
		log_debug_printf(_logname, "Ignore requested source '%s'\n", sourceName);
		return;
	}
	log_debug_printf(_logname, "Accepting channel for '%s'\n", sourceName);

	// Set up a shared pointer to the database channel and provide a deleter lambda for when it will eventually be deleted
	std::shared_ptr<dbChannel> pChannel(pdbChannel, [](dbChannel* ch) { dbChannelDelete(ch); });

	if (DBErrMsg err = dbChannelOpen(pChannel.get())) {
		log_debug_printf(_logname, "Error opening database channel for '%s: %s'\n", sourceName, err.c_str());
		throw std::runtime_error(err.c_str());
	}

	// Create callbacks for handling requests and channel subscriptions
	createRequestAndSubscriptionHandlers(channelControl, pChannel);
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
 * @param pChannel the pointer to the database channel to set up the handlers for
 */
void SingleSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl,
		const std::shared_ptr<dbChannel>& pChannel) {
	auto subscriptionContext(std::make_shared<SingleSourceSubscriptionCtx>(pChannel));

	auto dbChannel(pChannel.get());
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto valueType(IOCSource::getChannelValueType(pChannel.get()));

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

	// Get and Put requests
	channelControl->onOp([pChannel, valuePrototype](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
		// First stage for handling any request is to announce the channel type with a `connect()` call
		// @note The type signalled here must match the eventual type returned by a pvxs get
		channelConnectOperation->connect(valuePrototype);

		// pvxs get
		channelConnectOperation->onGet([pChannel, valuePrototype](std::unique_ptr<server::ExecOp>&& getOperation) {
			onGet(pChannel, getOperation, valuePrototype);
		});

		// pvxs put
		channelConnectOperation
				->onPut([pChannel, valuePrototype](std::unique_ptr<server::ExecOp>&& putOperation, Value&& value) {
					onPut(pChannel, putOperation, valuePrototype, value);
				});
	});

	// Subscription requests
	// Shared ptr for one of captured vars below
	subscriptionContext->prototype = valuePrototype.cloneEmpty();
	channelControl
			->onSubscribe([this, subscriptionContext](
					std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
				subscriptionContext->subscriptionControl = subscriptionOperation
						->connect(subscriptionContext->prototype);

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
				subscriptionContext->subscriptionControl->onStart([subscriptionContext](bool isStarting) {
					if (isStarting) {
						onStartSubscription(subscriptionContext);
					} else {
						onDisableSubscription(subscriptionContext);
					}
				});
			});
}

/**
 * This callback handles notifying of updates to subscribed-to pv values
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param pChannel pointer to the channel whose value has been updated
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void SingleSource::subscriptionValueCallback(void* userArg, struct dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (SingleSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadValueEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog);
}

/**
 * This callback handles notifying of updates to subscribed-to pv properties
 *
 * @param userArg the user argument passed to the callback function from the framework: the subscriptionContext
 * @param pChannel pointer to the channel whose properties have been updated
 * @param eventsRemaining the remaining number of events to process
 * @param pDbFieldLog the database field log containing the changes to notify
 */
void SingleSource::subscriptionPropertiesCallback(void* userArg, struct dbChannel* pChannel, int eventsRemaining,
		struct db_field_log* pDbFieldLog) {
	auto subscriptionContext = (SingleSourceSubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadPropertyEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog);
}

void SingleSource::subscriptionCallback(SingleSourceSubscriptionCtx* subscriptionContext, struct dbChannel* pChannel,
		int eventsRemaining, struct db_field_log* pDbFieldLog) {
	// Make sure that the initial subscription update has occurred on both channels before continuing
	if (!subscriptionContext->hadValueEvent || !subscriptionContext->hadPropertyEvent) {
		return;
	}

	// Get and return the value to the monitor
	bool forValue = (subscriptionContext->pValueChannel.get() == pChannel);
	auto pdbChannel = forValue ? subscriptionContext->pValueChannel : subscriptionContext->pPropertiesChannel;
	IOCSource::onGet(pdbChannel, subscriptionContext->prototype, forValue, !forValue, [subscriptionContext](Value& value) {
		// Return value
		subscriptionContext->subscriptionControl->tryPost(value);
	}, [](const char* errorMessage) {
		throw std::runtime_error(errorMessage);
	});
}

/**
 * Called when a client starts a subscription it has subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void SingleSource::onStartSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext) {
	db_event_enable(subscriptionContext->pValueEventSubscription.get());
	db_event_enable(subscriptionContext->pPropertiesEventSubscription.get());
	db_post_single_event(subscriptionContext->pValueEventSubscription.get());
	db_post_single_event(subscriptionContext->pPropertiesEventSubscription.get());
}

/**
 * Called when a client pauses / stops a subscription it has been subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void SingleSource::onDisableSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext) {
	db_event_disable(subscriptionContext->pValueEventSubscription.get());
	db_event_disable(subscriptionContext->pPropertiesEventSubscription.get());
}

/**
 * Handle the get operation
 *
 * @param channel the channel that the request comes in on
 * @param getOperation the current executing operation
 * @param valuePrototype a value prototype that is made based on the expected type to be returned
 */
void SingleSource::onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
		const Value& valuePrototype) {
	IOCSource::onGet(channel, valuePrototype, true, true, [&getOperation](Value& value) {
		getOperation->reply(value);
	}, [&getOperation](const char* errorMessage) {
		getOperation->error(errorMessage);
	});
}


/**
 * Handler invoked when a peer sends data on a PUT
 *
 * @param channel
 * @param putOperation
 * @param valuePrototype
 * @param value
 */
void SingleSource::onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
		const Value& valuePrototype, const Value& value) {
	const char* pvName = channel->name;

	epicsAny valueBuffer[100]; // value buffer to store the field we will get from the database.
	void* pValueBuffer = &valueBuffer[0];
	DBADDR dbAddress;   // Special struct for storing database addresses
	long nElements;     // number of elements - 1 for scalar or enum, more for arrays

	// Convert pvName to a dbAddress
	if (DBErrMsg err = IOCSource::nameToAddr(pvName, &dbAddress)) {
		putOperation->error(err.c_str());
		return;
	}

	if (dbAddress.precord->lset == nullptr) {
		putOperation->error("pvName not specified in request");
		return;
	}

	// Calculate number of elements to save as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	nElements = MIN(dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

	if (dbAddress.dbr_field_type == DBR_ENUM) {
		*(uint16_t*)(pValueBuffer) = (value)["value.index"].as<uint16_t>();
	} else if (nElements == 1) {
		setBuffer(value, pValueBuffer);
	} else {
		setBuffer(value, pValueBuffer, nElements);
	}

	if (DBErrMsg err = dbPutField(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, nElements)) {
		putOperation->error(err.c_str());
		return;
	}
	putOperation->reply();
}

/**
 * Get value into given database buffer
 *
 * @param value the value to get
 * @param pValueBuffer the database buffer to put it in
 */
void SingleSource::setBuffer(const Value& value, void* pValueBuffer) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		auto strValue = value["value"].as<std::string>();
		auto len = MIN(strValue.length(), MAX_STRING_SIZE - 1);

		value["value"].as<std::string>().copy((char*)pValueBuffer, len);
		((char*)pValueBuffer)[len] = '\0';
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setBuffer, (value, pValueBuffer));
	}
}

void SingleSource::setBuffer(const Value& value, void* pValueBuffer, long nElements) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::StringA) {
		char valueRef[20];
		for (auto i = 0; i < nElements; i++) {
			snprintf(valueRef, 20, "value[%d]", i);
			auto strValue = value[valueRef].as<std::string>();
			auto len = MIN(strValue.length(), MAX_STRING_SIZE - 1);
			strValue.copy((char*)pValueBuffer + MAX_STRING_SIZE * i, len);
			((char*)pValueBuffer + MAX_STRING_SIZE * i)[len] = '\0';
		}
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setBuffer, (value, pValueBuffer, nElements));
	}
}

// Get the value into the given database value buffer (templated)
template<typename valueType> void SingleSource::setBuffer(const Value& value, void* pValueBuffer) {
	((valueType*)pValueBuffer)[0] = value["value"].as<valueType>();
}

// Get the value into the given database value buffer (templated)
template<typename valueType> void SingleSource::setBuffer(const Value& value, void* pValueBuffer, long nElements) {
	char valueRef[20];
	// auto s = value.as<shared_array<const valueType>>();
	for (auto i = 0; i < nElements; i++) {
		snprintf(valueRef, 20, "value[%d]", i);
		((valueType*)pValueBuffer)[i] = value[valueRef].as<valueType>();
	}
}


} // ioc
} // pvxs
