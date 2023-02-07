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
#include <iocInit.h>

#include "osiFileName.h"
#include "pvxs/client.h"
#include "utilpvt.h"

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

static dbEventCtx testEvtCtx;

static void pvxsTestIocInitOk()
{
	if(iocBuild() || iocRun())
		testAbort("Failed to start up test database");
	if(!(testEvtCtx=db_init_events()))
		testAbort("Failed to initialize test dbEvent context");
	if(DB_EVENT_OK!=db_start_events(testEvtCtx, "CAS-test", NULL, NULL, epicsThreadPriorityCAServerLow))
		testAbort("Failed to start test dbEvent context");
}

static void pvxsTestIocShutdownOk()
{
	db_close_events(testEvtCtx);
	testEvtCtx = NULL;
	if(iocShutdown())
		testAbort("Failed to shutdown test database");
}

MAIN(testioc) {
	testPlan(22);
	testSetup();

	testdbPrepare();

	testThrows<std::logic_error>([] {
		ioc::server();
	});

	// Test (1) loading configuration file
	testDiag("dbLoadDatabase(\"testioc.dbd\")");
	testdbReadDatabase("testioc.dbd", nullptr, nullptr);
	// Test (2)
	testEq(0, testioc_registerRecordDeviceDriver(pdbbase));
	// Test (3)
	testEq(0, iocshCmd("pvxsr"));
	// Test (4)
	testEq(0, iocshCmd("pvxsi"));

	// Loading database
	testdbReadDatabase(EPICS_BASE OSI_PATH_SEPARATOR "test" OSI_PATH_SEPARATOR "testioc.db", nullptr, "user=test");

	// (5) Test starting server
	testTrue((bool)ioc::server());

	// (5)
	pvxsTestIocInitOk();

	// Test (6) fields loaded ok
	testEq(0, iocshCmd("dbl"));

	// Test (7-11) ability to get fields via shell
	testEq(0, iocshCmd("dbgf test:aiExample"));
	testEq(0, iocshCmd("dbgf test:calcExample"));
	testEq(0, iocshCmd("dbgf test:compressExample"));
	testEq(0, iocshCmd("dbgf test:stringExample"));
	testEq(0, iocshCmd("dbgf test:arrayExample"));

	// Test (12-15) value of fields correct
	testdbGetFieldEqual("test:aiExample", DBR_DOUBLE, 42.2);
	testdbGetFieldEqual("test:compressExample", DBR_DOUBLE, 42.2);
	testdbGetFieldEqual("test:stringExample", DBR_STRING, "Some random value");
	double expected = 0.0;
	testdbGetArrFieldEqual("test:arrayExample", DBR_DOUBLE, 0, 0, &expected);

	// Get a client config to connect to server for network testing
	client::Context cli(ioc::server().clientConfig().build());

	// Test (16) client access to ioc
	auto val = cli.get("test:aiExample").exec()->wait(5.0);
	auto aiExample = val["value"].as<double>();
	testEq(42.2, aiExample);

	// Test (17)
	val = cli.get("test:calcExample").exec()->wait(5.0);
	auto calcExample = val["value"].as<double>();
	testEq(0.0, calcExample);

	// Set test (18) values into array
	shared_array<double> expectedArray({ 1.0, 2.0, 3.0 });
	val = cli.get("test:arrayExample").exec()->wait(5.0);
	auto actualArray = val["value"].as<shared_array<const double>>();
	testArrEq(expectedArray, actualArray);

	shared_array<double> expectedArray2({ 1.0, 2.0, 3.0, 4.0, 5.0 });
	// (19)
	testdbPutArrFieldOk("test:arrayExample", DBR_DOUBLE, expectedArray2.size(), expectedArray2.data());
	val = cli.get("test:arrayExample").exec()->wait(5.0);
	actualArray = val["value"].as<shared_array<const double>>();
	// (20)
	testArrEq(expectedArray2, actualArray);


//	val = cli.get("test:compressExample").exec()->wait(5.0);
//	auto compressExample = val["value"].as<double>();
//	testEq(42.2, compressExample);

	// (21)
	val = cli.get("test:longExample").exec()->wait(5.0);
	auto longValue = val["value"].as<long>();
	testEq(102042, longValue);

	// (22)
	val = cli.get("test:stringExample").exec()->wait(5.0);
	auto testString = val["value"];
	testStrEq(std::string(SB() << testString), "string = \"Some random value\"\n");

	//	cli.put();

	// Test shutdown
	pvxsTestIocShutdownOk();
	testdbCleanup();

	return testDone();
}
