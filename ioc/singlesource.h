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

namespace pvxs {
namespace ioc {

class SingleSource : public server::Source {
	List allrecords;
	static dbEventCtx eventContext;
	static epicsMutexId eventLock;
//	static std::map<std::shared_ptr<dbChannel>, std::map<epicsEventId, std::vector<dbEventSubscription>>> subscriptions;
	static void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& putOperation,
			const std::shared_ptr<dbChannel>& pChannel);
	static TypeCode getChannelValueType(const std::shared_ptr<dbChannel>& pChannel) ;
	static void getMetadata(void*& pValueBuffer, dbCommon& metadata, const char*& pUnits, const dbr_precision*& pPrecision,
			const dbr_enumStrs*& enumStrings, const dbr_grLong*& graphicsLong, const dbr_grDouble*& graphicsDouble,
			const dbr_ctrlLong*& controlLong, const dbr_ctrlDouble*& controlDouble, const dbr_alLong*& alarmLong,
			const dbr_alDouble*& alarmDouble);
	static void onDisableSubscription(const std::shared_ptr<dbChannel>& pChannel);
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);
	static void onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
			const Value& valuePrototype, const Value& value);
	static void onStartSubscription(const std::shared_ptr<dbChannel>& pChannel);
	static long nameToAddr(const char* pname, DBADDR* paddr);
	static void setValue(Value& value, void* pValueBuffer);
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer);
	static void setValue(Value& value, void* pValueBuffer, long nElements);
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer, long nElements);
	template<typename valueType> static void setValueFromBuffer(Value& value, valueType* pValueBuffer);
	template<typename valueType> static void setValueFromBuffer(Value& value, valueType* pValueBuffer, long nElements);
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
