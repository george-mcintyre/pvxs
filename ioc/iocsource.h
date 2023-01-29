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

namespace pvxs {
namespace ioc {

class IOCSource {
public:
	static void onGet(const std::shared_ptr<dbChannel>& channel,
			const Value& valuePrototype, bool forValues, bool forProperties,
			const std::function<void(Value&)>& returnFn, const std::function<void(const char*)>& errorFn);

	// Get metadata from the given value buffer and deliver it in the given metadata buffer
	static void getMetadata(void*& pValueBuffer, Metadata& metadata, bool forValues, bool forProperties);

	//////////////////////////////
	// Get & Subscription
	//////////////////////////////
	// Set a return value from the given database value buffer
	static void setValue(Value& valueTarget, void* pValueBuffer);
	// Set a return value from the given database value buffer
	static void setValue(Value& valueTarget, void* pValueBuffer, long nElements);
	// Set a return value from the given database value buffer (templated)
	template<typename valueType> static void setValue(Value& valueTarget, void* pValueBuffer);
	// Set a return value from the given database value buffer (templated)
	template<typename valueType> static void setValue(Value& valueTarget, void* pValueBuffer, long nElements);
	// Set alarm metadata in the given return value
	static void setAlarmMetadata(Metadata& metadata, Value& value);
	// Set timestamp metadata in the given return value
	static void setTimestampMetadata(Metadata& metadata, Value& value);
	// Set display metadata in the given return value
	static void setDisplayMetadata(Metadata& metadata, Value& value);
	// Set control metadata in the given return value
	static void setControlMetadata(const Metadata& metadata, Value& value);
	// Set alarm limit metadata in the given return value
	static void setAlarmLimitMetadata(const Metadata& metadata, Value& value);

	//////////////////////////////
	// Common Utils
	//////////////////////////////
	// Utility function to get the corresponding database address structure given a pvName
	static long nameToAddr(const char* pvName, DBADDR* pdbAddress);
	// Utility function to get the TypeCode that the given database channel is configured for
	static TypeCode getChannelValueType(const dbChannel* pChannel, bool erroOnLinks = false);

};

} // pvxs
} // ioc

#endif //PVXS_IOCSOURCE_H
