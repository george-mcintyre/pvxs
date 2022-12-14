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

MAIN(testioc) {
	testPlan(19);
	testSetup();

	testdbPrepare();

	testThrows<std::logic_error>([] {
		ioc::server();
	});

	// Test loading configuration file
	testDiag("dbLoadDatabase(\"testioc.dbd\")");
	testdbReadDatabase("testioc.dbd", nullptr, nullptr);
	testEq(0, testioc_registerRecordDeviceDriver(pdbbase));
	testEq(0, iocshCmd("pvxsr"));
	testEq(0, iocshCmd("pvxsi"));

	// Loading database
	testdbReadDatabase(EPICS_BASE OSI_PATH_SEPARATOR "test" OSI_PATH_SEPARATOR "testioc.db", nullptr, "user=test");

	// Test starting server
	testTrue(!!ioc::server());

	testIocInitNonIsolatedOk();

	// Test fields loaded ok
	testEq(0, iocshCmd("dbl"));

	// Test ability to get fields via shell
	testEq(0, iocshCmd("dbgf test:aiExample"));
	testEq(0, iocshCmd("dbgf test:calcExample"));
	testEq(0, iocshCmd("dbgf test:compressExample"));
	testEq(0, iocshCmd("dbgf test:string"));
	testEq(0, iocshCmd("dbgf test:array"));

	// Test value of fields correct
	testdbGetFieldEqual("test:aiExample", DBR_DOUBLE, 0);
	testdbGetFieldEqual("test:calcExample", DBR_DOUBLE, 0);
	testdbGetFieldEqual("test:compressExample", DBR_DOUBLE, 0);
	testdbGetFieldEqual("test:string", DBR_STRING, "Some random value");
	double expected =0.0;
	testdbGetArrFieldEqual("test:array", DBR_DOUBLE, 0, 0, &expected);

	// Get a client config to connect to server for network testing
	client::Context cli(ioc::server().clientConfig().build());

	// Test client access to ioc
	auto val = cli.get("test:aiExample").exec()->wait(5.0);
	testEq(0, val["value"].as<double>());

	val = cli.get("test:calcExample").exec()->wait(5.0);
	testEq(0, val["value"].as<double>());

//	val = cli.get("test:compressExample").exec()->wait(5.0);
//	testEq(0, val["value"].as<double>());

	val = cli.get("test:string").exec()->wait(5.0);
	testStrEq(std::string(SB()<<val["value"]), "string = \"Some random value\"\n");

	//	cli.put();

	// Test shutdown
	testIocShutdownOk();
	testdbCleanup();

	return testDone();
}
