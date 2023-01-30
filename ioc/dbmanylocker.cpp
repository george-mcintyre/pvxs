/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 30/01/2023.
//

#include <vector>
#include <dbCommon.h>
#include "dbmanylocker.h"

namespace pvxs {
namespace ioc {

DBManyLock::DBManyLock()
		:pLocker(nullptr) {
}

DBManyLock::DBManyLock(const std::vector<dbCommon*>& channels, unsigned flags) {
	dbCommon** recordsArray = nullptr;
	if (!channels.empty()) {
		recordsArray = reinterpret_cast<dbCommon**>(((dbCommon**)&channels)[0]);
	}

	pLocker = dbLockerAlloc(recordsArray, channels.size(), flags);
	if (!pLocker) {
		throw std::invalid_argument("Failed to create locker");
	}
}

DBManyLock::~DBManyLock() {
	if (pLocker) {
		// TODO Why does this say that the memory was never allocated
//		dbLockerFree(pLocker);
		pLocker = nullptr;
	}
}

DBManyLock::operator dbLocker*() const {
	return pLocker;
}

} // pvxs
} // ioc
