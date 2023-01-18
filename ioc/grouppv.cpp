/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>

#include "grouppv.h"

namespace pvxs {
namespace ioc {

/**
 * Show details for this group
 * @param level the level of detail to show.  0 fields only
 */
void GroupPv::show(int level) const {
	// no locking as we only print things which are const after initialization

	// Group field information
	std::cout << "  Atomic Get/Put:" << (atomicPutGet ? "yes" : "no")
	          << " Atomic Monitor:" << (atomicMonitor ? "yes" : "no")
	          << " Members:" << fields.size()
	          << std::endl;

	// If we need to show detailed information then iterate through all fields showing details
	if (level > 1) {
		std::cout << "  ";
		if (fields.empty()) {
			std::cout << "/";
		} else {
			bool first = true;
			for (auto& field: fields) {
				if (first) {
					first = false;
				} else {
					std::cout << ".";
				}

				std::cout << field.name;
				if (field.isArray) {
					std::cout << "[]";
				}
				std::cout << "\t<-> " << field.channel << std::endl;
			}
		}
	}
}
} // pvxs
} // ioc
