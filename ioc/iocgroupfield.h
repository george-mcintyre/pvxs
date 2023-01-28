/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUPFIELD_H
#define PVXS_IOCGROUPFIELD_H

#include <map>
#include <vector>
#include <set>
#include <pvxs/nt.h>

#include "iocgroupfieldname.h"
#include "iocgroupchannel.h"

namespace pvxs {
namespace ioc {

typedef std::vector<size_t> IOCGroupTriggers;

class IOCGroupField {
private:
public:
	IOCGroupTriggers triggers;          // index in IOCGroup::fields
	bool isMeta, allowProc;
	IOCGroupChannel channel;
	IOCGroupFieldName fieldName;
	std::string name;
	std::string id; // For future structure functionality
	std::string fullName;
	bool isArray;

	IOCGroupField(const std::string& stringFieldName, const std::string& stringChannelName);
	Value& walkToValue(Value& top);
};

typedef std::vector<IOCGroupField> IOCGroupFields;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPFIELD_H
