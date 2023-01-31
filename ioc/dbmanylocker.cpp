/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <dbCommon.h>
#include "dbmanylocker.h"

namespace pvxs {
namespace ioc {

/**
 * Empty lock
 */
DBManyLock::DBManyLock()
		:pLocker(nullptr) {
}

/**
 * Create a many lock from a list of channels
 *
 * @param channels the list of channels to lock
 * @param flags the lock flags to be passed on to dbLockerAlloc()
 */
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

/**
 * When the lock goes out of scope, then free the lock
 */
DBManyLock::~DBManyLock() {
	if (pLocker) {
		dbLockerFree(pLocker);
		pLocker = nullptr;
	}
}

DBManyLock::operator dbLocker*() const {
	return pLocker;
}

DBManyLock::DBManyLock(DBManyLock&& other) noexcept
		:pLocker(other.pLocker) {
	other.pLocker = nullptr;
}

DBManyLock& DBManyLock::operator=(DBManyLock&& other) noexcept {
	if (pLocker) {
		dbLockerFree(pLocker);
	}
	pLocker = other.pLocker;
	other.pLocker = nullptr;
	return *this;
}
//

} // pvxs
} // ioc
