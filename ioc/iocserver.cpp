/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "iocserver.h"

namespace pvxs {
namespace ioc {
IOCServer::IOCServer(const server::Config& config)
		:pvxs::server::Server(config) {
}
} // pvxs
} // ioc
