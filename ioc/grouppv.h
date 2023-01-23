/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPV_H
#define PVXS_GROUPPV_H

#include <map>
#include <vector>
#include <set>
#include "grouppvfield.h"

namespace pvxs {
namespace ioc {

/**
 * Group pv channel trigger map, maps field name to set of related field it is triggered by
 */
typedef std::map<std::string, Triggers> TriggersMap;

/**
 * Tristate value for status flags
 */
typedef enum {
	Unset,
	True,
	False
} TriState;

/**
 * A Group PV
 * This class represents a group PV.  It contains a set of channels
 * `GroupPvChannels` that link the group to regular db channels.  Each of these channels
 * define fields that are scalar, array or processing placeholders
 */
class GroupPv {
public:
	GroupPv() = default;
	virtual ~GroupPv() = default;

	bool atomicPutGet{ false }, atomicMonitor{ false }, hasTriggers{ false };
	std::string structureId;            // The Normative Type structure ID or any other arbitrary string if not a normative type
	GroupPvFields fields;               // The group's fields
	GroupPvFieldsMap fieldMap;          // The field map, mapping field order
	TriState atomic{ Unset };
	TriggersMap triggerMap;             // The trigger map, mapping fields to related triggering fields

	/**
	 * Show details for this group
	 * @param level the level of detail to show.  0 fields only
	 */
	size_t channelCount() const;
};

// A map of group name to GroupPv
typedef std::map<std::string, GroupPv> GroupPvMap;

} // pvxs
} // ioc

#endif //PVXS_GROUPPV_H
