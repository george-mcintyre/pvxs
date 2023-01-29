/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUPFIELDNAME_H
#define PVXS_IOCGROUPFIELDNAME_H

#include <string>

#include "iocgroupfieldnamecomponent.h"

#define PADDING_CHARACTER  ' '
#define PADDING_WIDTH 15

namespace pvxs {
namespace ioc {

/**
 * Implements a group field as a delegate over a vector of group field components.
 * Therefore it can be used as a vector with size(), empty(), operator[], back() and swap() methods implemented.
 *
 * The group field is a vector of group field components.
 *
 */
class IOCGroupFieldName {
private:
public:
	IOCGroupFieldNameComponents fieldNameComponents;
	bool empty() const;
	size_t size() const;

	explicit IOCGroupFieldName(const std::string& fieldName);

	void swap(IOCGroupFieldName& o);
	const IOCGroupFieldNameComponent& operator[](size_t i) const;
	const IOCGroupFieldNameComponent& back() const;
	std::string to_string(size_t padLength = 0) const;
	const std::string& leafFieldName() const;

	void show(const std::string& suffix = {}) const;
};

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPFIELDNAME_H
