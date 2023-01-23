/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "iocgroupchannel.h"
#include "utilpvt.h"

namespace pvxs {
namespace ioc {

/**
 * Construct an empty and uninitialized group channel
 */
IOCGroupChannel::IOCGroupChannel()
		:pDbChannel(nullptr) {
}

/**
 * Construct a group channel from a given dbChannel
 *
 * @param pDbChannel pointer to created db channel
 */
IOCGroupChannel::IOCGroupChannel(dbChannel* pDbChannel)
		:pDbChannel(pDbChannel) {
	if (!pDbChannel) {
		throw std::invalid_argument("attempting to create a group channel with a null dbChannel");
	}
	prepare();

}

/**
 * Construct a group channel from a given db channel name
 *
 * @param name the db channel name
 */
IOCGroupChannel::IOCGroupChannel(const std::string& name)
		:pDbChannel(dbChannelCreate(name.c_str())) {
	if (!pDbChannel) {
		throw std::invalid_argument(SB() << "invalid group channel name: " << name);
	}
	prepare();
}

/**
 * Destroy the group channel and clean up the dbChannel resource
 */
IOCGroupChannel::~IOCGroupChannel() {
	if (pDbChannel) {
		dbChannelDelete(pDbChannel);
		pDbChannel = nullptr;
	}
}

/**
 * Internal function to prepare the dbChannel for operation by opening it
 */
void IOCGroupChannel::prepare() {
	if (!pDbChannel) {
		throw std::invalid_argument(SB() << "NULL channel while opening group channel");
	}
	if (dbChannelOpen(pDbChannel)) {
		dbChannelDelete(pDbChannel);
		throw std::invalid_argument(SB() << "Failed to open group channel " << dbChannelName(pDbChannel));
	}
}

/**
 * Exchange the given dbChannel for the dbChannel associated with this group channel
 * @param otherDbChannel
 */
void IOCGroupChannel::swap(IOCGroupChannel& otherDbChannel) {
	std::swap(pDbChannel, otherDbChannel.pDbChannel);
}

/**
 * Cast to a dbChannel pointer
 * @return pointer to the dbChannel associated with this group channel
 */
IOCGroupChannel::operator dbChannel*() {
	return pDbChannel;
}

/**
 * Cast to a const dbChannel pointer
 * @return pointer to the dbChannel associated with this group channel
 */
IOCGroupChannel::operator const dbChannel*() const {
	return pDbChannel;
}

/**
 * Pointer indirection operator
 * @return pointer to the dbChannel associated with this group channel
 */
dbChannel* IOCGroupChannel::operator->() {
	return pDbChannel;
}

/**
 * Const pointer indirection operator
 * @return pointer to the dbChannel associated with this group channel
 */
const dbChannel* IOCGroupChannel::operator->() const {
	return pDbChannel;
}

} // pvxs
} // ioc
