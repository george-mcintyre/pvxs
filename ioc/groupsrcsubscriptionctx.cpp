/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <functional>
#include <unordered_set>

#include "groupsrcsubscriptionctx.h"
namespace pvxs {
namespace ioc {

GroupSourceSubscriptionCtx::GroupSourceSubscriptionCtx(const IOCGroup& subscribedGroup)
		:group(subscribedGroup), fieldMap() {
	prototype = group.valueTemplate;
}
void GroupSourceSubscriptionCtx::subscribeField(dbEventCtx eventContext, const IOCGroupField& field,
		EVENTFUNC* subscriptionValueCallback, unsigned int selectOptions, bool forValues) {
	auto pChannel = ((std::shared_ptr<dbChannel>)(forValues ? field.valueChannel : field.propertiesChannel));
	auto& pEventSubMap = forValues ? pValueEventSubscription[pChannel.get()] : pPropertiesEventSubscription[pChannel
			.get()];
	pEventSubMap.first = pChannel; // set the shared pointer
	auto& pEventSubscription = pEventSubMap.second; // set the event subscription
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
};

} // pvxs
} // ioc
