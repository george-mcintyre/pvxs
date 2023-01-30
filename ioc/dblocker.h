/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 30/01/2023.
//

#ifndef PVXS_DBLOCKER_H
#define PVXS_DBLOCKER_H

#include <dbLock.h>

namespace pvxs {
namespace ioc {

class DBLock {
public:
	dbLocker* pLocker;

	DBLock();
	DBLock(const DBLock&) = default;
	explicit DBLock(dbCommon* channel, unsigned flags = 0);
	~DBLock();
	explicit operator dbLocker*() const;
};

struct DBLocker {
	DBLock lock;
	explicit DBLocker(DBLock& L)
			:lock(L) {
		dbScanLockMany(lock.pLocker);
	}

	~DBLocker() {
		dbScanUnlockMany(lock.pLocker);
	}
};

} // pvxs
} // ioc

#endif //PVXS_DBLOCKER_H
