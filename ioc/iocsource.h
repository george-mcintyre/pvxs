/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCSOURCE_H
#define PVXS_IOCSOURCE_H

#include <pvxs/data.h>

#include "dbeventcontextdeleter.h"
#include "singlesrcsubscriptionctx.h"
#include "metadata.h"

#define FOR_VALUES true
#define FOR_PROPERTIES false

// Maximum amount of space that metadata can use in the buffer returned by dbGetField()
#define MAX_METADATA_SIZE 100

namespace pvxs {
namespace ioc {

class IOCSource {
public:
	static void get(dbChannel* pChannel,
			const Value& valuePrototype, bool forValues, bool forProperties,
			const std::function<void(Value&)>& returnFn, const std::function<void(const char*)>& errorFn);
	static void put(dbChannel* pChannel, const Value& value);

	//////////////////////////////
	// Get & Subscription
	//////////////////////////////
	// Get a return value from the given database value buffer
	static void getValue(Value& valueTarget, const void* pValueBuffer);
	// Get a return value from the given database value buffer
	static void getValue(Value& valueTarget, const void* pValueBuffer, const long& nElements);
	// Get a return value from the given database value buffer (templated)
	template<typename valueType> static void getValue(Value& valueTarget, const void* pValueBuffer);
	// Get a return value from the given database value buffer (templated)
	template<typename valueType> static void getValue(Value& valueTarget, const void* pValueBuffer, const long& nElements);
	// Get alarm metadata in the given return value
	static void getAlarmMetadata(Value& value, const Metadata& metadata);
	// Get timestamp metadata in the given return value
	static void getTimestampMetadata(Value& value, const Metadata& metadata);
	// Get display metadata in the given return value
	static void getDisplayMetadata(Value& value, const Metadata& metadata);
	// Get control metadata in the given return value
	static void getControlMetadata(Value& value, const Metadata& metadata);
	// Get alarm limit metadata in the given return value
	static void getAlarmLimitMetadata(Value& value, const Metadata& metadata);

	//////////////////////////////
	// Set
	//////////////////////////////
	// Set the value into the given database value buffer
	static void setBuffer(const Value& valueTarget, void* pValueBuffer);
	// Set the value into the given database value buffer
	static void setBuffer(const Value& valueTarget, void* pValueBuffer, long nElements);
	// Set the value into the given database value buffer (templated)
	template<typename valueType> static void setBuffer(const Value& valueTarget, void* pValueBuffer);
	// Set the value into the given database value buffer (templated)
	template<typename valueType> static void setBuffer(const Value& valueTarget, void* pValueBuffer, long nElements);

	//////////////////////////////
	// Common Utils
	//////////////////////////////
	// Utility function to get the TypeCode that the given database channel is configured for
	static TypeCode getChannelValueType(const dbChannel* pChannel, bool errOnLinks = false);
	// Get metadata from the given value buffer and deliver it in the given metadata buffer
	static void getMetadata(void*& pValueBuffer, Metadata& metadata, bool forValues, bool forProperties);
};

} // pvxs
} // ioc

#endif //PVXS_IOCSOURCE_H
