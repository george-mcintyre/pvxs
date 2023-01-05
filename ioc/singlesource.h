/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include "dbeventcontextdeleter.h"
#include "subscriptioncontext.h"
#include "metadata.h"

namespace pvxs {
namespace ioc {

/**
 * Single Source class to handle initialisation, processing, and shutdown of single source database record support
 *  - Handlers for get, put and subscriptions
 *  - type converters to and from pvxs and db
 */
class SingleSource : public server::Source {
public:
	SingleSource();
	void onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) final;
	List onList() final;
	void onSearch(Search& searchOperation) final;
	void show(std::ostream& outputStream) final;

private:
	// List of all database records that this single source serves
	List allRecords;
	// The event context for all subscriptions
	std::unique_ptr<std::remove_pointer<dbEventCtx>::type, DBEventContextDeleter> eventContext;

	// Create request and subscription handlers for single record sources
	void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& putOperation,
			const std::shared_ptr<dbChannel>& pChannel);

	// Utility function to get the TypeCode that the given database channel is configured for
	static TypeCode getChannelValueType(const std::shared_ptr<dbChannel>& pChannel);

	//////////////////////////////
	// Subscriptions
	//////////////////////////////
	__attribute__((unused)) static void subscriptionValueCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	__attribute__((unused)) static void subscriptionPropertiesCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	static void subscriptionCallback(SubscriptionCtx* subscriptionCtx, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	// Called when a client pauses / stops a subscription it has been subscribed to
	static void onDisableSubscription(const std::shared_ptr<subscriptionCtx>& subscriptionContext);
	// Called when a client starts a subscription it has subscribed to
	static void onStartSubscription(const std::shared_ptr<subscriptionCtx>& subscriptionContext);

	//////////////////////////////
	// Get
	//////////////////////////////
	// Handle the get operation
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);
	static void onGet(const std::shared_ptr<dbChannel>& channel,
			const Value& valuePrototype, bool forValues, bool forProperties,
			const std::function<void(Value&)>& returnFn, const std::function<void(const char*)>& errorFn);

	//////////////////////////////
	// Put
	//////////////////////////////
	// Handler invoked when a peer sends data on a PUT
	static void onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
			const Value& valuePrototype, const Value& value);
	// Set the value into the given database value buffer
	static void setBuffer(const Value& value, void* pValueBuffer);
	// Set the value into the given database value buffer
	static void setBuffer(const Value& value, void* pValueBuffer, long nElements);
	// Set the value into the given database value buffer (templated)
	template<typename valueType> static void setBuffer(const Value& value, void* pValueBuffer);
	// Set the value into the given database value buffer (templated)
	template<typename valueType> static void setBuffer(const Value& value, void* pValueBuffer, long nElements);

	//////////////////////////////
	// Get & Subscription
	//////////////////////////////
	// Set a return value from the given database value buffer
	static void setValue(Value& value, void* pValueBuffer);
	// Set a return value from the given database value buffer
	static void setValue(Value& value, void* pValueBuffer, long nElements);
	// Set a return value from the given database value buffer (templated)
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer);
	// Set a return value from the given database value buffer (templated)
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer, long nElements);
	// Get metadata from the given value buffer and deliver it in the given metadata buffer
	static void getMetadata(void*& pValueBuffer, Metadata& metadata, bool forValues, bool forProperties);
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
};

} // ioc
} // pvxs


#endif //PVXS_SINGLESOURCE_H
