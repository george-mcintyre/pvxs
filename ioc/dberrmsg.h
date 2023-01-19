/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBERRMSG_H
#define PVXS_DBERRMSG_H

#include <epicsTypes.h>

namespace pvxs {
namespace ioc {

class DBErrMsg {
	long status = 0;
	char msg[MAX_STRING_SIZE]{};
public:
	DBErrMsg(long sts = 0);
	DBErrMsg& operator=(long sts);
	explicit operator bool() const;
	const char* c_str() const;
};

} // ioc
} // pvxs
#endif //PVXS_DBERRMSG_H
