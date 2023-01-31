/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#ifndef PVXS_FIELDSUBSCRIPTIONCTX_H
#define PVXS_FIELDSUBSCRIPTIONCTX_H

#include <map>
#include <pvxs/source.h>
#include "iocgroup.h"
#include "subscriptionctx.h"
#include "dbeventcontextdeleter.h"

namespace pvxs {
namespace ioc {

class GroupSourceSubscriptionCtx;

/**
 * Field subscription context.  This object is the user object that is supplied when one of a group subscription's
 * fields are updated, and their subscription event is triggered.
 *
 * It contains a pointer to the group subscription of which it forms a part, as well as the field it is monitoring.
 */
class FieldSubscriptionCtx : public SubscriptionCtx {
public:
	GroupSourceSubscriptionCtx* pGroupCtx;
	IOCGroupField* field;

	// Map channel to field index in group.fields
	void subscribeField(dbEventCtx eventContext, EVENTFUNC (* subscriptionValueCallback),
			unsigned int selectOptions, bool forValues = true);

	explicit FieldSubscriptionCtx(IOCGroupField& field, GroupSourceSubscriptionCtx* groupSourceSubscriptionCtx);
	FieldSubscriptionCtx(FieldSubscriptionCtx&&) = default;

	FieldSubscriptionCtx(const FieldSubscriptionCtx&) = delete;
};

} // pvcs
} // ioc

#endif //PVXS_FIELDSUBSCRIPTIONCTX_H
