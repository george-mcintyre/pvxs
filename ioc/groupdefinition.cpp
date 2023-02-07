/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include "groupdefinition.h"

namespace pvxs {
namespace ioc {

/**
 * Total number of channels referenced by this group.  This function counts all the channels referenced
 * by all the fields in this group.  This is a second stage group configuration processing object
 *
 * @return the total number of channels referenced by this group
 */
size_t ioc::GroupDefinition::channelCount() const {
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
