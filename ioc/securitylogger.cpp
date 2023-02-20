/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include <algorithm>

#include <asLib.h>

#include "securitylogger.h"

namespace pvxs {
namespace ioc {

SecurityLogger::~SecurityLogger() {
    asTrapWriteAfterWrite(pvt);
}

void SecurityLogger::swap(SecurityLogger& o) {
    std::swap(pvt, o.pvt);
}

SecurityLogger::SecurityLogger(void* pvt)
        :pvt(pvt) {
}

SecurityLogger::SecurityLogger()
        :pvt(nullptr) {
}

} // pvxs
} // ioc
