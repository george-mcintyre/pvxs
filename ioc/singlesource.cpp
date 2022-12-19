/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

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

DEFINE_LOGGER(_logname, "pvxs.ioc.search");

/**
 * Constructor for SingleSource registrar.
 * For each record type and for each record in that type, add record name to the list of all records
 */
SingleSource::SingleSource() {
	auto names(std::make_shared<std::set<std::string >>());

	DBEntry db;
	for (long status = dbFirstRecordType(db); !status; status = dbNextRecordType(db)) {
		for (status = dbFirstRecord(db); !status; status = dbNextRecord(db)) {
			names->insert(db->precnode->recordname);
		}
	}

	allrecords.names = names;
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
	return allrecords;
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
	for (auto& name: *SingleSource::allrecords.names) {
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
	channelControl
			->onSubscribe([pChannel, valuePrototype](std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
				auto subscriptionControl(subscriptionOperation->connect(valuePrototype));

				// get event control mask
				db_field_log eventControlMask;

//				std::map<epicsEventId, std::vector<dbEventSubscription>> channelMonitor = subscriptions[pChannel];

//				void* eventSubscription = db_add_event(eventContext, pChannel, &subscriptionCallback, channelMonitor, eventControlMask);
				void* eventSubscription(nullptr);

				subscriptionControl->onStart([pChannel](bool isStarting) {
					if (isStarting) {
						onStartSubscription(pChannel);
					} else {
						onDisableSubscription(pChannel);
					}
				});
			});
}

/**
 * Called when a client starts a subscription it has subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void SingleSource::onStartSubscription(const std::shared_ptr<dbChannel>& pChannel) {
	// db_event_enable()
	// db_post_single_event()
}

/**
 * Called when a client pauses / stops a subscription it has been subscribed to
 *
 * @param pChannel the pointer to the database channel subscribed to
 */
void SingleSource::onDisableSubscription(const std::shared_ptr<dbChannel>& pChannel) {
	// db_event_disable()
}

/*
void SingleSource::subscriptionCallback(void* user_arg, const std::shared_ptr<dbChannel>& pChannel,
		int eventsRemaining, struct db_field_log* pfl) {
	epicsMutexMustLock(eventLock);
	std::map<epicsEventId, std::vector<dbEventSubscription>> channelMonitor = subscriptions[pChannel];
	epicsMutexUnlock(eventLock);
//	epicsEventMustTrigger(channelMonitor[user_arg]);
}
*/

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

/**
 * Handle the get operation
 *
 * @param channel the channel that the request comes in on
 * @param getOperation the current executing operation
 * @param valuePrototype a value prototype that is made based on the expected type to be returned
 */
void SingleSource::onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
		const Value& valuePrototype) {
	const char* pvName = channel->name;

	epicsAny valueBuffer[100]; // value buffer to store the field we will get from the database.
	void* pValueBuffer = &valueBuffer[0];
	DBADDR dbAddress;   // Special struct for storing database addresses
	long nElements;     // Calculated number of elements to retrieve.  For single values its 1 but for arrays it can be any number

	// Convert pvName to a dbAddress
	if (DBErrMsg err = nameToAddr(pvName, &dbAddress)) {
		getOperation->error(err.c_str());
		return;
	}

	if (dbAddress.precord->lset == nullptr) {
		getOperation->error("pvName no specified in request");
		return;
	}

	// Calculate number of elements to retrieve as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	nElements = MIN(dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

	// Lock record
	dbCommon* pRecord = dbAddress.precord;
	dbScanLock(pRecord);

	// Get field value and all metadata
	// Note that metadata will precede the value in the buffer and will be laid out in the order of the
	// options bit mask LSB to MSB
	long options = IOC_OPTIONS;

	if (DBErrMsg err = dbGet(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, &options, &nElements,
			nullptr)) {
		getOperation->error(err.c_str());
		return;
	}

	// Unlock record
	dbScanUnlock(pRecord);

	// Extract metadata
	Metadata metadata;
	getMetadata(pValueBuffer, metadata);

	// read out the value from the buffer
	// Create a placeholder for the return value
	auto value(valuePrototype.cloneEmpty());
	// based on whether it is an enum, scalar or array then call the appropriate setter
	if (dbAddress.dbr_field_type == DBR_ENUM) {
		if (value["value.index"]) {
			value["value.index"] = *((uint16_t*)pValueBuffer);

			shared_array<std::string> choices(metadata.enumStrings->no_str);
			for (auto i = 0; i < metadata.enumStrings->no_str; i++) {
				choices[i] = metadata.enumStrings->strs[i];
			}
			value["value.choices"] = choices.freeze().castTo<const void>();
		} else {
			getOperation->error("Programming error: Database enum record but not NTEnum response");
			return;
		}
	} else if (nElements == 1) {
		setValue(value, pValueBuffer);
	} else {
		setValue(value, pValueBuffer, nElements);
	}

	// Add metadata to response
	setAlarmMetadata(metadata, value);
	setTimestampMetadata(metadata, value);
	setDisplayMetadata(metadata, value);
	setControlMetadata(metadata, value);
	setAlarmLimitMetadata(metadata, value);

	// Send reply
	getOperation->reply(value);
}

