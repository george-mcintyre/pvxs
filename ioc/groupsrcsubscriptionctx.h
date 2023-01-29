/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPSRCSUBSCRIPTIONCTX_H
#define PVXS_GROUPSRCSUBSCRIPTIONCTX_H

#include <pvxs/source.h>
#include "iocgroup.h"
#include "subscriptionctx.h"

namespace pvxs {
namespace ioc {

// TODO use correct template for subscription context
class GroupSourceSubscriptionCtx : public SubscriptionCtx<std::shared_ptr<void>> {
	std::shared_ptr<IOCGroup> pGroup;

public:
	std::set<std::shared_ptr<dbChannel>> pValueChannels;
	std::set<std::shared_ptr<dbChannel>> pPropertiesChannels;

	explicit GroupSourceSubscriptionCtx(IOCGroup& group) {
	};
};

} // pvxs
} // ioc

#endif //PVXS_GROUPSRCSUBSCRIPTIONCTX_H
