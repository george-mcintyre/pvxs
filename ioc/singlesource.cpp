/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <iostream>

#include <pvxs/source.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>
#include <pvxs/singlesource.h>
#include <pvxs/dberrmsg.h>

#include <epicsTypes.h>
#include <dbStaticLib.h>
#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>

#include "pvxs/dbentry.h"
#include "pvxs/typeutils.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.search");

/**
 * Constructor for SingleSource registrar.
 * For each record type and for each record in that type, add record name to the list of all records
 */
SingleSource::SingleSource() {
	auto names(std::make_shared<std::set<std::string >>());

	DBEntry db;
	for (long status = dbFirstRecordType(db); !status; status = dbNextRecordType(db)) {
		for (status = dbFirstRecord(db); !status; status = dbNextRecord(db)) {
			names->insert(db->precnode->recordname);
		}
	}

	allrecords.names = names;
}

/**
 * Handle the create source operation.  This is called once when the source is created.
 * We will register all of the database records that have been loaded until this time as pv names in this
 * source.
 * @param channelControl
 */
void SingleSource::onCreate(std::unique_ptr<server::ChannelControl>&& channelControl) {
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

SingleSource::List SingleSource::onList() {
	return allrecords;
}

/**
 * Respond to search requests.  For each matching pv, claim that pv
 *
 * @param searchOperation the search operation
 */
void SingleSource::onSearch(Search& searchOperation) {
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
void SingleSource::show(std::ostream& outputStream) {
	outputStream << "IOC";
	for (auto& name: *SingleSource::allrecords.names) {
		outputStream << "\n" << indent{} << name;
	}
}

/**
 * Create request and subscription handlers for single record sources
 *
 * @param channelControl the control channel pointer that we got from onCreate
 * @param pChannel the pointer to the database channel to set up the handlers for
 */
void SingleSource::createRequestAndSubscriptionHandlers(std::unique_ptr<server::ChannelControl>& channelControl,
		const std::shared_ptr<dbChannel>& pChannel) {
	auto valueType(getChannelValueType(pChannel));
	Value valuePrototype(nt::NTScalar{ valueType }.create()); // TODO: enable meta

	// Get and Put requests
	channelControl->onOp([pChannel, valuePrototype](std::unique_ptr<server::ConnectOp>&& channelConnectOperation) {
		// First stage for handling any request is to announce the channel type with a `connect()` call
		// @note The type signalled here must match the eventual type returned by a pvxs get
		channelConnectOperation->connect(valuePrototype);

		// pvxs get
		channelConnectOperation->onGet([pChannel, valuePrototype](std::unique_ptr<server::ExecOp>&& getOperation) {
			onGet(pChannel, getOperation, valuePrototype);
		});

		// pvxs put
		channelConnectOperation
				->onPut([pChannel, valuePrototype](std::unique_ptr<server::ExecOp>&& putOperation, Value&& value) {
					onPut(pChannel, putOperation, valuePrototype, value);
				});

	});

	// Subscription requests
	channelControl
			->onSubscribe([pChannel, valuePrototype](std::unique_ptr<server::MonitorSetupOp>&& subscriptionOperation) {
				auto subscriptionControl(subscriptionOperation->connect(valuePrototype));

				// get event control mask
				db_field_log eventControlMask;

//				std::map<epicsEventId, std::vector<dbEventSubscription>> channelMonitor = subscriptions[pChannel];

//				void* eventSubscription = db_add_event(eventContext, pChannel, &subscriptionCallback, channelMonitor, eventControlMask);
				void* eventSubscription(nullptr);

				subscriptionControl->onStart([pChannel](bool isStarting) {
					if (isStarting) {
						onStartSubscription(pChannel);
					} else {
						onDisableSubscription(pChannel);
					}
				});
			});
}

void SingleSource::onStartSubscription(const std::shared_ptr<dbChannel>& pChannel) {
	// db_event_enable()
	// db_post_single_event()
}

void SingleSource::onDisableSubscription(const std::shared_ptr<dbChannel>& pChannel) {
	// db_event_disable()
}

/*
void SingleSource::subscriptionCallback(void* user_arg, const std::shared_ptr<dbChannel>& pChannel,
		int eventsRemaining, struct db_field_log* pfl) {
	epicsMutexMustLock(eventLock);
	std::map<epicsEventId, std::vector<dbEventSubscription>> channelMonitor = subscriptions[pChannel];
	epicsMutexUnlock(eventLock);
//	epicsEventMustTrigger(channelMonitor[user_arg]);
}

*/
TypeCode SingleSource::getChannelValueType(const std::shared_ptr<dbChannel>& pChannel) {
	auto dbChannel(pChannel.get());
	short dbrType(dbChannelFinalFieldType(dbChannel));
	auto nFinalElements(dbChannelFinalElements(dbChannel));
	auto nElements(dbChannelElements(dbChannel));

	TypeCode valueType(TypeCode::Null);

	if (dbChannelFieldType(dbChannel) == DBF_STRING && nElements == 1 && dbrType && nFinalElements > 1) {
		// single DBF_STRING being cast to DBF_CHAR array.  aka long string
		valueType = TypeCode::String;

	} else if (dbrType == DBF_ENUM && nFinalElements == 1) {

	} else {
		// TODO handle links.  For the moment we handle as chars and fail later
		if (dbrType == DBF_INLINK || dbrType == DBF_OUTLINK || dbrType == DBF_FWDLINK) dbrType = DBF_CHAR;

		valueType = toTypeCode(dbfType(dbrType));
		if (valueType != TypeCode::Null && nFinalElements != 1) {
			valueType = valueType.arrayOf();
		}
	}
	return valueType;
}

/**
 * Handle the get operation
 *
 * @param channel the channel that the request comes in on
 * @param getOperation the current executing operation
 * @param valuePrototype a value prototype that is made based on the expected type to be returned
 */
void SingleSource::onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& getOperation,
		const Value& valuePrototype) {
	const char* pvName = channel->name;

	epicsAny valueBuffer[100]; // value buffer to store the field we will get from the database.
	void* pValueBuffer = &valueBuffer[0];
	DBADDR dbAddress;   // Special struct for storing database addresses
	long options = 0;   // For options returned from database read along with data
	long nElements;     // Calculated number of elements to retrieve.  For single values its 1 but for arrays it can be any number

	// Convert pvName to a dbAddress
	if (DBErrMsg err = nameToAddr(pvName, &dbAddress)) {
		getOperation->error(err.c_str());
		return;
	}

	if (dbAddress.precord->lset == nullptr) {
		getOperation->error("pvName no specified in request");
		return;
	}

	// Calculate number of elements to retrieve as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	nElements = MIN(dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

	if (dbAddress.dbr_field_type == DBR_ENUM) {
		onGetEnum(getOperation, valuePrototype, pValueBuffer, dbAddress, options, nElements);
	} else {
		onGetNonEnum(getOperation, valuePrototype, pValueBuffer, dbAddress, options, nElements);
	}
}

void
SingleSource::onGetEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
		DBADDR& dbAddress, long& options, long& nElements) {
	if (DBErrMsg err = dbGetField(&dbAddress, DBR_STRING, pValueBuffer, &options, &nElements, nullptr)) {
		operation->error(err.c_str());
	}

//						prototype = *pbuffer;
	operation->reply(valuePrototype);
}

