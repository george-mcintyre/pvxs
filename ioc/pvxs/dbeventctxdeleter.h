/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBEVENTCTXDELETER_H
#define PVXS_DBEVENTCTXDELETER_H

namespace pvxs {
namespace ioc {

class DbEventCtxDeleter {
public:
	void operator()(dbEventCtx ctxt);
};

}
}

#endif //PVXS_DBEVENTCTXDELETER_H
