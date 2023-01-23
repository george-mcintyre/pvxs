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

namespace pvxs {
namespace ioc {

class IOCGroup {
private:
public:
//	epicsMutex lock;
	std::string name;
	IOCGroupFields fields;
	bool atomicPutGet, atomicMonitor;
	static size_t num_instances;
	Value valueTemplate;

	IOCGroup();
	virtual ~IOCGroup();
	virtual void show(int level) const;
};

// A map of group name to IOCGroup
typedef std::map<std::string, IOCGroup> IOCGroupMap;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUP_H
