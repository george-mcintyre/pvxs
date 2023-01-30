/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>
#include <utility>
#include "iocgroupfield.h"

namespace pvxs {
namespace ioc {

/**
 * Construct an IOCGroupField from a field name and channel name
 *
 * @param stringFieldName the field name
 * @param stringChannelName the channel name
 */
IOCGroupField::IOCGroupField(const std::string& stringFieldName, const std::string& stringChannelName)
		:isMeta(false), allowProc(false), valueChannel(stringChannelName), propertiesChannel(stringChannelName),
		 fieldName(stringFieldName), isArray(false) {
	if (!fieldName.fieldNameComponents.empty()) {
		name = fieldName.fieldNameComponents[0].name;
		fullName = std::string(fieldName.to_string());
	}
}

/**
 * Using the field components configured in this IOCGroupField, walk down from the given value,
 * to arrive at the part of the value referenced by this field.
 *
 * @param top the given value
 * @return the Value referenced by this field within the given value
 */
Value IOCGroupField::findIn(Value value) const {
	if (!fieldName.empty()) {
		for (const auto& component: fieldName.fieldNameComponents) {
			value = value[component.name];
			if (component.isArray()) {
				// Get required array capacity
				auto index = component.index;
				shared_array<const Value> constValueArray = value.as<shared_array<const Value>>();
				value = shared_array<const Value>();
				shared_array<Value> valueArray(constValueArray.thaw());
				auto size = valueArray.size();
				if ((index + 1) > size) {
					valueArray.resize(index + 1);
				}

				// Put new data into array
				auto newElement = valueArray[index];
				if ( !newElement) {
					// Only allocate new member if it is not already allocated
					valueArray[index] = newElement = value.allocMember();
				}
				value = valueArray.freeze();
				value = newElement;
			}
		}
	}
	return value;
}

} // pvxs
} // ioc
