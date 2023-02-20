/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_DBENTRY_H
#define PVXS_DBENTRY_H

#include <dbStaticLib.h>

namespace pvxs {
namespace ioc {

/**
 * Wrapper class for DBENTRY that is a type that encapsulates an IOC database entry.
 */
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
