/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_GROUP_H
#define PVXS_GROUP_H

#include <map>

#include <pvxs/data.h>

#include "dbmanylocker.h"
#include "field.h"

namespace pvxs {
namespace ioc {

class Group {
private:
public:
	std::string name;
	Fields fields;
	bool atomicPutGet, atomicMonitor;
	Value valueTemplate;
	std::vector<dbCommon*> valueChannels;
	std::vector<dbCommon*> propertiesChannels;
	DBManyLock lock;
	DBManyLock propertiesLock;

	Group();
	virtual ~Group();
	virtual void show(int level) const;
	Field& operator[](const std::string& fieldName);
};

// A map of group name to IOCGroup
typedef std::map<std::string, Group> IOCGroupMap;

} // pvxs
} // ioc

#endif //PVXS_GROUP_H
