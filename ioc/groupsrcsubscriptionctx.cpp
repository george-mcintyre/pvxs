/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include "groupsrcsubscriptionctx.h"

namespace pvxs {
namespace ioc {

GroupSourceSubscriptionCtx::GroupSourceSubscriptionCtx(Group& subscribedGroup)
		:group(subscribedGroup) {
}

} // pvxs
} // ioc
