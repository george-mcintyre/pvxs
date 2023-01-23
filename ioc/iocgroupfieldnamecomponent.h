/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUPFIELDNAMECOMPONENT_H
#define PVXS_IOCGROUPFIELDNAMECOMPONENT_H

#include <string>
#include <vector>

namespace pvxs {
namespace ioc {

/**
 * A field component.  Fields can be made up of any number of components.  e.g. a.b[1].c
 * Each of the components (a, b, or c) are represented by a component.
 * isArray() determines whether this component is an array of structures or not, by looking
 * at the index field.  If it is -1 then its not an array of structures otherwise it is a simple scalar
 * or an array of scalars.
 *
 * An array of structures is an array whose elements are themselves structures.
 */
class IOCGroupFieldNameComponent {
	// If this component is an array this is the index into the array
	IOCGroupFieldNameComponent();
public:
	explicit IOCGroupFieldNameComponent(std::string name, uint32_t index = (uint32_t)-1);
	bool isArray() const;
// the name of this field component
	std::string name;
	uint32_t index;
};

typedef std::vector<IOCGroupFieldNameComponent> IOCGroupFieldNameComponents;

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPFIELDNAMECOMPONENT_H
