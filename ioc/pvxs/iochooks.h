/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#ifndef PVXS_IOCHOOKS_H
#define PVXS_IOCHOOKS_H

#include <pvxs/version.h>
#include "iocshcommand.h"

#if defined(_WIN32) || defined(__CYGWIN__)

#  if defined(PVXS_IOC_API_BUILDING) && defined(EPICS_BUILD_DLL)
/* building library as dll */
#    define PVXS_IOC_API __declspec(dllexport)
#  elif !defined(PVXS_IOC_API_BUILDING) && defined(EPICS_CALL_DLL)
/* calling library in dll form */
#    define PVXS_IOC_API __declspec(dllimport)
#  endif

#elif __GNUC__ >= 4
#  define PVXS_IOC_API __attribute__ ((visibility("default")))
#endif

#ifndef PVXS_IOC_API
#  define PVXS_IOC_API
#endif


namespace pvxs {
namespace server {
class Server;
}
namespace ioc {

/** Return the singleton Server instance which is setup
 *  for use in an IOC process.
 *
 *  This Server instance is created during a registrar function,
 *  started by the initHookAfterCaServerRunning phase of iocInit().
 *  It is stopped and destroyed during an epicsAtExit() hook added
 *  during an initHookAfterInitDatabase hook..
 *
 *  Any configuration changes via. epicsEnvSet() must be made before registrars are run
 *  by \*_registerRecordDeviceDriver(pdbbase).
 *
 *  server::SharedPV and server::Source added before iocInit() will be available immediately.
 *  Others may be added (or removed) later.
 *
 *  @throws std::logic_error if called before instance is created, or after instance is destroyed.
 *
 * @code
 * static void myinitHook(initHookState state) {
 *     if(state!=initHookAfterIocBuilt)
 *         return;
 *
 *     server::SharedPV mypv(...);
 *     ioc::server()
 *           .addPV("my:pv:name", mypv);
 * }
 * static void myregistrar() {
 *     initHookRegister(&myinitHook);
 * }
 * extern "C" {
 *     epicsExportRegistrar(myregistrar); // needs matching entry in .dbd
 * }
 * @endcode
 */
PVXS_IOC_API
server::Server server();

void runOnServer(const std::function <void (server::Server *)>& function, const char *method = nullptr, const char *context = nullptr);

}} // namespace pvxs::ioc

/**
 * Run given lambda function against the provided pvxs server instance
 * @param _lambda the lambda function to run against the provided pvxs server instance
 */
#define runOnPvxsServer(_lambda) runOnServer(_lambda, __func__)

/**
 * Run given lambda function against the provided pvxs server instance with the given context string
 * @param _context the context string, used in error reporting.  e.g. "Updating tables"
 * @param _lambda the lambda function to run against the provided pvxs server instance
 */
#define runOnPvxsServerWhile_(_context, _lambda) runOnServer(_lambda, __func__, _context)

#endif // PVXS_IOCHOOKS_H
