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

class DBLocker {
public:
	dbCommon* const lock;
	explicit DBLocker(dbCommon* L)
			:lock(L) {
		dbScanLock(lock);
	}

	~DBLocker() {
		dbScanUnlock(lock);
	}
};

} // pvxs
} // ioc

#endif //PVXS_DBLOCKER_H
