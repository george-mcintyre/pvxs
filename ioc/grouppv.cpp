/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>

#include "grouppv.h"
#include "grouppvchannel.h"

namespace pvxs {
namespace ioc {

/**
 * Show details for this group
 * @param level the level of detail to show.  0 fields only
 */
void GroupPv::show(int level) const {
	// no locking as we only print things which are const after initialization

	// Group field information
	std::cout << "  Atomic Get/Put:" << (atomicPutGet ? "yes" : "no")
	          << " Atomic Monitor:" << (atomicMonitor ? "yes" : "no")
	          << " Members:" << channels->size()
	          << std::endl;

	// If we need to show detailed information then iterate through all fields showing details
	if (level > 1) {
		for (std::vector<GroupPvChannel>::const_iterator it(channels->begin()), end(channels->end());
		     it != end; ++it) {
			const GroupPvChannel& groupPvInfo = *it;
			std::cout << "  ";
			groupPvInfo.showFields();
			std::cout << "\t<-> " << groupPvInfo.pdbChannel->name << std::endl;
		}
	}
}
} // pvxs
} // ioc
