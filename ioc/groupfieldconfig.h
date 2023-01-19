/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPFIELDCONFIG_H
#define PVXS_GROUPFIELDCONFIG_H

#include <string>
#include <map>

namespace pvxs {
namespace ioc {

/**
 * Class to read the group field configuration into during initialization.
 * It is subsequently read into GroupChannelField for runtime use
 */
class GroupFieldConfig {
public:
	std::string type, channel, trigger, structureId;
	int64_t putOrder;
};

typedef std::map<std::string, GroupFieldConfig> GroupFieldsConfigMap;

} // pvxs
} // ioc

#endif //PVXS_GROUPFIELDCONFIG_H
