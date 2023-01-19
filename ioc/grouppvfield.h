/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPVFIELD_H
#define PVXS_GROUPPVFIELD_H

#include <vector>
#include <set>
#include <string>
#include <epicsTypes.h>

namespace pvxs {
namespace ioc {

typedef std::set<std::string> Triggers;

class GroupPvField {
public:
	GroupPvField()
			:putOrder(0) {
	}

	std::string name;                       // Field's name
	std::string channel;                    // Database record name aka channel
	std::string structureId;                // Field's Normative Type structure ID or any other arbitrary string if not a normative type
	std::string type;                       // Database field type
	Triggers triggers;                      // Fields in this group which are posted on events from channel
	int64_t putOrder;                       // Order to serialise the field for put operations
	bool isArray{false};                    // Is this field an array field

	bool operator<(const GroupPvField& o) const {
		return putOrder < o.putOrder;
	}
};

typedef std::vector<GroupPvField> GroupPvFields;
typedef std::map<std::string, size_t> GroupPvFieldsMap;

} // pvxs
} // ioc
#endif //PVXS_GROUPPVFIELD_H
