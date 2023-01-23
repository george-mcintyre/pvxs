/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "iocgroupfieldnamecomponent.h"

#include <utility>

namespace pvxs {
namespace ioc {

/**
 * Construct an simple Field name component holder
 */
IOCGroupFieldNameComponent::IOCGroupFieldNameComponent()
		:index((uint32_t)-1) {
}

/**
 * Construct an Field Name Component from the given name and index
 *
 * @param name the field name component
 * @param index the index of the field name component if the component is an array of structures.  Note
 *              that index will only ever be specified in configuration if this is an array of structures.
 */
IOCGroupFieldNameComponent::IOCGroupFieldNameComponent(std::string name, uint32_t index)
		:name(std::move(name)), index(index) {
}

/**
 * Is this an array of structures.  Determines whether this component is an array of structures or not, by looking
 * at the index field.  If it is -1 then its not an array of structures otherwise it is a simple scalar
 * or an array of scalars
 *
 * @return true if this is an array of structures
 */
bool IOCGroupFieldNameComponent::isArray() const {
	return index != (uint32_t)-1;
}

} // pvxs
} // ioc
