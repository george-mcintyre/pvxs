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

using namespace pvxs;

namespace pvxs {
namespace ioc {

std::atomic<server::Server*> instance{};

DEFINE_LOGGER(log, "pvxs.ioc");

void pvxsl(int detail) {
	try {
		if (auto serv = instance.load()) {
			for (auto& pair: serv->listSource()) {
				auto src = serv->getSource(pair.first, pair.second);
				if (!src)
					continue; // race?

				auto list = src->onList();

				if (detail > 0)
					printf("# Source %s@%d%s\n",
							pair.first.c_str(), pair.second,
							list.dynamic ? " [dynamic]" : "");

				if (!list.names) {
					if (detail > 0)
						printf("# no PVs\n");
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
		if (auto serv = instance.load()) {
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
		if (auto serv = instance.load()) {
			if (instance.compare_exchange_strong(serv, nullptr)) {
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
			if (auto serv = instance.load()) {
				serv->start();
				log_debug_printf(log, "Started Server %p", serv);
			}
		}
		if (state == initHookAfterCaServerPaused) {
			if (auto serv = instance.load()) {
				serv->stop();
				log_debug_printf(log, "Stopped Server %p", serv);
			}
		}
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}

server::Server server() {
	if (auto serv = instance.load()) {
		return *serv;
	} else {
		throw std::logic_error("No Instance");
	}
}

}
} // namespace pvxs::ioc

namespace {
void pvxsRegistrar() {
	try {
		pvxs::logger_config_env();

		pvxs::ioc::IOCShCommand<int>("pvxsl", "detail").implementation<&pvxs::ioc::pvxsl>();
		pvxs::ioc::IOCShCommand<int>("pvxsr", "detail").implementation<&pvxs::ioc::pvxsr>();
		pvxs::ioc::IOCShCommand<>("pvxs_target_info").implementation<&pvxs::ioc::pvxs_target_info>();

		auto serv = pvxs::ioc::instance.load();
		if (!serv) {
			std::unique_ptr<server::Server> temp(new server::Server(server::Config::from_env()));

			if (pvxs::ioc::instance.compare_exchange_strong(serv, temp.get())) {
				log_debug_printf(pvxs::ioc::log, "Installing Server %p\n", temp.get());
				temp.release();
			} else {
				log_crit_printf(pvxs::ioc::log, "Race installing Server? %p\n", serv);
			}
		} else {
			log_err_printf(pvxs::ioc::log, "Stale Server? %p\n", serv);
		}

		initHookRegister(&pvxs::ioc::pvxsInitHook);
	} catch (std::exception& e) {
		fprintf(stderr, "Error in %s : %s\n", __func__, e.what());
	}
}
} // namespace

extern "C" {
epicsExportRegistrar(pvxsRegistrar);
}
