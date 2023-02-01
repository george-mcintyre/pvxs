/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUP_H
#define PVXS_IOCGROUP_H

#include <map>
#include <pvxs/data.h>

#include "iocgroupfield.h"
#include "dbmanylocker.h"

namespace pvxs {
namespace ioc {

class IOCGroup {
private:
public:
	std::string name;
	IOCGroupFields fields;
	bool atomicPutGet, atomicMonitor;
	Value valueTemplate;
	std::vector<dbCommon*> valueChannels;
	std::vector<dbCommon*> propertiesChannels;
	DBManyLock lock;
	// TODO when is properties lock used?
	DBManyLock propertiesLock;

	IOCGroup();
	virtual ~IOCGroup();
	virtual void show(int level) const;
	IOCGroupField& operator[](const std::string& fieldName);
};

// A map of group name to IOCGroup
typedef std::map<std::string, IOCGroup> IOCGroupMap;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUP_H
