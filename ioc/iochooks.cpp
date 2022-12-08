/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <atomic>
#include <memory>
#include <stdexcept>

#include <pvxs/log.h>
#include <pvxs/server.h>
#include <pvxs/source.h>
#include <pvxs/iochooks.h>

#include <iocsh.h>
#include <initHooks.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include "pvxs/iocshcommand.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(log, "pvxs.ioc");

// The pvxs server singleton
std::atomic<server::Server*> pvxsServer{};

/**
 * Get the pvxs server instance
 *
 * @return the pvxs server instance
 */
server::Server server() {
	if (auto serv = pvxsServer.load()) {
		return *serv;
	} else {
		throw std::logic_error("No Instance");
	}
}

void pvxsl(int detail) {
	try {
		if (auto serv = pvxsServer.load()) {
			for (auto& pair: serv->listSource()) {
				auto src = serv->getSource(pair.first, pair.second);
				if (!src) { // race?
					continue;
				}

				auto list = src->onList();

				if (detail > 0) {
					printf("# Source %s@%d%s\n",
							pair.first.c_str(), pair.second,
							list.dynamic ? " [dynamic]" : "");
				}

				if (!list.names) {
					if (detail > 0) {
						printf("# no PVs\n");
					}
				} else {
					for (auto& name: *list.names) {
						printf("%s\n", name.c_str());
					}
				}
			}
		}
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

void pvxsr(int detail) {
	try {
		if (auto serv = pvxsServer.load()) {
			std::ostringstream strm;
			Detailed D(strm, detail);
			strm << *serv;
			printf("%s", strm.str().c_str());
		}
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

void pvxs_target_info() {
	try {
		std::ostringstream capture;
		target_information(capture);
		printf("%s", capture.str().c_str());
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

void pvxsAtExit(void* unused) {
	try {
		if (auto serv = pvxsServer.load()) {
			if (pvxsServer.compare_exchange_strong(serv, nullptr)) {
				// take ownership
				std::unique_ptr<server::Server> trash(serv);
				trash->stop();
				log_debug_printf(log, "Stopped Server?%s", "\n");
			}
		}
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

void pvxsInitHook(initHookState state) {
	try {
		// iocBuild()
		if (state == initHookAfterInitDatabase) {
			// we want to run before exitDatabase
			epicsAtExit(&pvxsAtExit, nullptr);
		}
		// iocRun()/iocPause()
		if (state == initHookAfterCaServerRunning) {
			if (auto serv = pvxsServer.load()) {
				serv->start();
				log_debug_printf(log, "Started Server %p", serv);
			}
		}
		if (state == initHookAfterCaServerPaused) {
			if (auto serv = pvxsServer.load()) {
				serv->stop();
				log_debug_printf(log, "Stopped Server %p", serv);
			}
		}
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

}
} // namespace pvxs::ioc


using namespace pvxs::ioc;

namespace {

// Initialise the pvxs server instance
void initialisePvxsServer();

// Register callback functions to be used in the IOC shell and also during initialization.
void pvxsRegistrar();

/**
 * Create the pvxs server instance.  We use the global pvxsServer atomic
 */
void initialisePvxsServer() {
	auto serv = pvxsServer.load();
	if (!serv) {
		std::unique_ptr<pvxs::server::Server> temp(new pvxs::server::Server(pvxs::server::Config::from_env()));

		if (pvxsServer.compare_exchange_strong(serv, temp.get())) {
			log_debug_printf(log, "Installing Server %p\n", temp.get());
			temp.release();
		} else {
			log_crit_printf(log, "Race installing Server? %p\n", serv);
		}
	} else {
		log_err_printf(log, "Stale Server? %p\n", serv);
	}
}

/**
 * IOC registrar.  This implements the required registrar function that is called by xxxx_registerRecordDeviceDriver,
 * the auto-generated stub created for all IOC implementations.
 *
 * It is registered by using the `epicsExportRegistrar()` macro.
 *
 * 1. Specify here all of the commands that you want to be registered and available in the IOC shell.
 * 2. Also make sure that you initialize your server implementation - PVXS in our case - so that it will be available for the shell.
 * 3. Lastly register your hook handler to handle any state hooks that you want to implement
 */
void pvxsRegistrar() {
	try {
		pvxs::logger_config_env();

		// Register commands to be available in the IOC shell
		IOCShCommand<int>("pvxsl", "detail").implementation<&pvxsl>();
		IOCShCommand<int>("pvxsr", "detail").implementation<&pvxsr>();
		IOCShCommand<>("pvxs_target_info").implementation<&pvxs_target_info>();

		// Initialise the PVXS Server
		initialisePvxsServer();

		// Register our hook handler to intercept certain state changes
		initHookRegister(&pvxsInitHook);
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}
} // namespace

extern "C" {
epicsExportRegistrar(pvxsRegistrar);
}
