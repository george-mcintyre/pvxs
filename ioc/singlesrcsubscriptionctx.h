/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESRCSUBSCRIPTIONCTX_H
#define PVXS_SINGLESRCSUBSCRIPTIONCTX_H

#include <pvxs/source.h>
#include <dbChannel.h>

#include "subscriptionctx.h"

namespace pvxs {
namespace ioc {

/**
 * A subscription context
 */
class SingleSourceSubscriptionCtx : public SubscriptionCtx<std::shared_ptr<void> > {

public:
	explicit SingleSourceSubscriptionCtx(const std::shared_ptr<dbChannel>& sharedPtr);
// For locking access to subscription context
	std::shared_ptr<dbChannel> pValueChannel;
	std::shared_ptr<dbChannel> pPropertiesChannel;
};

} // ioc
} // pvxs

#endif //PVXS_SINGLESRCSUBSCRIPTIONCTX_H
