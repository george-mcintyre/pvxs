/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

// Created on 03/01/2023.
//

#include <pvxs/source.h>
#include <pvxs/log.h>

#include <dbEvent.h>
#include <dbChannel.h>


#include "dbentry.h"
#include "dberrmsg.h"
#include "groupsource.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.search");

/**
 * Constructor for GroupSource registrar.
 */
GroupSource::GroupSource()
		:eventContext(db_init_events()) // Initialise event context
{
	auto names(std::make_shared<std::set<std::string >>());

	//  For each group record type and for each record in that type, add record name to the list of all records
	// TODO Implement
	DBEntry db;
/*
	for (long status = dbFirstRecordType(db); !status; status = dbNextRecordType(db)) {
		for (status = dbFirstRecord(db); !status; status = dbNextRecord(db)) {
			names->insert(db->precnode->recordname);
		}
	}
*/

	allRecords.names = names;

	// Start event pump
	if ( !eventContext) {
		throw std::runtime_error("Group Source: Event Context failed to initialise: db_init_events()");
	}

	if ( db_start_events(eventContext.get(), "qsrvGroup", nullptr, nullptr, epicsThreadPriorityCAServerLow-1) ) {
		throw std::runtime_error("Could not start event thread: db_start_events()");
	}
}

/**
 * Handle the create source operation.  This is called once when the source is created.
 * We will register all of the database records that have been loaded until this time as pv names in this
 * source.
 * @param channelControl
 */
void GroupSource::onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) {
	auto sourceName(channelControl->name().c_str());
	dbChannel* pdbChannel = dbChannelCreate(sourceName);
	if (!pdbChannel) {
		log_debug_printf(_logname, "Ignore requested source '%s'\n", sourceName);
		return;
	}
	log_debug_printf(_logname, "Accepting channel for '%s'\n", sourceName);

	// Set up a shared pointer to the database channel and provide a deleter lambda for when it will eventually be deleted
	std::shared_ptr<dbChannel> pChannel(pdbChannel, [](dbChannel* ch) { dbChannelDelete(ch); });

	if (DBErrMsg err = dbChannelOpen(pChannel.get())) {
		log_debug_printf(_logname, "Error opening database channel for '%s: %s'\n", sourceName, err.c_str());
		throw std::runtime_error(err.c_str());
	}

	// Create callbacks for handling requests and channel subscriptions
	createRequestAndSubscriptionHandlers(channelControl, pChannel);
}

GroupSource::List GroupSource::onList() {
	return allRecords;
}

/**
 * Respond to search requests.  For each matching pv, claim that pv
 *
 * @param searchOperation the search operation
 */
void GroupSource::onSearch(Search& searchOperation) {
	for (auto& pv: searchOperation) {
		if (!dbChannelTest(pv.name())) {
			pv.claim();
			log_debug_printf(_logname, "Claiming '%s'\n", pv.name());
		}
	}
}

/**
 * Respond to the show request by displaying a list of all the PVs hosted in this ioc
 *
 * @param outputStream the stream to show the list on
 */
void GroupSource::show(std::ostream& outputStream) {
	outputStream << "IOC";
	for (auto& name: *GroupSource::allRecords.names) {
		outputStream << "\n" << indent{} << name;
	}
}

/**
 * Create request and subscription handlers for group record sources
 *
 * @param channelControl the control channel pointer that we got from onCreate
 * @param pChannel the pointer to the database channel to set up the handlers for
 */
void GroupSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& putOperation,
		const std::shared_ptr<dbChannel>& pChannel) {

}

} // ioc
} // pvxs
