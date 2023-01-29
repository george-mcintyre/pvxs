/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "groupsrcsubscriptionctx.h"
namespace pvxs {
namespace ioc {

GroupSourceSubscriptionCtx::GroupSourceSubscriptionCtx(IOCGroup& subscribedGroup, IOCGroupField& subscribedField)
		:group(subscribedGroup), field(subscribedField), leafNode(field.findIn(prototype)) {
	prototype = group.valueTemplate;

	// values channel
	auto& channel = field.channel;
	pValueChannel = (std::shared_ptr<dbChannel>)channel;

	// properties channel
	pPropertiesChannel = std::shared_ptr<dbChannel>(dbChannelCreate(dbChannelName(pValueChannel)),
			[](dbChannel* ch) {
				if (ch) {
					dbChannelDelete(ch);
				}
			});
	if (pPropertiesChannel && dbChannelOpen(pValueChannel.get())) {
		throw std::bad_alloc();
	}
};

} // pvxs
} // ioc
