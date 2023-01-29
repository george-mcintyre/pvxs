/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPSOURCE_H
#define PVXS_GROUPSOURCE_H

#include "dbeventcontextdeleter.h"
#include "iocserver.h"

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
	static void
	createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl, IOCGroup& group);

	static void get(IOCGroup& group, std::unique_ptr<server::ExecOp>& getOperation);
	static void groupGet(IOCGroup& group, const std::function<void(Value&)>& returnFn,
			const std::function<void(const char*)>& errorFn);
	static void onOp(IOCGroup& group, std::unique_ptr<server::ConnectOp>&& channelConnectOperation);
	static void putGroup(IOCGroup& group, std::unique_ptr<server::ExecOp>& putOperation, const Value& value);
};

} // ioc
} // pvxs


#endif //PVXS_GROUPSOURCE_H
