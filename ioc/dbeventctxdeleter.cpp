/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <dbEvent.h>

#include "pvxs/dbeventctxdeleter.h"

namespace pvxs {
namespace ioc {

void DbEventCtxDeleter::operator()(dbEventCtx ctxt) {
	db_close_events(ctxt);
}

} // ioc
} // pvxs
