/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPVCHANNEL_H
#define PVXS_GROUPPVCHANNEL_H

#include <vector>
#include <dbChannel.h>
#include "grouppvchannelfield.h"

namespace pvxs {
namespace ioc {

/**
 * A Group PV Channel.
 * Each Group PV is made up of a set of Group PV channels which are related to distinct dbChannels.
 * This class encapsulates each Group PV channel and its relation to its dbChannel.
 * It contains all the group fields that map to fields in the related dbChannel.
 */
class GroupPvChannel {
public:
	// The related dbChannel
	std::shared_ptr<dbChannel> pdbChannel;
	// The fields that map to fields in the related dbChannel
	GroupPvChannelFields fields;
	GroupPvChannel() = default;

	/**
	 * To show the detail of the group fields related to this dbChannel
	 */
	void showFields() const;
};

// A list of GroupPvChannels found in a GroupPv
typedef std::shared_ptr<std::vector<GroupPvChannel>> GroupPvChannels;

} // pvxs
} // ioc

#endif //PVXS_GROUPPVCHANNEL_H
