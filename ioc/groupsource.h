/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 03/01/2023.
//

#ifndef PVXS_GROUPSOURCE_H
#define PVXS_GROUPSOURCE_H

#include "dbeventcontextdeleter.h"

namespace pvxs {
namespace ioc {

class GroupSource : public server::Source {
public:
	// List of all database records that this single source serves
	List allRecords;

	// The event context for all subscriptions
	std::unique_ptr<std::remove_pointer<dbEventCtx>::type, DBEventContextDeleter> eventContext;

	GroupSource();
	void onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) final;
	List onList() final;
	void onSearch(Search& searchOperation) final;
	void show(std::ostream& outputStream) final;

private:
	// Create request and subscription handlers for single record sources
	void createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& putOperation,
			const std::shared_ptr<dbChannel>& pChannel);

};

} // ioc
} // pvxs


#endif //PVXS_GROUPSOURCE_H