void
SingleSource::onGetNonEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
		DBADDR& dbAddress, long& options, long& nElements) {// Get the field value
	if (DBErrMsg err = dbGetField(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, &options, &nElements,
			nullptr)) {
		operation->error(err.c_str());
	}

	// Create a placeholder for the return value
	auto value(valuePrototype.cloneEmpty());
	// based on whether it is a scalar or array then call the appropriate setter
	if (nElements == 1) {
		setValue(value, pValueBuffer);
	} else {
		setValue(value, pValueBuffer, nElements);
	}

	value["alarm.severity"] = 0;
	value["alarm.status"] = 0;
	value["alarm.message"] = "";
	auto ts(value["timeStamp"]);
	// TODO: KLUDGE use current time
	epicsTimeStamp now;
	if (!epicsTimeGetCurrent(&now)) {
		ts["secondsPastEpoch"] = now.secPastEpoch + POSIX_TIME_AT_EPICS_EPOCH;
		ts["nanoseconds"] = now.nsec;
	}

	operation->reply(value);
}

void SingleSource::onPut(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& putOperation,
		const Value& valuePrototype, const Value& value) {
	// dbPutField() ...
}

long SingleSource::nameToAddr(const char* pname, DBADDR* paddr) {
	long status = dbNameToAddr(pname, paddr);

	if (status) {
		printf("PV '%s' not found\n", pname);
	}
	return status;
}

void SingleSource::setValue(Value& value, void* pValueBuffer) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		value["value"] = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setValue, (value, pValueBuffer));
	}
}

/**
 * Set a value from the given database value buffer.  This is the array version of the function
 *
 * @param value the value to set
 * @param pValueBuffer the database value buffer
 * @param nElements the number of elements in the buffer
 */
void SingleSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	auto valueType(value["value"].type());
	if (valueType.code == TypeCode::String) {
		value["value"] = ((const char*)pValueBuffer);
	} else {
		SwitchTypeCodeForTemplatedCall(valueType.code, setValue, (value, pValueBuffer, nElements));
	}
}

template<typename valueType> void SingleSource::setValue(Value& value, void* pValueBuffer) {
	return setValueFromBuffer(value, (valueType*)pValueBuffer);
}

template<typename valueType> void SingleSource::setValue(Value& value, void* pValueBuffer, long nElements) {
	return setValueFromBuffer(value, (valueType*)pValueBuffer, nElements);
}

/**
 * Set the value of the given value parameter from the given database value buffer.  This version
 * of the function uses the template parameter to determine the type to use to set the value
 *
 * @tparam valueType the type to use to set the value
 * @param value the value to set
 * @param pValueBuffer the database buffer
 */
template<typename valueType> void SingleSource::setValueFromBuffer(Value& value, valueType* pValueBuffer) {
	value["value"] = pValueBuffer[0];
}

/**
 * Set the value of the given value parameter from the given database value buffer.  This version
 * of the function uses the template parameter to determine the type to use to set the value
 *
 * @tparam valueType the type to use to set the value
 * @param value the value to set
 * @param pValueBuffer the database buffer
 * @param nElements the number of elements in the buffer
 */
template<typename valueType>
void SingleSource::setValueFromBuffer(Value& value, valueType* pValueBuffer, long nElements) {
	shared_array<valueType> values(nElements);
	for (auto i = 0; i < nElements; i++) {
		values[i] = (pValueBuffer)[i];
	}
	value["value"] = values.freeze().template castTo<const void>();
}

} // ioc
} // pvxs
