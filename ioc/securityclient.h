/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_SECURITYCLIENT_H
#define PVXS_SECURITYCLIENT_H

#include <vector>
#include <asLib.h>
#include <dbChannel.h>

#include "credentials.h"

namespace pvxs {
namespace ioc {

class SecurityClient {
public:
	std::vector<ASCLIENTPVT> cli;
	~SecurityClient();
	void update(dbChannel* ch, Credentials& cred);
	bool canWrite() const;
};

struct GroupSecurityCache {
	bool done = false;
	std::vector<SecurityClient> securityClients;
	std::unique_ptr<Credentials> credentials;
};

struct SecurityCache {
	bool done = false;
	SecurityClient securityClient;
	std::unique_ptr<Credentials> credentials;
};

} // pvxs
} // ioc

#endif //PVXS_SECURITYCLIENT_H
