/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>

#include "iocsource.h"
#include "dbentry.h"
#include "dberrormessage.h"
#include "typeutils.h"

namespace pvxs {
namespace ioc {

/**
 * IOC function that will get data from the database.  This will use the provided value prototype to determine the shape
 * of the data to be returned (if it has a `value` subfield, it is a structure).  The provided channel
 * is used to retrieve the data and the flags forValues and forProperties are used to determine whether to fetch
 * values, properties or both from the database.
 *
 * When the data has been retrieved the provided returnFn is called with the value, otherwise the errorFn is called
 * with the error text.
 *
 * @param channel the channel to get the data from
 * @param valuePrototype the value prototype to use to determine the shape of the data
 * @param forValues the flag to denote that value is to be retrieved from the database
 * @param forProperties the flag to denote that properties are to be retrieved from the database
 * @param returnFn the function to call when the data has been retrieved
 * @param errorFn the function to call on errors
 */
void IOCSource::get(const std::shared_ptr<dbChannel>& channel,
		const Value& valuePrototype, const bool forValues, const bool forProperties,
		const std::function<void(Value&)>& returnFn, const std::function<void(const char*)>& errorFn) {
	// value buffer to store the field we will get from the database including metadata.
	epicsAny valueBuffer[channel->addr.no_elements * channel->addr.field_size + MAX_METADATA_SIZE];
	void* pValueBuffer = &valueBuffer[0];
	auto nElements = (long)channel->addr.no_elements;

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

	DBErrorMessage dbErrorMessage(dbGetField(&channel->addr, channel->addr.dbr_field_type,
			pValueBuffer, &options, &nElements, nullptr));
	if (dbErrorMessage) {
		errorFn(dbErrorMessage.c_str());
		return;
	}

	// Extract metadata
	Metadata metadata;
	getMetadata(pValueBuffer, metadata, forValues, forProperties);

	// read out the value from the buffer
	// Create a placeholder for the return value
	auto value(valuePrototype);

	// If we're setting values then,
	// based on whether it is an enum, scalar or array then call the appropriate setter
	auto isCompound = value["value"].valid();
	Value valueTarget = value;
	if (isCompound) {
		valueTarget = value["value"];
	}

	if (forValues) {
		auto isEnum = channel->addr.dbr_field_type == DBR_ENUM;
		if (isEnum) {
			valueTarget["index"] = *((uint16_t*)pValueBuffer);

			// TODO Don't output choices for subscriptions unless changed
			shared_array<std::string> choices(metadata.enumStrings->no_str);
			for (epicsUInt32 i = 0; i < metadata.enumStrings->no_str; i++) {
				choices[i] = metadata.enumStrings->strs[i];
			}
			valueTarget["choices"] = choices.freeze().castTo<const void>();
		} else if (nElements == 1) {
			getValue(valueTarget, pValueBuffer);
		} else {
			getValue(valueTarget, pValueBuffer, nElements);
		}
	}

	if (isCompound) {
		// Add metadata to response
		getTimestampMetadata(value, metadata);
		if (forValues) {
			getAlarmMetadata(value, metadata);
		}
		if (forProperties) {
			getDisplayMetadata(value, metadata);
			getControlMetadata(value, metadata);
			getAlarmLimitMetadata(value, metadata);
		}
	}

	// Send reply
	returnFn(value);
}

/**
 * Put a given value to the specified channel.  Throw an exception if there are any errors.
 *
 * @param channel
 * @param value
 */
void IOCSource::put(const std::shared_ptr<dbChannel>& channel, const Value& value) {
	// value buffer to store the field we will get from the database including metadata.
	epicsAny valueBuffer[channel->addr.no_elements * channel->addr.field_size + MAX_METADATA_SIZE];
	void* pValueBuffer = &valueBuffer[0];
	long nElements;     // number of elements - 1 for scalar or enum, more for arrays

	// Calculate number of elements to save as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	nElements = MIN(channel->addr.no_elements, sizeof(valueBuffer) / channel->addr.field_size);

	auto isCompound = value["value"].valid();
	Value valueTarget = value;
	if (isCompound) {
		valueTarget = value["value"];
	}

	if (channel->addr.dbr_field_type == DBR_ENUM) {
		*(uint16_t*)(pValueBuffer) = (valueTarget)["index"].as<uint16_t>();
	} else if (nElements == 1) {
		IOCSource::setBuffer(valueTarget, pValueBuffer);
	} else {
		IOCSource::setBuffer(valueTarget, pValueBuffer, nElements);
	}

	DBErrorMessage dbErrorMessage(dbPutField(&channel->addr, channel->addr.dbr_field_type, pValueBuffer, nElements));
	if (dbErrorMessage) {
		throw std::runtime_error(dbErrorMessage.c_str());
	}
}

/**
 * Utility function to get the TypeCode that the given database channel is configured for
 *
 * @param pChannel the pointer to the database channel to get the TypeCode for
 * @param errOnLinks determines whether to throw an error on finding links, default no
 * @return the TypeCode that the channel is configured for
 */
TypeCode IOCSource::getChannelValueType(const dbChannel* pChannel, const bool errOnLinks) {
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
			if (errOnLinks) {
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
 * @param valueTarget the value to set
 * @param pValueBuffer pointer to the database value buffer
 */
void IOCSource::getValue(Value& valueTarget, const void* pValueBuffer) {
	auto valueType(valueTarget.type());
	if (valueType.code == TypeCode::String) {
		valueTarget = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, getValue, (valueTarget, pValueBuffer));
	}
}

/**
 * Set a return value from the given database value buffer.  This is the array version of the function
 *
 * @param valueTarget the value to set
 * @param pValueBuffer the database value buffer
 * @param nElements the number of elements in the buffer
 */
void IOCSource::getValue(Value& valueTarget, const void* pValueBuffer, const long& nElements) {
	auto valueType(valueTarget.type());
	if (valueType.code == TypeCode::String) {
		valueTarget = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, getValue, (valueTarget, pValueBuffer, nElements));
	}
}

/**
 * Set alarm metadata in the given return value.
 *
 * @param metadata metadata containing alarm metadata
 * @param value the value to set metadata for
 */
void IOCSource::getAlarmMetadata(Value& value, const Metadata& metadata) {
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
void IOCSource::getTimestampMetadata(Value& value, const Metadata& metadata) {
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
void IOCSource::getDisplayMetadata(Value& value, const Metadata& metadata) {
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
void IOCSource::getControlMetadata(Value& value, const Metadata& metadata) {
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
void IOCSource::getAlarmLimitMetadata(Value& value, const Metadata& metadata) {
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
 * @param valueTarget the return value
 * @param pValueBuffer the pointer to the data containing the database data to store in the return value
 */
template<typename valueType> void IOCSource::getValue(Value& valueTarget, const void* pValueBuffer) {
	valueTarget = ((valueType*)pValueBuffer)[0];
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
 * @param valueTarget the return value
 * @param pValueBuffer the pointer to the data containing the database data to store in the return value
 * @param nElements the number of elements in the array
 */
template<typename valueType>
void IOCSource::getValue(Value& valueTarget, const void* pValueBuffer, const long& nElements) {
	shared_array<valueType> values(nElements);
	for (auto i = 0; i < nElements; i++) {
		values[i] = ((valueType*)pValueBuffer)[i];
	}
	valueTarget = values.freeze().template castTo<const void>();
}

/**
 * Get value into given database buffer
 *
 * @param valueTarget the value to get
 * @param pValueBuffer the database buffer to put it in
 */
void IOCSource::setBuffer(const Value& valueTarget, void* pValueBuffer) {
	auto valueType(valueTarget.type());
	if (valueType.code == TypeCode::String) {
		auto strValue = valueTarget.as<std::string>();
		auto len = MIN(strValue.length(), MAX_STRING_SIZE - 1);

		valueTarget.as<std::string>().copy((char*)pValueBuffer, len);
		((char*)pValueBuffer)[len] = '\0';
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setBuffer, (valueTarget, pValueBuffer));
	}
}

void IOCSource::setBuffer(const Value& valueTarget, void* pValueBuffer, long nElements) {
	auto valueType(valueTarget.type());
	if (valueType.code == TypeCode::StringA) {
		char valueRef[20];
		for (auto i = 0; i < nElements; i++) {
			snprintf(valueRef, 20, "[%d]", i);
			auto strValue = valueTarget[valueRef].as<std::string>();
			auto len = MIN(strValue.length(), MAX_STRING_SIZE - 1);
			strValue.copy((char*)pValueBuffer + MAX_STRING_SIZE * i, len);
			((char*)pValueBuffer + MAX_STRING_SIZE * i)[len] = '\0';
		}
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setBuffer, (valueTarget, pValueBuffer, nElements));
	}
}

// Get the value into the given database value buffer (templated)
template<typename valueType> void IOCSource::setBuffer(const Value& valueTarget, void* pValueBuffer) {
	((valueType*)pValueBuffer)[0] = valueTarget.as<valueType>();
}

// Get the value into the given database value buffer (templated)
template<typename valueType>
void IOCSource::setBuffer(const Value& valueTarget, void* pValueBuffer, long nElements) {
	auto valueArray = valueTarget.as<shared_array<const valueType>>();
	for (auto i = 0; i < nElements; i++) {
		((valueType*)pValueBuffer)[i] = valueArray[i];
	}
}

} // pvxs
} // ioc
