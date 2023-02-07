/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_FIELD_H
#define PVXS_FIELD_H

#include <string>
#include <map>
#include <vector>
#include <set>

#include <pvxs/nt.h>

#include "dblocker.h"
#include "channel.h"
#include "dbmanylocker.h"
#include "fieldname.h"

namespace pvxs {
namespace ioc {

class Field;
typedef std::vector<Field*> Triggers;

class Field {
private:
public:
	std::string id;     // For structure functionality
	std::string name;
	FieldName fieldName;
	std::string fullName;
	bool isMeta, allowProc;
	bool isArray;
	Channel valueChannel;
	Channel propertiesChannel;  // Used only in subscriptions

	Triggers triggers;          // index in Group::fields
	std::vector<dbCommon*> referencedValueChannels;
	std::vector<dbCommon*> referencedPropertiesChannels;
	DBManyLock lock;                    // Lock to use when field subscription value change event triggers
	DBManyLock propertiesLock;          // Lock to use when field subscription property change event triggers

	Field(const std::string& stringFieldName, const std::string& stringChannelName);
	Value findIn(Value value) const;
};

typedef std::vector<Field> Fields;

} // pvxs
} // ioc

#endif //PVXS_FIELD_H
