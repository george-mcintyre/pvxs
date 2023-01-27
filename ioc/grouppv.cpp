/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "grouppv.h"

namespace pvxs {
namespace ioc {

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
