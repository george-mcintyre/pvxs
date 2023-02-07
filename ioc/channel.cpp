/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include <iostream>
#include <string>

#include "channel.h"
#include "utilpvt.h"

namespace pvxs {
namespace ioc {
/**
 * Construct a group channel from a given db channel name
 *
 * @param name the db channel name
 */
Channel::Channel(const std::string& name)
		:pDbChannel(std::shared_ptr<dbChannel>(dbChannelCreate(name.c_str()),
		[](dbChannel* ch) {
			if (ch) {
				dbChannelDelete(ch);
			}
		})) {
	if (!pDbChannel) {
		throw std::invalid_argument(SB() << "invalid group channel name: " << name);
	}
	prepare();
}

/**
 * Move constructor
 *
 * @param other other IOCGroupChannel
 */
Channel::Channel(Channel&& other) noexcept
		:pDbChannel(std::move(other.pDbChannel)) {
}

/**
 * Destructor is default because pDbChannel cleans up after itself.
 */
Channel::~Channel() = default;

/**
 * Internal function to prepare the dbChannel for operation by opening it
 */
void Channel::prepare() {
	if (!pDbChannel) {
		throw std::invalid_argument(SB() << "NULL channel while opening group channel");
	}
	if (dbChannelOpen(pDbChannel.get())) {
		throw std::invalid_argument(SB() << "Failed to open group channel " << dbChannelName(pDbChannel));
	}
}

/**
 * Cast as a shared pointer to a dbChannel.  This returns the pDbChannel member
 *
 * @return the pDbChannel member
 */
Channel::operator dbChannel*() const {
	return pDbChannel.get();
}

/**
 * Const pointer indirection operator
 * @return pointer to the dbChannel associated with this group channel
 */
const dbChannel* Channel::operator->() const {
	return pDbChannel.get();
}

} // pvxs
} // ioc
