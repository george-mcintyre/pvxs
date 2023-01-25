/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include "dbeventcontextdeleter.h"
#include "singlesrcsubscriptionctx.h"
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
	void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl,
			const std::shared_ptr<dbChannel>& pChannel);

	//////////////////////////////
	// Subscriptions
	//////////////////////////////
	static void subscriptionValueCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	static void subscriptionPropertiesCallback(void* userArg, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	static void subscriptionCallback(SingleSourceSubscriptionCtx* subscriptionCtx, dbChannel* pChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	// Called when a client pauses / stops a subscription it has been subscribed to
	static void onDisableSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext);
	// Called when a client starts a subscription it has subscribed to
	static void onStartSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext);

	//////////////////////////////
	// Get
	//////////////////////////////
	// Handle the get operation
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);

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
};

} // ioc
} // pvxs


#endif //PVXS_SINGLESOURCE_H
