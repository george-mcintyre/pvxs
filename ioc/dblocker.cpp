/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 30/01/2023.
//

#include <iostream>
#include <vector>
#include <dbCommon.h>
#include "dblocker.h"

namespace pvxs {
namespace ioc {
DBLock::DBLock()
		:pLocker(nullptr) {
}

DBLock::DBLock(dbCommon* channel, unsigned int flags) {
	dbCommon** recordsArray = nullptr;
	if (channel) {
		recordsArray = &channel;
	}

	pLocker = dbLockerAlloc(recordsArray, 1, flags);
	if (!pLocker) {
		throw std::invalid_argument("Failed to create locker");
	}
}

DBLock::~DBLock() {
	if (pLocker) {
		dbLockerFree(pLocker);
		pLocker = nullptr;
	}
}

DBLock::operator dbLocker*() const {
	return pLocker;
}

DBLock::DBLock(DBLock&& other) noexcept {
	other.pLocker = nullptr;
}

DBLock& DBLock::operator=(DBLock&& other) noexcept {
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
