/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "groupprocessorcontext.h"

namespace pvxs {
namespace ioc {

void GroupProcessorContext::canAssign() const {
	if (depth < 2 || depth > 3) {
		throw std::runtime_error("Can't assign value in this context");
	}
}

void GroupProcessorContext::assign(const epics::pvData::AnyScalar& value) {
	canAssign();
	auto groupPvConfig = iocServer->groupPvMap[groupName].get();

	if (depth == 2) {
		if (field == "+atomic") {
			groupPvConfig->atomic = value.as<epics::pvData::boolean>() ? True : False;

		} else if (field == "+id") {
			groupPvConfig->structureId = value.as<std::string>();

		} else {
			iocServer->groupProcessingWarnings += "Unknown group option ";
			iocServer->groupProcessingWarnings += field;
		}
		field.clear();

	} else if (depth == 3) {
		auto groupField = groupPvConfig->fieldMap[field].get();

		if (key == "+type") {
			groupField->type = value.ref<std::string>();

		} else if (key == "+channel") {
			groupField->channel = channelPrefix + value.ref<std::string>();

		} else if (key == "+id") {
			groupField->structureId = value.ref<std::string>();

		} else if (key == "+trigger") {
			groupField->trigger = value.ref<std::string>();

		} else if (key == "+putorder") {
			groupField->putOrder = value.as<epics::pvData::int32>();

		} else {
			iocServer->groupProcessingWarnings += "Unknown group field option ";
			iocServer->groupProcessingWarnings += field + ":" + key ;
		}
		key.clear();
	}
}

} // pvxs
} // ioc
