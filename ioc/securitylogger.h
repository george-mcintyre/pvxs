/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_SECURITYLOGGER_H
#define PVXS_SECURITYLOGGER_H

namespace pvxs {
namespace ioc {

class SecurityLogger {
    void* pvt;
public:
    SecurityLogger();
    explicit SecurityLogger(void* pvt);
    ~SecurityLogger();

    void swap(SecurityLogger& o);

    SecurityLogger(const SecurityLogger&) = delete;
    SecurityLogger& operator=(const SecurityLogger&) = delete;
};

} // pvxs
} // ioc

#endif //PVXS_SECURITYLOGGER_H
