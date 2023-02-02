
/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_IOCGROUPCHANNEL_H
#define PVXS_IOCGROUPCHANNEL_H

#include <string>
#include <memory>
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
	std::shared_ptr<dbChannel> pDbChannel;
	void prepare();

public:
	// This constructor calls dbChannelOpen()
	explicit IOCGroupChannel(const std::string& name);
	~IOCGroupChannel();

	// Casting and indirection
	explicit operator dbChannel*() const;
	const dbChannel* operator->() const;

	// Disallowed methods.  Copy and move constructors
	IOCGroupChannel(const IOCGroupChannel&) = delete;
	IOCGroupChannel(IOCGroupChannel&&) noexcept;
	const std::shared_ptr<dbChannel>& shared_ptr() const {
		return pDbChannel;
	};
};

} // pvxs
} // ioc

#endif //PVXS_IOCGROUPCHANNEL_H
