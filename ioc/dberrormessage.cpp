/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <errlog.h>
#include <errSymTbl.h>
#include "dberrormessage.h"

namespace pvxs {
namespace ioc {

/**
 * Construct a new DBErrorMessage from a native database command status code
 *
 * @param dbStatus database command status code
 */
DBErrorMessage::DBErrorMessage(const long& dbStatus) {
	(*this) = dbStatus;
}

/**
 * Set value of this DBErrorMessage object from the specified database status code
 *
 * @param dbStatus database command status code
 * @return updated  DBErrorMessage object
 */
DBErrorMessage& DBErrorMessage::operator=(const long& dbStatus) {
	status = dbStatus;
	if (!dbStatus) {
		message[0] = '\0';
	} else {
		errSymLookup(dbStatus, message, sizeof(message));
		message[sizeof(message) - 1] = '\0';
	}
	return *this;
}

/**
 * bool cast operation returns true if the status indicates a failure
 *
 * @return returns true if the status indicates a failure
 */
DBErrorMessage::operator bool() const {
	return status;
}

/**
 * Return the text of the database status as a string pointer
 *
 * @return the text of the database status as a string pointer
 */
const char* DBErrorMessage::c_str() const {
	return message;
}

} // ioc
} // pvxs
