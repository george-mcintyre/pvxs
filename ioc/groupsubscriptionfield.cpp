/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "groupsubscriptionfield.h"

namespace pvxs {
namespace ioc {
GroupSubscriptionField::GroupSubscriptionField(IOCGroupField& groupField, bool isValue = true)
		:isValue(isValue), groupField(groupField) {
}
} // pvxs
} // ioc
