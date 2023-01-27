
/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUPCHANNEL_H
#define PVXS_IOCGROUPCHANNEL_H

#include <dbChannel.h>

namespace pvxs {
namespace ioc {

/**
 * This class encapsulates a dbChannel bt provides constructors from string and dbChannel to
 * make its use simpler.  It can be used wherever a dbChannel is used.
 * As a bonus when constructed with parameters it provides an already open dbChannel.
 */
class IOCGroupChannel {
private:
	dbChannel* pDbChannel;

public:
	IOCGroupChannel();
	IOCGroupChannel(const IOCGroupChannel&) = delete;
	IOCGroupChannel(IOCGroupChannel&&) noexcept;

	// This constructor calls dbChannelOpen()
	explicit IOCGroupChannel(dbChannel* pDbChannel);
	// This constructor calls dbChannelOpen()
	explicit IOCGroupChannel(const std::string& name);

	~IOCGroupChannel();

	void swap(IOCGroupChannel&);

	explicit operator dbChannel*();
	explicit operator const dbChannel*() const;

	dbChannel* operator->();
	const dbChannel* operator->() const;

	void prepare();
};

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPCHANNEL_H
