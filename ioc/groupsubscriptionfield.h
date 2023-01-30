/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPSUBSCRIPTIONFIELD_H
#define PVXS_GROUPSUBSCRIPTIONFIELD_H

#include "iocgroupfield.h"
namespace pvxs {
namespace ioc {

class GroupSubscriptionField {
public:
	bool isValue{ true };   // if this field uses the value channel (default), false uses property channel
	IOCGroupField& groupField;

	GroupSubscriptionField(IOCGroupField& groupField, bool isValue);
};

} // pvxs
} // ioc

#endif //PVXS_GROUPSUBSCRIPTIONFIELD_H
