/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "grouppv.h"

namespace pvxs {
namespace ioc {

/**
 * Total number of channels referenced by this group.  This function counts all the channels referenced
 * by all the fields in this group.
 *
 * @return the total number of channels referenced by this group
 */
size_t ioc::GroupPv::channelCount() const {
	size_t channelCount = 0;
	for (const auto& field: fields) {
		if (!field.channel.empty()) {
			channelCount++;
		}
	}
	return channelCount;
}

} // pvxs
} // ioc