/**
 * Get metadata from the given value buffer and deliver it in the given metadata buffer
 *
 * @param pValueBuffer The given value buffer
 * @param metadata to store the metadata
 */
void SingleSource::getMetadata(void*& pValueBuffer, Metadata& metadata) {

	// Read out the metadata from the buffer
	// Alarm Status
	auto* pStatus = (uint16_t*)pValueBuffer;
	metadata.metadata.stat = *pStatus++;
	metadata.metadata.sevr = *pStatus++;
	metadata.metadata.acks = *pStatus++;
	metadata.metadata.ackt = *pStatus++;
	pValueBuffer = (char*)pStatus;

	// Alarm message
	strcpy(metadata.metadata.amsg, (const char*)pValueBuffer);
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[sizeof(metadata.metadata.amsg)]);

	// Alarm message
	metadata.pUnits = (const char*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[DB_UNITS_SIZE]);

	// Precision
	metadata.pPrecision = (const dbr_precision*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[dbr_precision_size]);

	// Time
	auto* pTime = (uint32_t*)pValueBuffer;
	metadata.metadata.time.secPastEpoch = *pTime++;
	metadata.metadata.time.nsec = *pTime++;
	pValueBuffer = (char*)pTime;

	// Utag
	auto* pUTag = (uint64_t*)pValueBuffer;
	metadata.metadata.utag = *pUTag++;
	pValueBuffer = (char*)pUTag;

	// Enum strings
	metadata.enumStrings = (const dbr_enumStrs*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[dbr_enumStrs_size]);

	// Graphics
	metadata.graphicsDouble = (const struct dbr_grDouble*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[dbr_grDouble_size]);

	// Control
	metadata.controlDouble = (const struct dbr_ctrlDouble*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[dbr_ctrlDouble_size]);

	// Alarm
	metadata.alarmDouble = (const struct dbr_alDouble*)pValueBuffer;
	pValueBuffer = ((void*)&((const char*)pValueBuffer)[dbr_alDouble_size]);
}

void SingleSource::onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
		const Value& valuePrototype, const Value& value) {
	// dbPutField() ...
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
 * Set a value from the given database value
 *
 * @param value the value to set
 * @param pValueBuffer pointer to the database value
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
 * Set a value from the given database value buffer.  This is the array version of the function
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

/**
 * Set alarm metadata in the given value.
 *
 * @param metadata metadata containing alarm metadata
 * @param value the value to set metadata for
 */
void SingleSource::setAlarmMetadata(Metadata& metadata, Value& value) {
	checkedSetField(metadata.metadata.stat, alarm.status);
	checkedSetField(metadata.metadata.sevr, alarm.severity);
	checkedSetField(metadata.metadata.acks, alarm.acks);
	checkedSetField(metadata.metadata.ackt, alarm.ackt);
	checkedSetStringField(metadata.metadata.amsg, alarm.message);
}

/**
 * Set timestamp metadata in the given value.
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
 * Set display metadata in the given value.
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
 * Set control metadata in the given value.
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
 * Set alarm limit metadata in the given value.
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
