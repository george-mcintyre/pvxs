/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "singlesrcsubscriptionctx.h"

namespace pvxs {
namespace ioc {

SingleSourceSubscriptionCtx::SingleSourceSubscriptionCtx(const std::shared_ptr<dbChannel>& pChannel) {
	pValueChannel = pChannel;
	pPropertiesChannel.reset(dbChannelCreate(dbChannelName(pChannel)), [](dbChannel* ch) {
		if (ch) dbChannelDelete(ch);
	});
	if (pPropertiesChannel && dbChannelOpen(pPropertiesChannel.get())) {
		throw std::bad_alloc();
	}

}
} // iocs
} // pvxs
