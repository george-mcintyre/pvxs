/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPROCESSORCONTEXT_H
#define PVXS_GROUPPROCESSORCONTEXT_H

#include <string>
#include <utility>
#include <pv/anyscalar.h>
#include "iocserver.h"

namespace pvxs {
namespace ioc {

class GroupProcessorContext {
	const std::string channelPrefix;
	IOCServer* iocServer;

	// depth 0 - invalid
	// depth 1 - top Object
	// depth 2 - Group
	// depth 3 - field

public:
	std::string msg;
	unsigned depth; // number of '{'s
	std::string groupName, field, key;

	GroupProcessorContext(std::string channelPrefix, IOCServer* iocServer)
			:channelPrefix(std::move(channelPrefix)), iocServer(iocServer), depth(0u) {
	}
	void canAssign() const;
	void assign(const epics::pvData::AnyScalar& value);

};

} // pvxs
} // ioc

#endif //PVXS_GROUPPROCESSORCONTEXT_H
