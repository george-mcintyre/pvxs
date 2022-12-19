/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include <dbEvent.h>
#include <dbAddr.h>
#include <pvxs/source.h>
#include <dbAccess.h>

#define IOC_OPTIONS (DBR_STATUS | \
    DBR_AMSG | \
    DBR_UNITS | \
    DBR_PRECISION | \
    DBR_TIME | \
    DBR_UTAG | \
    DBR_ENUM_STRS | \
    DBR_GR_DOUBLE | \
    DBR_CTRL_DOUBLE | \
    DBR_AL_DOUBLE)

#define checkedSetField(_lvalue, _rvalue) \
if (auto&& __field = value[#_rvalue] ) { \
	__field = _lvalue; \
}

#define checkedSetDoubleField(_lvalue, _rvalue) \
if (auto&& __field = value[#_rvalue] ) { \
    if ( !isnan(_lvalue)) { \
        __field = _lvalue; \
    } \
}

#define checkedSetStringField(_lvalue, _rvalue) \
if (auto&& __field = value[#_rvalue] ) { \
    if ( strlen(_lvalue)) { \
        __field = _lvalue; \
    } \
}

namespace pvxs {
namespace ioc {

/**
 * structure to store metadata internally
 */
typedef struct metadata {
	dbCommon metadata;
	const char* pUnits;
	const dbr_precision* pPrecision;
	const dbr_enumStrs* enumStrings;
	const struct dbr_grDouble* graphicsDouble;
	const struct dbr_ctrlDouble* controlDouble;
	const struct dbr_alDouble* alarmDouble;
} Metadata;

class SingleSource : public server::Source {
	List allrecords;
	static dbEventCtx eventContext;
	static epicsMutexId eventLock;

//	static std::map<std::shared_ptr<dbChannel>, std::map<epicsEventId, std::vector<dbEventSubscription>>> subscriptions;
	static void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& putOperation,
			const std::shared_ptr<dbChannel>& pChannel);
	static TypeCode getChannelValueType(const std::shared_ptr<dbChannel>& pChannel);
	static void getMetadata(void*& pValueBuffer, Metadata& metadata);
	static void onDisableSubscription(const std::shared_ptr<dbChannel>& pChannel);
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);
	static void onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
			const Value& valuePrototype, const Value& value);
	static void onStartSubscription(const std::shared_ptr<dbChannel>& pChannel);
	static long nameToAddr(const char* pvName, DBADDR* pdbAddress);
	static void setValue(Value& value, void* pValueBuffer);
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer);
	static void setValue(Value& value, void* pValueBuffer, long nElements);
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer, long nElements);

	static void setAlarmMetadata(Metadata& metadata, Value& value);
	static void setTimestampMetadata(Metadata& metadata, Value& value);
	static void setDisplayMetadata(Metadata& metadata, Value& value);
	static void setControlMetadata(const Metadata& metadata, Value& value);
	static void setAlarmLimitMetadata(const Metadata& metadata, Value& value);
/*
	static void subscriptionCallback(void* user_arg, const std::shared_ptr<dbChannel>& pChannel, int eventsRemaining,
			db_field_log* pfl);
*/

public:
	SingleSource();
	void onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) final;
	List onList() final;
	void onSearch(Search& searchOperation) final;
	void show(std::ostream& outputStream) final;
};

} // ioc
} // pvxs


#endif //PVXS_SINGLESOURCE_H
