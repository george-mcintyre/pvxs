/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>

#include "iocsource.h"
#include "dbentry.h"
#include "dberrmsg.h"
#include "typeutils.h"

namespace pvxs {
namespace ioc {

void IOCSource::onGet(const std::shared_ptr<dbChannel>& channel,
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
	auto nElements = (long)std::min((size_t)dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

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
 * Utility function to get the TypeCode that the given database channel is configured for
 *
 * @param pChannel the pointer to the database channel to get the TypeCode for
 * @return the TypeCode that the channel is configured for
 */
TypeCode IOCSource::getChannelValueType(const dbChannel* pChannel, bool errOnLinks) {
	auto dbChannel(pChannel);
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto nFinalElements(dbChannelFinalElements(dbChannel));
	auto nElements(dbChannelElements(dbChannel));

	TypeCode valueType;

	if (dbChannelFieldType(dbChannel) == DBF_STRING && nElements == 1 && dbrType && nFinalElements > 1) {
		// single character long DBF_STRING being cast to DBF_CHAR array.
		valueType = TypeCode::String;

	} else {
		if (dbrType == DBF_INLINK || dbrType == DBF_OUTLINK || dbrType == DBF_FWDLINK) {
			if ( errOnLinks) {
				throw std::runtime_error("Link fields not allowed in this context");
			} else {
				// TODO handle links.  For the moment we handle as chars and fail later
				dbrType = DBF_CHAR;
			}
		}

		valueType = toTypeCode(dbfType(dbrType));
		if (valueType != TypeCode::Null && nFinalElements != 1) {
			valueType = valueType.arrayOf();
		}
	}
	return valueType;
}

/**
 * Get metadata from the given value buffer and deliver it in the given metadata buffer
 *
 * @param pValueBuffer The given value buffer
 * @param metadata to store the metadata
 */
void IOCSource::getMetadata(void*& pValueBuffer, Metadata& metadata, bool forValues, bool forProperties) {
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
 * Set a return value from the given database value buffer
 *
 * @param value the value to set
 * @param pValueBuffer pointer to the database value buffer
 */
void IOCSource::setValue(Value& value, void* pValueBuffer) {
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
void IOCSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		value["value"] = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setValue, (value, pValueBuffer, nElements));
	}
}

/**
 * Set alarm metadata in the given return value.
 *
 * @param metadata metadata containing alarm metadata
 * @param value the value to set metadata for
 */
void IOCSource::setAlarmMetadata(Metadata& metadata, Value& value) {
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
void IOCSource::setTimestampMetadata(Metadata& metadata, Value& value) {
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
void IOCSource::setDisplayMetadata(Metadata& metadata, Value& value) {
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
void IOCSource::setControlMetadata(const Metadata& metadata, Value& value) {
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
void IOCSource::setAlarmLimitMetadata(const Metadata& metadata, Value& value) {
	if (value["valueAlarm"]) {
		checkedSetDoubleField(metadata.alarmDouble->lower_alarm_limit, valueAlarm.lowAlarmLimit);
		checkedSetDoubleField(metadata.alarmDouble->lower_warning_limit, valueAlarm.lowWarningLimit);
		checkedSetDoubleField(metadata.alarmDouble->upper_warning_limit, valueAlarm.highWarningLimit);
		checkedSetDoubleField(metadata.alarmDouble->upper_alarm_limit, valueAlarm.highAlarmLimit);
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
template<typename valueType> void IOCSource::setValue(Value& value, void* pValueBuffer) {
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
template<typename valueType>
void IOCSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	shared_array<valueType> values(nElements);
	for (auto i = 0; i < nElements; i++) {
		values[i] = ((valueType*)pValueBuffer)[i];
	}
	value["value"] = values.freeze().template castTo<const void>();
}

/**
 * Utility function to get the corresponding database address structure given a pvName
 *
 * @param pvName the pvName
 * @param pdbAddress pointer to the database address structure
 * @return status that can be decoded with DBErrMsg - 0 means success
 */
long IOCSource::nameToAddr(const char* pvName, DBADDR* pdbAddress) {
	long status = dbNameToAddr(pvName, pdbAddress);

	if (status) {
		printf("PV '%s' not found\n", pvName);
	}
	return status;
}

} // pvxs
} // ioc
