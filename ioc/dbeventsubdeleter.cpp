/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <dbEvent.h>

#include "pvxs/dbeventsubdeleter.h"

namespace pvxs {
namespace ioc {

void DbEventSubscriptionDeleter::operator()(dbEventSubscription ctxt) {
	db_cancel_event(ctxt);
}

} // ioc
} // pvxs
