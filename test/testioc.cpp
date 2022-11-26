/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <testMain.h>
#include <dbUnitTest.h>
#include <dbAccess.h>
#include <iocsh.h>

#include <pvxs/iochooks.h>
#include <pvxs/server.h>
#include <pvxs/unittest.h>

#include "osiFileName.h"

#ifndef EPICS_BASE
#define EPICS_BASE ""
#error -DEPICS_BASE=<path-to-epics-base> required while building this test
#endif

extern "C" {
extern int testioc_registerRecordDeviceDriver(struct dbBase*);
}

using namespace pvxs;

namespace {

} // namespace

MAIN(testioc)
{
    testPlan(9);
    testSetup();

    testdbPrepare();

    testThrows<std::logic_error>([]{
        ioc::server();
    });

	// Test loading configuration file
    testdbReadDatabase("testioc.dbd", nullptr, nullptr);
	testEq(0, testioc_registerRecordDeviceDriver(pdbbase));
	testEq(0, iocshCmd("pvxsr"));
	testEq(0, iocshCmd("pvxs_target_info"));

	// Loading database
	testdbReadDatabase(EPICS_BASE OSI_PATH_SEPARATOR "test" OSI_PATH_SEPARATOR "testioc.db", nullptr, "user=test");

	// Test starting server
    testTrue(!!ioc::server());
    testIocInitOk();

	// Test fields loaded ok
	testEq(0, iocshCmd("dbl"));
	testEq(0, iocshCmd("dbgf test:aiExample"));
	testEq(0, iocshCmd("dbgf test:calcExample"));
	testEq(0, iocshCmd("dbgf test:compressExample"));

	// Test shutdown
	testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
