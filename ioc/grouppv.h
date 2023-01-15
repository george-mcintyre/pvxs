/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPV_H
#define PVXS_GROUPPV_H

#include <map>
#include <vector>
#include "grouppvchannel.h"

namespace pvxs {
namespace ioc {

/**
 * A Group PV
 * This class represents a group PV.  It contains a set of channels
 * `GroupPvChannels` that link the group to regular db channels.  Each of these channels
 * define fields that are scalar, array or processing placeholders
 */
class GroupPv {
	GroupPv() = default;
	virtual ~GroupPv() = default;
	bool atomicPutGet{ false }, atomicMonitor{ false };
	GroupPvChannels channels;

public:
	/**
	 * Show details for this group
	 * @param level the level of detail to show.  0 fields only
	 */
	void show(int level);
};

// A map of group name to GroupPv
typedef std::map<std::string, std::shared_ptr<GroupPv>> GroupPvMap;

} // pvxs
} // ioc

#endif //PVXS_GROUPPV_H
