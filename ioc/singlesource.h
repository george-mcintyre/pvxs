/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include "dbeventcontextdeleter.h"
#include "metadata.h"
#include "singlesrcsubscriptionctx.h"

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
	DBEventContext eventContext;

	// Create request and subscription handlers for single record sources
	void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>&& channelControl,
			const std::shared_ptr<dbChannel>& dbChannelSharedPtr);
	// Handles all get, put and subscribe requests
	static void onOp(const std::shared_ptr<dbChannel>& dbChannelSharedPtr, const Value& valuePrototype,
			std::unique_ptr<server::ConnectOp>&& channelConnectOperation);
	// Helper function to create a value prototype for the given channel
	static Value getValuePrototype(const std::shared_ptr<dbChannel>& dbChannelSharedPtr);

	//////////////////////////////
	// Get
	//////////////////////////////
	// Handle the get operation
	static void get(dbChannel* channel, std::unique_ptr<server::ExecOp>& getOperation,
			const Value& valuePrototype);

	//////////////////////////////
	// Subscriptions
	//////////////////////////////
	// Called when values are requested by a subscription
	static void subscriptionValueCallback(void* userArg, dbChannel* pDbChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	// Called when properties are requested by a subscription
	static void subscriptionPropertiesCallback(void* userArg, dbChannel* pDbChannel, int eventsRemaining,
			db_field_log* pDbFieldLog);
	// General subscriptions callback
	static void subscriptionCallback(SingleSourceSubscriptionCtx* subscriptionCtx, dbChannel* pDbChannel,
			int eventsRemaining, db_field_log* pDbFieldLog);
	// Called by onStart() when a client pauses / stops a subscription it has been subscribed to
	static void onDisableSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext);
	// Called by onStart() when a client starts a subscription it has subscribed to
	static void onStartSubscription(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext);
	// Called when a subscription is being set up
	void onSubscribe(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext,
			std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) const;
	// Called when a client starts or stops a subscription. isStarting flag determines which
	static void onStart(const std::shared_ptr<SingleSourceSubscriptionCtx>& subscriptionContext, bool isStarting) ;

};

} // ioc
} // pvxs


#endif //PVXS_SINGLESOURCE_H
