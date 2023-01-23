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

#include "iocgroupfieldname.h"
#include "iocgroupchannel.h"

namespace pvxs {
namespace ioc {

typedef std::vector<size_t> IOCGroupTriggers;

class IOCGroupField {
private:
public:
	IOCGroupTriggers triggers;          // index in IOCGroup::fields
	bool had_initial_VALUE, had_initial_PROPERTY, allowProc;
	IOCGroupChannel channel;
	IOCGroupFieldName fieldName;

	IOCGroupField();
	IOCGroupField(const std::string& fieldName, const std::string& channelName);
};

typedef std::vector<IOCGroupField> IOCGroupFields;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPFIELD_H
