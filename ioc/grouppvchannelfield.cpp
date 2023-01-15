/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "grouppvchannelfield.h"

#include <utility>
namespace pvxs {
namespace ioc {

GroupPvChannelField::GroupPvChannelField(std::string name)
		:name(std::move(name)) {
}
bool GroupPvChannelField::isArray() const {
	return index != (epicsUInt32)-1;;
}
} // pvxs
} // ioc
