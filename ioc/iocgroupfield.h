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
#include "dblocker.h"
#include "dbmanylocker.h"

namespace pvxs {
namespace ioc {

class IOCGroupField;
typedef std::vector<IOCGroupField*> IOCGroupTriggers;

class IOCGroupField {
private:
public:
	std::string id; // For future structure functionality
	std::string name;
	IOCGroupFieldName fieldName;
	std::string fullName;
	bool isMeta, allowProc;
	bool isArray;
	IOCGroupChannel valueChannel;
	IOCGroupChannel propertiesChannel;  // Used only in subscriptions

	IOCGroupTriggers triggers;          // index in IOCGroup::fields
	std::vector<dbCommon*> referencedValueChannels;
	std::vector<dbCommon*> referencedPropertiesChannels;
	DBManyLock lock;                    // Lock to use when field subscription value change event triggers
	DBManyLock propertiesLock;          // Lock to use when field subscription property change event triggers

	IOCGroupField(const std::string& stringFieldName, const std::string& stringChannelName);
	Value findIn(Value value) const;
};

typedef std::vector<IOCGroupField> IOCGroupFields;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPFIELD_H
