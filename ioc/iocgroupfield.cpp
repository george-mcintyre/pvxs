/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>
#include "iocgroupfield.h"

namespace pvxs {
namespace ioc {

IOCGroupField::IOCGroupField(const std::string& stringFieldName, const std::string& channelName)
		: isMeta(false), allowProc(false), channel(channelName),
		 fieldName(stringFieldName), isArray(false) {
	if (!fieldName.fieldNameComponents.empty()) {
		name = fieldName.fieldNameComponents[0].name;
		fullName = std::string(fieldName.to_string());
	}
}

Value& IOCGroupField::walkToValue(Value& top) {
	Value& value = top;
	if (!fieldName.empty()) {
		for (const auto& component: fieldName.fieldNameComponents) {
			value = value[component.name];
			if (component.isArray()) {
				// Get required array capacity
				auto index = component.index;
				shared_array<const Value> constValueArray = value.as<shared_array<const Value>>();
				// TODO clear value so that we don't do a copy
				shared_array<Value> valueArray(constValueArray.thaw());
				auto size = valueArray.size();   // Is this the capacity
				if ((index + 1) > size) {
					valueArray.resize(index+1);
				}

				// Put new data into array
				auto newElement = valueArray[index] = value.allocMember();
				value = valueArray.freeze();
				value = newElement;
			}
		}
	}
	return value;
}
} // pvxs
} // ioc
