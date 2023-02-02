/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>
#include "grouppvfield.h"

namespace pvxs {
namespace ioc {

/**
 * Part of the second pass group configuration processing. This is the constructor for a group field configuration
 * object.
 *
 * @param fieldConfig the first stage field configuration object it will be based on
 * @param fieldName the name of the field
 */
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
