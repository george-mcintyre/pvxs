/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPVCHANNELFIELD_H
#define PVXS_GROUPPVCHANNELFIELD_H

#include <vector>
#include <string>
#include <epicsTypes.h>

namespace pvxs {
namespace ioc {

class GroupPvChannelField {
public:
	explicit GroupPvChannelField(std::string  name);
public:
	std::string name;
	epicsUInt32 index{};
	bool isArray() const;
};

typedef std::vector<GroupPvChannelField> GroupPvChannelFields;

} // pvxs
} // ioc
#endif //PVXS_GROUPPVCHANNELFIELD_H
