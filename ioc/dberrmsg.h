/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBERRMSG_H
#define PVXS_DBERRMSG_H

namespace pvxs {
namespace ioc {

#define DBERRMSG_LEN 40
class DBErrMsg {
	long status = 0;
	char msg[DBERRMSG_LEN]{};
public:
	DBErrMsg(long sts = 0);
	DBErrMsg& operator=(long sts);
	explicit operator bool() const;
	const char* c_str() const;
};

} // ioc
} // pvxs
#endif //PVXS_DBERRMSG_H
