/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>
#include "dberrmsg.h"

namespace pvxs {
namespace ioc {

DBErrMsg::DBErrMsg(long sts) {
	(*this) = sts;
}

DBErrMsg& DBErrMsg::operator=(long sts) {
	status = sts;
	if (!sts) {
		msg[0] = '\0';
	} else {
// TODO ci fails with: ‘errSymLookup’ was not declared in this scope, so don't lookup error
		std::snprintf( msg, DBERRMSG_LEN, "database error: %ld", sts );
//		errSymLookup(sts, msg, sizeof(msg));
//		msg[sizeof(msg) - 1] = '\0';
	}
	return *this;
}

DBErrMsg::operator bool() const {
	return status;
}

const char* DBErrMsg::c_str() const {
	return msg;
}

} // ioc
} // pvxs
