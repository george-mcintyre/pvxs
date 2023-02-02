/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>
#include <sstream>
#include <cstdlib>

#include "iocgroupfieldname.h"

namespace pvxs {
namespace ioc {

static void pad(std::string& stringToPad, size_t padLength);

/**
 * Construct a Group field name from a field name string.  The string is a sequence of components separated by
 * periods each of which may be optionally followed by an array specifier. e.g. a.b[1].c.
 *
 * This constructor breaks the string on periods and stores each component in the fieldNameComponents member,
 * while extracting the array reference where specified.
 * @param fieldName
 */
IOCGroupFieldName::IOCGroupFieldName(const std::string& fieldName) {
	if (!fieldName.empty()) {
		// Split field name on periods
		std::stringstream splitter(fieldName);
		std::string fieldNamePart;
		while (std::getline(splitter, fieldNamePart, '.')) {
			if (fieldNamePart.empty()) {
				throw std::runtime_error("Empty field component in: " + fieldName);
			}

			// If this is an array reference then extract the index
			auto endArraySpecifier = fieldNamePart.size();
			if (fieldNamePart[endArraySpecifier - 1] == ']') {
				const size_t startArraySpecifier = fieldNamePart.find_last_of('[');
				if (startArraySpecifier == std::string::npos) {
					throw std::runtime_error("Invalid field array sub-script in : " + fieldName);
				}

				auto arrayIndex = fieldNamePart.substr(startArraySpecifier + 1);
				long index = 0;
				char* endScan;
				index = strtol(arrayIndex.c_str(), &endScan, 10);
				if (*endScan != ']') {
					throw std::runtime_error("Invalid field array sub-script in : " + fieldName);
				}

				fieldNameComponents.emplace_back(fieldNamePart.substr(0, startArraySpecifier), index);
			} else {
				// Otherwise this is a regular field part
				fieldNameComponents.emplace_back(fieldNamePart);
			}
		}

		// If empty then throw an error
		if (fieldNameComponents.empty()) {
			throw std::runtime_error("Empty field name");
		}

		// If the field name ends in an array reference then it's an error.  e.g. a.b[1] is not allowed,
		// whereas a.b[1].c is allowed.
		if (fieldNameComponents.back().isArray()) {
			throw std::runtime_error("leaf field may not have sub-script : " + fieldName);
		}
	}
}

/**
 * Convert this group field name to a string.
 */
std::string IOCGroupFieldName::to_string(size_t padLength) const {
	std::string fieldName;
	if (fieldNameComponents.empty()) {
		fieldName = "/";
	} else {
		bool first = true;
		for (const auto& fieldNameComponent: fieldNameComponents) {
			if (!first) {
				fieldName += ".";
			} else {
				first = false;
			}
			fieldName += fieldNameComponent.name;
			if (fieldNameComponent.isArray()) {
				fieldName += "[" + std::to_string((unsigned)fieldNameComponent.index) + "]";
			}
		}
	}
	pad(fieldName, padLength);
	return fieldName;
}

/**
 * Show this field name.  All components are shown as they were configured.
 */
void IOCGroupFieldName::show(const std::string& suffix) const {
	printf("%s%s", to_string(PADDING_WIDTH - suffix.size()).c_str(), suffix.c_str());
}

/**
 * swap delegate
 *
 * @param o
 */
void IOCGroupFieldName::swap(IOCGroupFieldName& o) {
	fieldNameComponents.swap(o.fieldNameComponents);
}

/**
 * empty delegate
 *
 * @return
 */
bool IOCGroupFieldName::empty() const {
	return fieldNameComponents.empty() || (fieldNameComponents.size() == 1 && fieldNameComponents[0].name.empty());
}

/**
 * size delegate
 *
 * @return
 */
size_t IOCGroupFieldName::size() const {
	return fieldNameComponents.size();
}

/**
 * back() delegate
 *
 * @return
 */
const IOCGroupFieldNameComponent& IOCGroupFieldName::back() const {
	return fieldNameComponents.back();
}

/**
 * operator[] delegate
 *
 * @param i
 * @return
 */
const IOCGroupFieldNameComponent& IOCGroupFieldName::operator[](size_t i) const {
	return fieldNameComponents[i];
}
const std::string& IOCGroupFieldName::leafFieldName() const {
	return fieldNameComponents[fieldNameComponents.size() - 1].name;
}

/**
 * Utility function to pad given string with spaces
 *
 * @param stringToPad
 */
static void pad(std::string& stringToPad, const size_t padLength) {
	if (padLength > stringToPad.size()) {
		stringToPad.insert(stringToPad.size(), padLength - stringToPad.size(), PADDING_CHARACTER);
	}
}

} // pvxs
} // ioc
