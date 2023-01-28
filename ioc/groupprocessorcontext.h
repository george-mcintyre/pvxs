/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPROCESSORCONTEXT_H
#define PVXS_GROUPPROCESSORCONTEXT_H

#include <string>
#include <utility>
#include "iocserver.h"

#include "groupconfigprocessor.h"

namespace pvxs {
namespace ioc {

class GroupProcessorContext {
	const std::string channelPrefix;
	GroupConfigProcessor* groupConfigProcessor;

public:
	std::string groupName, field, key;
	unsigned depth; // number of '{'s
	std::string errorMessage;

	GroupProcessorContext(std::string &channelPrefix, GroupConfigProcessor* groupConfigProcessor)
			:channelPrefix(channelPrefix), groupConfigProcessor(groupConfigProcessor), depth(0u) {
	}
	void canAssign() const;
	void assign(const Value& value);

};

} // pvxs
} // ioc

#endif //PVXS_GROUPPROCESSORCONTEXT_H
