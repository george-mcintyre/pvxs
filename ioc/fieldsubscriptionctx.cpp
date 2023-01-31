/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "fieldsubscriptionctx.h"

namespace pvxs {
namespace ioc {

void FieldSubscriptionCtx::subscribeField(dbEventCtx eventContext, EVENTFUNC (* subscriptionValueCallback),
		unsigned int selectOptions, bool forValues) {

	auto& pChannel = (forValues ? field->valueChannel : field->propertiesChannel).ptr();
	auto& pEventSubscription = forValues ? pValueEventSubscription : pPropertiesEventSubscription;
	pEventSubscription.reset(
			db_add_event(
					eventContext,
					pChannel.get(),
					subscriptionValueCallback,
					this, selectOptions),
			[](dbEventSubscription pEventSubscription) {
				if (pEventSubscription) {
					db_cancel_event(pEventSubscription);
				}
			});

	if (!pEventSubscription) {
		throw std::runtime_error("Failed to create db subscription");
	}
}

FieldSubscriptionCtx::FieldSubscriptionCtx(IOCGroupField& field, GroupSourceSubscriptionCtx* groupSourceSubscriptionCtx)
		:pGroupCtx(groupSourceSubscriptionCtx), field(&field) {

};

} // pvcs
} // ioc
