/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_YAJLCALLBACKHANDLER_H
#define PVXS_YAJLCALLBACKHANDLER_H

#include <stdexcept>

#include <yajl_parse.h>
namespace pvxs {
namespace ioc {

class YajlCallbackHandler {
	yajl_handle handle;
public:
	explicit YajlCallbackHandler(yajl_handle yajlHandler);
	~YajlCallbackHandler();
	operator yajl_handle(); // NOLINT(google-explicit-constructor)
};

} // pvxs
} // ioc

#endif //PVXS_YAJLCALLBACKHANDLER_H
