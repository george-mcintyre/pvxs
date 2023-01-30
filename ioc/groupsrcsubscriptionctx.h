/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPSRCSUBSCRIPTIONCTX_H
#define PVXS_GROUPSRCSUBSCRIPTIONCTX_H

#include <map>
#include <pvxs/source.h>
#include "iocgroup.h"
#include "subscriptionctx.h"
#include "dbeventcontextdeleter.h"

namespace pvxs {
namespace ioc {

class GroupSourceSubscriptionCtx : public SubscriptionCtx<std::map<dbChannel*,
                                                                   std::pair<std::shared_ptr<dbChannel>,
                                                                             std::shared_ptr<void>>>> {
public:
	const IOCGroup& group;
	std::map<dbChannel*, const IOCGroupField&> fieldMap;

	// Map channel to field index in group.fields
	void subscribeField(dbEventCtx eventContext,
			const IOCGroupField& field,
			void (* subscriptionValueCallback)(void* userArg, dbChannel* pChannel, int eventsRemaining,
					struct db_field_log* pDbFieldLog), unsigned selectOptions, bool forValues = true);

	explicit GroupSourceSubscriptionCtx(const IOCGroup& group);
};

} // pvxs
} // ioc

#endif //PVXS_GROUPSRCSUBSCRIPTIONCTX_H
