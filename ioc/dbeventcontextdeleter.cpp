/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 19/12/2022.
//

#include "dbeventcontextdeleter.h"

void pvxs::ioc::DBEventContextDeleter::operator()(dbEventCtx eventContext) {
	db_close_events(eventContext);
}
