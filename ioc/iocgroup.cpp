/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "iocgroup.h"

namespace pvxs {
namespace ioc {

/**
 * Show details for this group
 * @param level the level of detail to show.  0 fields only
 */
void IOCGroup::show(int level) const {
	// no locking as we only print things which are const after initialization

	// Group field information
	printf("  Atomic Get/Put:%s Atomic Monitor:%s Members:%ld\n",
			(atomicPutGet ? "yes" : "no"),
			(atomicMonitor ? "yes" : "no"),
			fields.size());

	// If we need to show detailed information then iterate through all fields showing details
	if (level > 1) {
		if (fields.empty()) {
			printf("/");
		} else {
			for (auto& field: fields) {
				printf("    ");
				field.fieldName.show();
				printf(" <-> %s\n", dbChannelName(field.channel));
			}
		}
	}
}

IOCGroup::IOCGroup()
		:atomicPutGet(false), atomicMonitor(false) {
}

IOCGroup::~IOCGroup() {
}

} // pvxs
} // ioc
