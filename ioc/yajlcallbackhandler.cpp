/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include "yajlcallbackhandler.h"

namespace pvxs {
namespace ioc {

/**
 * Set the callback handler for the yajl parser
 *
 * @param yajlHandler the allocated handler to set
 */
YajlCallbackHandler::YajlCallbackHandler(yajl_handle yajlHandler)
		:handle(yajlHandler) {
	if (!handle) {
		throw std::runtime_error("Failed to allocate yajl handle");
	}
}

/**
 * Destructor for the callback handler
 */
YajlCallbackHandler::~YajlCallbackHandler() {
	yajl_free(handle);
}

YajlCallbackHandler::operator yajl_handle() {
	return handle;
}

} // pvxs
} // ioc

