/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include <dbStaticLib.h>
#include <dbAccess.h>

#include "dbentry.h"

namespace pvxs {
namespace ioc {

DBEntry::DBEntry() {
    dbInitEntry(pdbbase, &ent);
}

DBEntry::~DBEntry() {
    dbFinishEntry(&ent);
}

DBEntry::operator DBENTRY*() {
    return &ent;
}

DBENTRY* DBEntry::operator->() {
    return &ent;
}

} // ioc
} // pvxs
