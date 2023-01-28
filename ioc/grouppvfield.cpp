/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "grouppvfield.h"

namespace pvxs {
namespace ioc {

GroupPvField::GroupPvField(const GroupFieldConfig& fieldConfig, const std::string& fieldName)
		:putOrder(0) {
	channel = fieldConfig.channel;
	name = fieldName;
	structureId = fieldConfig.structureId;
	putOrder = fieldConfig.putOrder;
	type = fieldConfig.type;
}

}
}
