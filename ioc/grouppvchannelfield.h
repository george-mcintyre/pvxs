/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPVCHANNELFIELD_H
#define PVXS_GROUPPVCHANNELFIELD_H

#include <vector>
#include <set>
#include <string>
#include <epicsTypes.h>

namespace pvxs {
namespace ioc {

typedef std::set<std::string> FieldTriggers;

class GroupPvChannelField {
public:
	explicit GroupPvChannelField(std::string  name);
public:
	std::string name;
	epicsUInt32 index{};
	std::string structureId;                // Field's Normative Type structure ID or any other arbitrary string if not a normative type
	std::string channel;                    // Database record name aka channel
	std::string type;                       // Database field type
	std::string trigger;                    // Database field trigger
	int putOrder;                           // Order to serialise the field for put operations

	bool isArray() const;
};

typedef std::vector<GroupPvChannelField> GroupPvChannelFields;

} // pvxs
} // ioc
#endif //PVXS_GROUPPVCHANNELFIELD_H
