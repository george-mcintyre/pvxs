/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBERRORMESSAGE_H
#define PVXS_DBERRORMESSAGE_H

#include <epicsTypes.h>

namespace pvxs {
namespace ioc {

/**
 * Wrapper class for status returned from base IOC database commands.
 */
class DBErrorMessage {
	long status = 0;
	char message[MAX_STRING_SIZE]{};
public:
	explicit DBErrorMessage(const long& dbStatus = 0);
	DBErrorMessage& operator=(const long& dbStatus);
	explicit operator bool() const;
	const char* c_str() const;
};

} // ioc
} // pvxs
#endif //PVXS_DBERRORMESSAGE_H
