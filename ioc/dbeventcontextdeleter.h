/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBEVENTCONTEXTDELETER_H
#define PVXS_DBEVENTCONTEXTDELETER_H

#include <dbEvent.h>

namespace pvxs {
namespace ioc {
class DBEventContextDeleter {
public:
	void operator()(const dbEventCtx& eventContext);
};

}
}
#endif //PVXS_DBEVENTCONTEXTDELETER_H
