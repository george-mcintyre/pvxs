
/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_CHANNEL_H
#define PVXS_CHANNEL_H

#include <string>
#include <memory>

#include <dbChannel.h>

namespace pvxs {
namespace ioc {

/**
 * This class encapsulates a shared pointer to a dbChannel but provides constructors
 * from string and dbChannel to make its use simpler.  It can be used wherever a dbChannel is used.
 * As a bonus when constructed with parameters it provides an already open dbChannel.
 */
class Channel {
private:
	std::shared_ptr<dbChannel> pDbChannel;
	void prepare();

public:
	// This constructor calls dbChannelOpen()
	explicit Channel(const std::string& name);
	~Channel();

	// Casting and indirection
	explicit operator dbChannel*() const;
	const dbChannel* operator->() const;

	// Disallowed methods.  Copy and move constructors
	Channel(const Channel&) = delete;
	Channel(Channel&&) noexcept;
	const std::shared_ptr<dbChannel>& shared_ptr() const {
		return pDbChannel;
	};
};

} // pvxs
} // ioc

#endif //PVXS_CHANNEL_H
