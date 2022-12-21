/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 21/12/2022.
//

#ifndef PVXS_SUBSCRIPTIONCONTEXT_H
#define PVXS_SUBSCRIPTIONCONTEXT_H

/**
 * Sets the given event-handled flag on the specified subscription context
 * @param _subscriptionCtx the subscription context
 * @param _event the event-handled flag to set
 */
#define setEventHandledFlag(_subscriptionCtx, _event) \
if ( !(_subscriptionCtx)->_event ) { \
    epicsMutexMustLock ( (_subscriptionCtx)->eventLock ); \
    (_subscriptionCtx)->_event = true; \
    epicsMutexUnlock ( (_subscriptionCtx)->eventLock ); \
}

namespace pvxs {
namespace ioc {

/**
 * A subscription context
 */
typedef struct subscriptionCtx {
	// For locking access to subscription context
	epicsMutexId eventLock{};
	std::shared_ptr<dbChannel> pValueChannel{};
	std::shared_ptr<dbChannel> pPropertiesChannel{};
	std::shared_ptr<void> pValueEventSubscription{};
	std::shared_ptr<void> pPropertyEventSubscription{};
	bool hadValueEvent = false;
	bool hadPropertyEvent = false;
	std::unique_ptr<server::MonitorControlOp> subscriptionControl{};
	Value prototype{};
} SubscriptionCtx;

} // ioc
} // pvxs

#endif //PVXS_SUBSCRIPTIONCONTEXT_H
