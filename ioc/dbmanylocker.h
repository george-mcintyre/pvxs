/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_DBMANYLOCKER_H
#define PVXS_DBMANYLOCKER_H

#include <vector>

#include <dbLock.h>

namespace pvxs {
namespace ioc {

/**
 * A lock that can lock multiple DB records simultaneously.  To be used with DBManyLocker
 *
 * e.g.
 * 		DBManyLock lock = DBManyLock(channels);
 */
class DBManyLock {

public:
	dbLocker* pLocker{};

	explicit DBManyLock(const std::vector<dbCommon*>& channels, unsigned flags = 0);

	DBManyLock();
	DBManyLock(DBManyLock&& other) noexcept;
	~DBManyLock();

	explicit operator dbLocker*() const;
	DBManyLock& operator=(DBManyLock&& other) noexcept;

	// Prevent copy construction and assignment
	DBManyLock(const DBManyLock&) = delete;
	DBManyLock& operator=(const DBManyLock& other) = delete;
};

/**
 * To lock access to multiple DB records simultaneously.
 * Use by creating a new object.  It will lock the records locked by the constructor parameter while the object is
 * in scope.  First you need to create a lock using the DBManyLock().
 *
 * e.g.
 * 		{
 * 			DBManyLock lock = DBManyLock(channels);
 *			DBManyLocker F(lock); // Lock all channels
 *			for ( auto& pDbChannel: channels ) {
 *				IOCSource::put(pDbChannel, ...);
 *			}
 *			...
 * 		}  // Unlocked
 */
class DBManyLocker {
public:
	const DBManyLock& lock;
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
