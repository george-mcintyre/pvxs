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
		:had_initial_VALUE(false), had_initial_PROPERTY(false), isMeta(false), allowProc(false), channel(channelName),
		 fieldName(stringFieldName) {
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
				shared_array<const Value> valueArray = value.as<shared_array<const Value>>();
				auto capacity = valueArray.max_size();   // Is this the capacity
				if ((index + 1) > capacity) {
//					valueArray.thaw();
//					valueArray.resize(index+1);
					// Copy old values
					// TODO ...
				}

				// Put new data into array
//				valueArray[index] = value.allocMember();
//				value.from(valueArray.freeze());

				std::stringstream elementRef;
				elementRef << "[" << index << "]";
				value = value[elementRef.str()];
			}
		}
	}
	return value;
}
} // pvxs
} // ioc
