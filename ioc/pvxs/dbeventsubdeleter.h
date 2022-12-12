/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBEVENTSUBDELETER_H
#define PVXS_DBEVENTSUBDELETER_H

namespace pvxs {
namespace ioc {

class DbEventSubscriptionDeleter {
public:
	void operator()(dbEventSubscription ctxt);
};

}
}

#endif //PVXS_DBEVENTSUBDELETER_H
