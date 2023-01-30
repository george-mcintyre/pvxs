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

class GroupSourceSubscriptionCtx : public SubscriptionCtx {
	IOCGroup& group;
	IOCGroupField& field;

public:
	std::shared_ptr<dbChannel> pValueChannel;
	std::shared_ptr<dbChannel> pPropertiesChannel;
	Value leafNode;

	explicit GroupSourceSubscriptionCtx(IOCGroup& group, IOCGroupField& field);
};

} // pvxs
} // ioc

#endif //PVXS_GROUPSRCSUBSCRIPTIONCTX_H
