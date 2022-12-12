/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>

#include <pvxs/source.h>
#include <pvxs/iochooks.h>
#include <pvxs/singlesource.h>

#include <initHooks.h>
#include <epicsExport.h>

namespace {
using namespace pvxs;

/**
 * Initialise qsrv database records by adding them as sources in our running pvxs server instance
 *
 * @param theInitHookState the initHook state - we only want to trigger on the initHookAfterIocBuilt state - ignore all others
 */
void qsrvRecordInit(initHookState theInitHookState) {
	if (theInitHookState != initHookAfterIocBuilt) {
		return;
	}
	pvxs::ioc::server().addSource("qsrv", std::make_shared<pvxs::ioc::SingleSource>());
}

/**
 * IOC pvxs records registrar.  This implements the required registrar function that is called by xxxx_registerRecordDeviceDriver,
 * the auto-generated stub created for all IOC implementations.
 *
 * It is registered by using the `epicsExportRegistrar()` macro.
 *
 * 1. Register your hook handler to handle any state hooks that you want to implement.  Here we install
 * an `initHookState` handler connected to the `initHookAfterIocBuilt` state.  It  will add all of the
 * record sources defined so far.  Note that you can define sources up until the `iocInit()` call,
 * after which point the `initHookAfterIocBuilt` handlers are called and will register all the defined records.
 */
void pvxsRecordsRegistrar(void) {
	initHookRegister(&qsrvRecordInit);
}
} // namespace

// in .dbd file
//registrar(pvxsRecordsRegistrar)
extern "C" {
epicsExportRegistrar(pvxsRecordsRegistrar);
}
