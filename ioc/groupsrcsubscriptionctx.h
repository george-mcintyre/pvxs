/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPSRCSUBSCRIPTIONCTX_H
#define PVXS_GROUPSRCSUBSCRIPTIONCTX_H

#include <map>
#include <pvxs/source.h>
#include "iocgroup.h"
#include "subscriptionctx.h"
#include "dbeventcontextdeleter.h"

#include <vector>
#include "fieldsubscriptionctx.h"

namespace pvxs {
namespace ioc {

class GroupSourceSubscriptionCtx {
public:
	IOCGroup& group;
	epicsMutex eventLock{};
	std::unique_ptr<server::MonitorControlOp> subscriptionControl{};
	std::vector<FieldSubscriptionCtx> fieldSubscriptionContexts{};

	explicit GroupSourceSubscriptionCtx(IOCGroup& group);
};

} // pvxs
} // ioc

#endif //PVXS_GROUPSRCSUBSCRIPTIONCTX_H
