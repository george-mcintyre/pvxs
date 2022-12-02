/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <atomic>
#include <memory>
#include <stdexcept>
#include <sstream>

#include <pvxs/log.h>
#include <pvxs/server.h>
#include <pvxs/source.h>
#include <pvxs/iochooks.h>

#include <iocsh.h>
#include <epicsStdio.h>
#include <epicsExit.h>
#include <epicsExport.h>

#include <initHooks.h>
#include <pvxs/iocshcommand.h>

/**
 * Library for implementation of IOC hooks.
 * IOC works in a standard way, execution follows a set of phases in which hooks can be attached.  The attached
 * hooks are executed when encountered.   Implementing hooks allows an IOC implementation to completely control
 * the way it starts up and is implemented.  This library uses hooks to implement the IOC functionality
 * based on pvxs instead of PVAccess and PVData from epics-base.
 *
* The phases and their hooks are as follows:
 * - iocInit() PHASE:
 *   - iocInit(): PHASE:
 *     - initHookAtIocBuild = 0,        - Start of iocBuild()
 *     - initHookAtBeginning            - Database sanity checks passed
 *     - initHookAfterCallbackInit      - Callbacks, generalTime & taskwd init
 *     - initHookAfterCaLinkInit        - CA links init
 *     - initHookAfterInitDrvSup        - Driver support init
 *     - initHookAfterInitRecSup        - Record support init
 *     - initHookAfterInitDevSup        - Device support init pass 0
 *     - initHookAfterInitDatabase      - Records and lock-sets init
 *     - initHookAfterFinishDevSup      - Device support init pass 1
 *     - initHookAfterScanInit          - Scan, AS, ProcessNotify init
 *     - initHookAfterInitialProcess    - Records with PINI = YES processed
 *     - initHookAfterCaServerInit      - RSRV init
 *     - initHookAfterIocBuilt          - End of iocBuild()
 *   - iocRun(): PHASE:
 *     - initHookAtIocRun                - Start of iocRun()
 *     - initHookAfterDatabaseRunning    - Scan tasks and CA links running
 *     - initHookAfterCaServerRunning    - RSRV running
 *     - initHookAfterIocRunning         - End of iocRun() / iocInit()
 * - iocPause() PHASE:
 *   - initHookAtIocPause                - Start of iocPause()
 *   - initHookAfterCaServerPaused       - RSRV paused
 *   - initHookAfterDatabasePaused       - CA links and scan tasks paused
 *   - initHookAfterIocPaused            - End of iocPause()
 * - iocShutdown() PHASE:
 *   - initHookAtShutdown                - Start of iocShutdown() (unit tests only)
 *   - initHookAfterCloseLinks           - Links disabled/deleted
 *   - initHookAfterStopScan             - Scan tasks stopped
 *   - initHookAfterStopCallback         - Callback tasks stopped
 *   - initHookAfterStopLinks            - CA links stopped
 *   - initHookBeforeFree                - Resource cleanup about to happen
 *   - initHookAfterShutdown             - End of iocShutdown()
 *
 */
using namespace pvxs;

namespace {
std::atomic<server::Server*> instance{};

DEFINE_LOGGER(log, "pvxs.ioc");

void pvxsl(int detail)
{
    try {
        if(auto serv = instance.load()) {
            for(auto& pair : serv->listSource()) {
                auto src = serv->getSource(pair.first, pair.second);
                if(!src)
                    continue; // race?

                auto list = src->onList();

                if(detail>0)
                    printf("# Source %s@%d%s\n",
                           pair.first.c_str(), pair.second,
                           list.dynamic ? " [dynamic]":"");

                if(!list.names) {
                    if(detail>0)
                        printf("# no PVs\n");
                } else {
                    for(auto& name : *list.names) {
                        printf("%s\n", name.c_str());
                    }
                }
            }
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}

void pvxsr(int detail)
{
    try {
        if(auto serv = instance.load()) {
            std::ostringstream strm;
            Detailed D(strm, detail);
            strm<<*serv;
            printf("%s", strm.str().c_str());
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}

void pvxs_target_info()
{
    try {
        std::ostringstream capture;
        target_information(capture);
        printf("%s", capture.str().c_str());
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}


void pvxsAtExit(void* unused)
{
    try {
        if(auto serv = instance.load()) {
            if(instance.compare_exchange_strong(serv, nullptr)) {
                // take ownership
                std::unique_ptr<server::Server> trash(serv);
                trash->stop();
                log_debug_printf(log, "Stopped Server?%s", "\n");
            }
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}

void pvxsInitHook(initHookState state)
{
    try {
        // iocBuild()
        if(state==initHookAfterInitDatabase) {
            // we want to run before exitDatabase
            epicsAtExit(&pvxsAtExit, nullptr);
        }
        // iocRun()/iocPause()
        if(state==initHookAfterCaServerRunning) {
            if(auto serv = instance.load()) {
                serv->start();
                log_debug_printf(log, "Started Server %p", serv);
            }
        }
        if(state==initHookAfterCaServerPaused) {
            if(auto serv = instance.load()) {
                serv->stop();
                log_debug_printf(log, "Stopped Server %p", serv);
            }
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}

void pvxsRegistrar()
{
    try {
        pvxs::logger_config_env();

	    ioc::IOCShCommand<int>("pvxsl", "detail").implementation<&pvxsl>();
	    ioc::IOCShCommand<int>("pvxsr", "detail").implementation<&pvxsr>();
	    ioc::IOCShCommand<>("pvxs_target_info").implementation<&pvxs_target_info>();

        auto serv = instance.load();
        if(!serv) {
            std::unique_ptr<server::Server> temp(new server::Server(server::Config::from_env()));

            if(instance.compare_exchange_strong(serv, temp.get())) {
                log_debug_printf(log, "Installing Server %p\n", temp.get());
                temp.release();
            } else {
                log_crit_printf(log, "Race installing Server? %p\n", serv);
            }
        } else {
            log_err_printf(log, "Stale Server? %p\n", serv);
        }

        initHookRegister(&pvxsInitHook);
    } catch(std::exception& e) {
        fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
    }
}

} // namespace

namespace pvxs {
namespace ioc {

server::Server server()
{
    if(auto serv = instance.load()) {
        return *serv;
    } else {
        throw std::logic_error("No Instance");
    }
}

}} // namespace pvxs::ioc

extern "C" {
epicsExportRegistrar(pvxsRegistrar);
}
