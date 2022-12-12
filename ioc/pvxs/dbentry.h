/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBENTRY_H
#define PVXS_DBENTRY_H

#include <dbStaticLib.h>

namespace pvxs {
namespace ioc {

class DBEntry {
	DBENTRY ent{};
public:
	DBEntry();
	~DBEntry();
	operator DBENTRY*();
	DBENTRY* operator->();
};

} // ioc
} // pvxs
#endif //PVXS_DBENTRY_H
