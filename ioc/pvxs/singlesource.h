/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include <dbEvent.h>
#include <dbAddr.h>
#include "pvxs/source.h"

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
	static void onDisableSubscription(const std::shared_ptr<dbChannel>& pChannel);
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);
	static void onGetEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
			DBADDR& dbAddress, long& options, long& nElements);
	static void onGetNonEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
			dbAddr& dbAddress, long& options, long& nElements);
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
