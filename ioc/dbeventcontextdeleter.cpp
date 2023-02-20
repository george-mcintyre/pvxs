/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include "dbeventcontextdeleter.h"

void pvxs::ioc::DBEventContextDeleter::operator()(const dbEventCtx& eventContext) {
    db_close_events(eventContext);
}
