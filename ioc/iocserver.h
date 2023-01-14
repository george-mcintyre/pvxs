/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCSERVER_H
#define PVXS_IOCSERVER_H

#include "pvxs/server.h"
#include "grouppv.h"

namespace pvxs {
namespace ioc {

class IOCServer : public server::Server {

public:
	explicit IOCServer(const server::Config& config);
	GroupPvMap pvMap;
	std::list<std::string> groupDefinitionFiles;

	// For locking access to GroupPvMap
	epicsMutex pvMapMutex{};
};

IOCServer &iocServer();

} // pvxs
} // ioc

#endif //PVXS_IOCSERVER_H
