/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <iostream>

#include <pvxs/source.h>
#include <initHooks.h>
#include <epicsExport.h>
#include <epicsString.h>

#include "iocshcommand.h"
#include "groupsource.h"
#include "grouppv.h"
#include "groupconfigprocessor.h"

namespace pvxs {
namespace ioc {

/**
 * IOC command wrapper for dbLoadGroup function
 *
 * @param jsonFileName
 */
void dbLoadGroupCmd(const char* jsonFileName) {
	(void)dbLoadGroup(jsonFileName);
}

/**
 * List group db record/field names that are registered with the pvxs IOC server.
 * With no arguments this will list all the group record names.
 *
 * @param level optional depth to show details for
 * @param pattern optionally only show records matching the regex pattern
 */
void pvxsgl(int level, const char* pattern) {
	runOnPvxsServer(([level, &pattern](IOCServer* pPvxsServer) {
		try {
			// Default pattern to match everything
			if (!pattern) {
				pattern = "";
			}

			// Get copy of Group PV Map
			GroupPvMap map;
			{
				epicsGuard<epicsMutex> G(pPvxsServer->pvMapMutex);
				map = pPvxsServer->groupPvMap;  // copy map
			}

			// For each group
			for (GroupPvMap::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
				// if no pattern specified or the pattern matches
				if (!pattern[0] || !!epicsStrGlobMatch(it->first.c_str(), pattern)) {
					// Print the group name
					std::cout << it->first << std::endl;
					// print sub-levels if required
					if (level > 0) {
						it->second->show(level);
					}
				}
			}
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}));
}

/**
 * Load JSON group definition file.
 * This function does not actually parse the given file, but adds it to the list of files to be loaded,
 * at the appropriate time in the startup process.
 *
* @param jsonFilename the json file containing the group definitions.  If filename is a dash or a dash then star, the list of
 * files is cleared. If it starts with a dash followed by a filename then file is removed from the list.  Otherwise
 * the filename is added to the list of files to be loaded.
 * @return 0 for success, 1 for failure
 */
long dbLoadGroup(const char* jsonFilename) {
	try {
		if (!jsonFilename || !jsonFilename[0]) {
			std::cout << "dbLoadGroup(\"file.json\")" << std::endl
			          << "Load additional DB group definitions from file." << std::endl;
			std::cerr << "Missing filename" << std::endl;
			return 1;
		}

		runOnPvxsServer([&jsonFilename](IOCServer* pPvxsServer) {
			if (jsonFilename[0] == '-') {
				jsonFilename++;
				if (jsonFilename[0] == '*' && jsonFilename[1] == '\0') {
					pPvxsServer->groupDefinitionFiles.clear();
				} else {
					pPvxsServer->groupDefinitionFiles.remove(jsonFilename);
				}
			} else {
				pPvxsServer->groupDefinitionFiles.remove(jsonFilename);
				pPvxsServer->groupDefinitionFiles.emplace_back(jsonFilename);
			}
		});
		return 0;
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
}

}
} // namespace pvxs::ioc

using namespace pvxs::ioc;

namespace {
using namespace pvxs;

/**
 * Initialise qsrv database group records by adding them as sources in our running pvxs server instance
 *
 * @param theInitHookState the initHook state - we only want to trigger on the initHookAfterIocBuilt state - ignore all others
 */
void qsrvGroupSourceInit(initHookState theInitHookState) {
	if (theInitHookState == initHookAfterInitDatabase) {
		GroupConfigProcessor processor;
		// Parse all info(Q:Group... records to configure groups
		processor.parseDbConfig();

		// Load group configuration files
		processor.parseConfigFiles();

	} else if (theInitHookState == initHookAfterIocBuilt) {
		// Load group configuration from parsed groups in iocServer
		pvxs::ioc::iocServer().addSource("qsrv", std::make_shared<pvxs::ioc::GroupSource>(), 1);
	}
}

/**
 * IOC pvxs Group Source registrar.  This implements the required registrar function that is called by xxxx_registerRecordDeviceDriver,
 * the auto-generated stub created for all IOC implementations.
 *<p>
 * It is registered by using the `epicsExportRegistrar()` macro.
 *<p>
 * 1. Register your hook handler to handle any state hooks that you want to implement.  Here we install
 * an `initHookState` handler connected to the `initHookAfterIocBuilt` state.  It  will add all of the
 * group record type sources defined so far.  Note that you can define sources up until the `iocInit()` call,
 * after which point the `initHookAfterIocBuilt` handlers are called and will register all the defined records.
 */
void pvxsGroupSourceRegistrar() {
	// Register commands to be available in the IOC shell
	IOCShCommand<int, const char*>("pvxsgl", "[level, [pattern]]", "Group Sources list.\n"
	                                                               "List record/field names.\n"
	                                                               "If `level` is set then show only down to that level.\n"
	                                                               "If `pattern` is set then show records that match the pattern.")
			.implementation<&pvxsgl>();

	IOCShCommand<const char*>("dbLoadGroup", "jsonDefinitionFile", "Load Group Record Definition from given file.\n"
	                                                               "'-' or '-*' to remove previous files.\n"
	                                                               "'-<jsonDefinitionFile>' to remove the file from the list.\n"
	                                                               "otherwise add the file to the list of files to load.\n")
			.implementation<&dbLoadGroupCmd>();

	initHookRegister(&qsrvGroupSourceInit);
}

} // namespace

// in .dbd file
//registrar(pvxsGroupSourceRegistrar)
extern "C" {
epicsExportRegistrar(pvxsGroupSourceRegistrar);
}
