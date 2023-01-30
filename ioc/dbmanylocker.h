/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 30/01/2023.
//

#ifndef PVXS_DBMANYLOCKER_H
#define PVXS_DBMANYLOCKER_H

#include <dbLock.h>

namespace pvxs {
namespace ioc {

class DBManyLock {

public:
	dbLocker* pLocker{};

	DBManyLock();
	DBManyLock& operator=(const DBManyLock& other) = default;
	DBManyLock(DBManyLock& other) = default;
	explicit DBManyLock(const std::vector<dbCommon*>& channels, unsigned flags = 0);
	~DBManyLock();
	explicit operator dbLocker*() const;

	DBManyLock(const DBManyLock&) = delete;
};

struct DBManyLocker {
	const DBManyLock lock;
	explicit DBManyLocker(DBManyLock& L)
			:lock(L) {
		dbScanLockMany(lock.pLocker);
	}
	~DBManyLocker() {
		dbScanUnlockMany(lock.pLocker);
	}
};

} // pvxs
} // ioc

#endif //PVXS_DBMANYLOCKER_H
