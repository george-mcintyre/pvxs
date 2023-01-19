/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string.h>
#include <algorithm>
#include <vector>
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
	auto subscriptionContext(std::make_shared<SubscriptionCtx>());
	subscriptionContext->pValueChannel = pChannel;
	subscriptionContext->pPropertiesChannel.reset(dbChannelCreate(dbChannelName(pChannel)), [](dbChannel* ch) {
		if (ch) dbChannelDelete(ch);
	});
	if (subscriptionContext->pPropertiesChannel && dbChannelOpen(subscriptionContext->pPropertiesChannel.get())) {
		throw std::bad_alloc();
	}

	auto dbChannel(pChannel.get());
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto valueType(getChannelValueType(pChannel));

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
	auto subscriptionContext = (SubscriptionCtx*)userArg;
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
	auto subscriptionContext = (SubscriptionCtx*)userArg;
	{
		epicsGuard<epicsMutex> G((subscriptionContext)->eventLock);
		subscriptionContext->hadPropertyEvent = true;
	}
	subscriptionCallback(subscriptionContext, pChannel, eventsRemaining, pDbFieldLog);
}

void SingleSource::subscriptionCallback(SubscriptionCtx* subscriptionContext, struct dbChannel* pChannel,
		int eventsRemaining, struct db_field_log* pDbFieldLog) {
	// Make sure that the initial subscription update has occurred on both channels before continuing
	if (!subscriptionContext->hadValueEvent || !subscriptionContext->hadPropertyEvent) {
		return;
	}

	// Get and return the value to the monitor
	bool forValue = (subscriptionContext->pValueChannel.get() == pChannel);
	auto pdbChannel = forValue ? subscriptionContext->pValueChannel : subscriptionContext->pPropertiesChannel;
	onGet(pdbChannel, subscriptionContext->prototype, forValue, !forValue, [subscriptionContext](Value& value) {
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
void SingleSource::onStartSubscription(const std::shared_ptr<subscriptionCtx>& subscriptionContext) {
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
void SingleSource::onDisableSubscription(const std::shared_ptr<subscriptionCtx>& subscriptionContext) {
	db_event_disable(subscriptionContext->pValueEventSubscription.get());
	db_event_disable(subscriptionContext->pPropertiesEventSubscription.get());
}

/**
 * Utility function to get the TypeCode that the given database channel is configured for
 *
 * @param pChannel the pointer to the database channel to get the TypeCode for
 * @return the TypeCode that the channel is configured for
 */
TypeCode SingleSource::getChannelValueType(const std::shared_ptr<dbChannel>& pChannel) {
	auto dbChannel(pChannel.get());
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto nFinalElements(dbChannelFinalElements(dbChannel));
	auto nElements(dbChannelElements(dbChannel));

	TypeCode valueType;

	if (dbChannelFieldType(dbChannel) == DBF_STRING && nElements == 1 && dbrType && nFinalElements > 1) {
		// single character long DBF_STRING being cast to DBF_CHAR array.
		valueType = TypeCode::String;

	} else if (dbrType == DBF_ENUM && nFinalElements == 1) {
		valueType = TypeCode::UInt16;   // value field is

	} else {
		// TODO handle links.  For the moment we handle as chars and fail later
		if (dbrType == DBF_INLINK || dbrType == DBF_OUTLINK || dbrType == DBF_FWDLINK) dbrType = DBF_CHAR;

		valueType = toTypeCode(dbfType(dbrType));
		if (valueType != TypeCode::Null && nFinalElements != 1) {
			valueType = valueType.arrayOf();
		}
	}
	return valueType;
}

void SingleSource::onGet(const std::shared_ptr<dbChannel>& channel,
		const Value& valuePrototype, bool forValues, bool forProperties,
		const std::function<void(Value&)>& returnFn, const std::function<void(const char*)>& errorFn) {
	const char* pvName = channel->name;

	epicsAny valueBuffer[100]; // value buffer to store the field we will get from the database.
	void* pValueBuffer = &valueBuffer[0];
	DBADDR dbAddress;   // Special struct for storing database addresses

	// Convert pvName to a dbAddress
	if (DBErrMsg err = nameToAddr(pvName, &dbAddress)) {
		errorFn(err.c_str());
		return;
	}

	if (dbAddress.precord->lset == nullptr) {
		errorFn("pvName not specified in request");
		return;
	}

	// Calculate number of elements to retrieve as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	auto nElements = (long )std::min((size_t)dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

	// Get field value and all metadata
	// Note that metadata will precede the value in the buffer and will be laid out in the order of the
	// options bit mask LSB to MSB
	long options = 0;
	if (forValues) {
		options = IOC_VALUE_OPTIONS;
	}
	if (forProperties) {
		options |= IOC_PROPERTIES_OPTIONS;
	}

	if (!options) {
		throw std::runtime_error("call to get but neither values not properties requested");
	}

	if (DBErrMsg err = dbGetField(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, &options, &nElements,
			nullptr)) {
		errorFn(err.c_str());
		return;
	}

	// Extract metadata
	Metadata metadata;
	getMetadata(pValueBuffer, metadata, forValues, forProperties);

	// read out the value from the buffer
	// Create a placeholder for the return value
	auto value(valuePrototype.cloneEmpty());

	// If we're setting values then,
	// based on whether it is an enum, scalar or array then call the appropriate setter
	if (forValues) {
		if (dbAddress.dbr_field_type == DBR_ENUM) {
			value["value.index"] = *((uint16_t*)pValueBuffer);

			// TODO Don't output choices for subscriptions unless changed
			shared_array<std::string> choices(metadata.enumStrings->no_str);
			for (epicsUInt32 i = 0; i < metadata.enumStrings->no_str; i++) {
				choices[i] = metadata.enumStrings->strs[i];
			}
			value["value.choices"] = choices.freeze().castTo<const void>();
		} else if (nElements == 1) {
			setValue(value, pValueBuffer);
		} else {
			setValue(value, pValueBuffer, nElements);
		}
	}

	// Add metadata to response
	setTimestampMetadata(metadata, value);
	if (forValues) {
		setAlarmMetadata(metadata, value);
	}
	if (forProperties) {
		setDisplayMetadata(metadata, value);
		setControlMetadata(metadata, value);
		setAlarmLimitMetadata(metadata, value);
	}

	// Send reply
	returnFn(value);
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
	onGet(channel, valuePrototype, true, true, [&getOperation](Value& value) {
		getOperation->reply(value);
	}, [&getOperation](const char* errorMessage) {
		getOperation->error(errorMessage);
	});
}

/**
 * Get metadata from the given value buffer and deliver it in the given metadata buffer
 *
 * @param pValueBuffer The given value buffer
 * @param metadata to store the metadata
 */
void SingleSource::getMetadata(void*& pValueBuffer, Metadata& metadata, bool forValues, bool forProperties) {
	// Read out the metadata from the buffer
	if (forValues) {
		// Alarm
		get4MetadataFields(pValueBuffer, uint16_t,
				metadata.metadata.status, metadata.metadata.severity,
				metadata.metadata.acks, metadata.metadata.ackt);
		// Alarm message
		getMetadataString(pValueBuffer, metadata.metadata.amsg);
	}
	if (forProperties) {
		// Units
		getMetadataBuffer(pValueBuffer, const char, metadata.pUnits, DB_UNITS_SIZE);
		// Precision
		getMetadataBuffer(pValueBuffer, const dbr_precision, metadata.pPrecision, dbr_precision_size);
	}

	// Time
	get2MetadataFields(pValueBuffer, uint32_t, metadata.metadata.time.secPastEpoch, metadata.metadata.time.nsec);
	// User tag
	getMetadataField(pValueBuffer, uint64_t, metadata.metadata.utag);

	if (forValues) {
		// Enum strings
		getMetadataBuffer(pValueBuffer, const dbr_enumStrs, metadata.enumStrings, dbr_enumStrs_size);
	}

	if (forProperties) {
		// Display
		getMetadataBuffer(pValueBuffer, const struct dbr_grDouble, metadata.graphicsDouble, dbr_grDouble_size);
		// Control
		getMetadataBuffer(pValueBuffer, const struct dbr_ctrlDouble, metadata.controlDouble, dbr_ctrlDouble_size);
		// Alarm limits
		getMetadataBuffer(pValueBuffer, const struct dbr_alDouble, metadata.alarmDouble, dbr_alDouble_size);
	}
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
	if (DBErrMsg err = nameToAddr(pvName, &dbAddress)) {
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
 * Utility function to get the corresponding database address structure given a pvName
 *
 * @param pvName the pvName
 * @param pdbAddress pointer to the database address structure
 * @return status that can be decoded with DBErrMsg - 0 means success
 */
long SingleSource::nameToAddr(const char* pvName, DBADDR* pdbAddress) {
	long status = dbNameToAddr(pvName, pdbAddress);

	if (status) {
		printf("PV '%s' not found\n", pvName);
	}
	return status;
}

/**
 * Set a return value from the given database value buffer
 *
 * @param value the value to set
 * @param pValueBuffer pointer to the database value buffer
 */
void SingleSource::setValue(Value& value, void* pValueBuffer) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		value["value"] = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setValue, (value, pValueBuffer));
	}
}

/**
 * Set a return value from the given database value buffer.  This is the array version of the function
 *
 * @param value the value to set
 * @param pValueBuffer the database value buffer
 * @param nElements the number of elements in the buffer
 */
void SingleSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		value["value"] = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setValue, (value, pValueBuffer, nElements));
	}
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
//		auto s = value.as<shared_array<const std::string>>()
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

/**
 * Set the value field of the given return value to a scalar pointed to by pValueBuffer
 * Supported types are:
 *   TypeCode::Int8 	TypeCode::UInt8
 *   TypeCode::Int16 	TypeCode::UInt16
 *   TypeCode::Int32 	TypeCode::UInt32
 *   TypeCode::Int64 	TypeCode::UInt64
 *   TypeCode::Float32 	TypeCode::Float64
 *
 * @tparam valueType the type of the scalar stored in the buffer.  One of the supported types
 * @param value the return value
 * @param pValueBuffer the pointer to the data containing the database data to store in the return value
 */
template<typename valueType> void SingleSource::setValue(Value& value, void* pValueBuffer) {
	value["value"] = ((valueType*)pValueBuffer)[0];
}

/**
 * Set the value field of the given return value to an array of scalars pointed to by pValueBuffer
 * Supported types are:
 *   TypeCode::Int8 	TypeCode::UInt8
 *   TypeCode::Int16 	TypeCode::UInt16
 *   TypeCode::Int32 	TypeCode::UInt32
 *   TypeCode::Int64 	TypeCode::UInt64
 *   TypeCode::Float32 	TypeCode::Float64
 *
 * @tparam valueType the type of the scalars stored in this array.  One of the supported types
 * @param value the return value
 * @param pValueBuffer the pointer to the data containing the database data to store in the return value
 * @param nElements the number of elements in the array
 */
template<typename valueType> void SingleSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	shared_array<valueType> values(nElements);
	for (auto i = 0; i < nElements; i++) {
		values[i] = ((valueType*)pValueBuffer)[i];
	}
	value["value"] = values.freeze().template castTo<const void>();
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

/**
 * Set alarm metadata in the given return value.
 *
 * @param metadata metadata containing alarm metadata
 * @param value the value to set metadata for
 */
void SingleSource::setAlarmMetadata(Metadata& metadata, Value& value) {
	checkedSetField(metadata.metadata.status, alarm.status);
	checkedSetField(metadata.metadata.severity, alarm.severity);
	checkedSetField(metadata.metadata.acks, alarm.acks);
	checkedSetField(metadata.metadata.ackt, alarm.ackt);
	checkedSetStringField(metadata.metadata.amsg, alarm.message);
}

/**
 * Set timestamp metadata in the given return value.
 *
 * @param metadata metadata containing timestamp metadata
 * @param value the value to set metadata for
 */
void SingleSource::setTimestampMetadata(Metadata& metadata, Value& value) {
	checkedSetField(metadata.metadata.time.secPastEpoch, timeStamp.secondsPastEpoch);
	checkedSetField(metadata.metadata.time.nsec, timeStamp.nanoseconds);
	checkedSetField(metadata.metadata.utag, timeStamp.userTag);
}

/**
 * Set display metadata in the given return value.
 * Only set the metadata if the display field is included in value's structure
 *
 * @param metadata metadata containing display metadata
 * @param value the value to set metadata for
 */
void SingleSource::setDisplayMetadata(Metadata& metadata, Value& value) {
	if (value["display"]) {
		checkedSetDoubleField(metadata.graphicsDouble->lower_disp_limit, display.limitLow);
		checkedSetDoubleField(metadata.graphicsDouble->upper_disp_limit, display.limitHigh);
		checkedSetStringField(metadata.pUnits, display.units);
		// TODO This is never present, Why?
		checkedSetField(metadata.pPrecision->precision.dp, display.precision);
	}
}

/**
 * Set control metadata in the given return value.
 * Only set the metadata if the control field is included in value's structure
 *
 * @param metadata metadata containing control metadata
 * @param value the value to set metadata for
 */
void SingleSource::setControlMetadata(const Metadata& metadata, Value& value) {
	if (value["control"]) {
		checkedSetDoubleField(metadata.controlDouble->lower_ctrl_limit, control.limitLow);
		checkedSetDoubleField(metadata.controlDouble->upper_ctrl_limit, control.limitHigh);
	}
}

/**
 * Set alarm limit metadata in the given return value.
 * Only set the metadata if the valueAlarm field is included in value's structure
 *
 * @param metadata metadata containing alarm limit metadata
 * @param value the value to set metadata for
 */
void SingleSource::setAlarmLimitMetadata(const Metadata& metadata, Value& value) {
	if (value["valueAlarm"]) {
		checkedSetDoubleField(metadata.alarmDouble->lower_alarm_limit, valueAlarm.lowAlarmLimit);
		checkedSetDoubleField(metadata.alarmDouble->lower_warning_limit, valueAlarm.lowWarningLimit);
		checkedSetDoubleField(metadata.alarmDouble->upper_warning_limit, valueAlarm.highWarningLimit);
		checkedSetDoubleField(metadata.alarmDouble->upper_alarm_limit, valueAlarm.highAlarmLimit);
	}
}

} // ioc
} // pvxs
