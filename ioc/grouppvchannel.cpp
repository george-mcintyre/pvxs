/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>

#include "grouppvchannel.h"

namespace pvxs {
namespace ioc {

/**
 * To show the detail of the group fields related to this dbChannel
 */
void GroupPvChannel::showFields() const {
	// If there are no fields then output default string
	if (fields.empty()) {
		std::cout << "/";
	} else {
		// Otherwise, for each field output details
		bool first = true;
		for (const auto& field: fields) {
			if (first) {
				first = false;
			} else {
				std::cout << ".";
			}

			std::cout << field.name;
			if (field.isArray()) {
				std::cout << "[" << ((unsigned)field.index) << "]";
			}
		}
	}
}
} // pvxs
} // ioc

