/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

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

void IOCGroupField::allocateMembers(ArrayCapacityMap& capacityMap, Value& returnValue) const {
	// find the leaf node
	auto leafName = fullName.substr(0, fullName.find_first_of('['));
	auto &&leafNode = returnValue[leafName];

	//
	auto capacity = capacityMap[leafName];
	shared_array<Value> arr(capacity);

	for ( auto i = 0; i < capacity; ++i) {
		arr[i] = leafNode.allocMember();
	}
	leafNode = arr.freeze();
}
} // pvxs
} // ioc
