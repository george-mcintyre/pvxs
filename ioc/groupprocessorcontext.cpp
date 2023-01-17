/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "groupprocessorcontext.h"

namespace pvxs {
namespace ioc {

/**
 * Check whether anything can be assigned at the current depth within the json stream being processed.
 * Throw an exception if not
 */
void GroupProcessorContext::canAssign() const {
	if (depth < 2 || depth > 3) {
		throw std::runtime_error("Can't assign value in this context");
	}
}

/**
 * Assign the given value appropriately given the current context.
 * The context holds the current field, key, depth, etc.
 *
 * @param value the value to assign
 */
void GroupProcessorContext::assign(const Value& value) {
	canAssign();
	auto &groupPvConfig = iocServer->groupPvMap[groupName];

	if (depth == 2) {
		if (field == "+atomic") {
			groupPvConfig.atomic = value.as<bool>() ? True : False;

		} else if (field == "+id") {
			groupPvConfig.structureId = value.as<std::string>();

		} else {
			iocServer->groupProcessingWarnings += "Unknown group option ";
			iocServer->groupProcessingWarnings += field;
		}
		field.clear();

	} else if (depth == 3) {
		auto &groupField = groupPvConfig.fieldMap[field];

		if (key == "+type") {
			groupField.type = value.as<std::string>();

		} else if (key == "+channel") {
			groupField.channel = channelPrefix + value.as<std::string>();

		} else if (key == "+id") {
			groupField.structureId = value.as<std::string>();

		} else if (key == "+trigger") {
			groupField.trigger = value.as<std::string>();

		} else if (key == "+putorder") {
			groupField.putOrder = value.as<int64_t>();

		} else {
			iocServer->groupProcessingWarnings += "Unknown group field option ";
			iocServer->groupProcessingWarnings += field + ":" + key ;
		}
		key.clear();
	}
}

} // pvxs
} // ioc