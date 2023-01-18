/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPCONFIG_H
#define PVXS_GROUPCONFIG_H

#include <string>
#include "groupfieldconfig.h"

namespace pvxs {
namespace ioc {

/**
 * Class to store the group configuration as it is read in.  It is subsequently
 * read into the GroupPv class for runtime use
 */
class GroupConfig {

public:
	bool atomic, atomic_set;
	std::string structureId;
	GroupFieldsConfigMap groupFields;
	GroupConfig() :atomic(true), atomic_set(false) {}
};

// A map of group name to GroupConfig
typedef std::map<std::string, GroupConfig> GroupConfigMap;

} // pvxs
} // ioc

#endif //PVXS_GROUPCONFIG_H
