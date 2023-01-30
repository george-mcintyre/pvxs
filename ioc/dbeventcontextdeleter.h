/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBEVENTCONTEXTDELETER_H
#define PVXS_DBEVENTCONTEXTDELETER_H

#include <memory>
#include <type_traits>
#include <dbEvent.h>

namespace pvxs {
namespace ioc {
class DBEventContextDeleter {
public:
	void operator()(const dbEventCtx& eventContext);
};

typedef std::unique_ptr<std::remove_pointer<dbEventCtx>::type, DBEventContextDeleter> DBEventContext;

}
}
#endif //PVXS_DBEVENTCONTEXTDELETER_H
