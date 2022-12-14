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

void SingleSource::onCreate(std::unique_ptr<server::ChannelControl>&& op) {
	dbChannel* dbchan = dbChannelCreate(op->name().c_str());
	if (!dbchan) {
		log_debug_printf(_logname, "Ignore requested channel for '%s'\n", op->name().c_str());
		return;
	}
	log_debug_printf(_logname, "Accepting channel for '%s'\n", op->name().c_str());

	std::shared_ptr<dbChannel> chan(dbchan, [](dbChannel* ch) {
		dbChannelDelete(ch);
	});

	if (DBErrMsg err = dbChannelOpen(chan.get())) {
		throw std::runtime_error(err.c_str());
	}

	TypeCode valueType(TypeCode::Null);
	short dbrType = dbChannelFinalFieldType(chan.get());

	if (dbChannelFieldType(chan.get()) == DBF_STRING && dbChannelElements(chan.get()) == 1
			&& dbrType && dbChannelFinalElements(chan.get()) > 1) {
		// single DBF_STRING being cast to DBF_CHAR array.  aka long string
		valueType = TypeCode::String;

	} else if (dbrType == DBF_ENUM && dbChannelFinalElements(chan.get()) == 1) {

	} else {
		// TODO handle links.  For the moment we handle as chars and fail later
		if (dbrType == DBF_INLINK || dbrType == DBF_OUTLINK || dbrType == DBF_FWDLINK) {
			dbrType = DBF_CHAR;
		}

		valueType = toTypeCode(dbfType(dbrType));
		if (valueType != TypeCode::Null && dbChannelFinalElements(chan.get()) != 1) {
			valueType = valueType.arrayOf();
		}
	}

	Value valuePrototype(nt::NTScalar{ valueType }.create()); // TODO: enable meta

	op->onOp([chan, valuePrototype](std::unique_ptr<server::ConnectOp>&& cop) {
		cop->onGet([chan, valuePrototype](std::unique_ptr<server::ExecOp>&& eop) {
			onGet(chan, eop, valuePrototype);
		});

		cop->onPut([](std::unique_ptr<server::ExecOp>&& eop, Value&& val) {
			// TODO
			std::cout << "On Put" << std::endl;

			// dbPutField() ...
		});

		cop->connect(valuePrototype);
	});

	op->onSubscribe([chan, valuePrototype](std::unique_ptr<server::MonitorSetupOp>&& setup) {
		auto ctrl(setup->connect(valuePrototype));

		// db_add_event( &callback)

		ctrl->onStart([](bool start) {
			// TODO
			if (start) {
				// db_event_enable()
				// db_post_single_event()

			} else {
				// db_event_disable()
			}
		});

	});
}

SingleSource::List SingleSource::onList() {
	return allrecords;
}

void SingleSource::onSearch(Search& op) {
	for (auto& pv: op) {
		std::cout << "Checking : " << pv.name() << std::endl;
		if (!dbChannelTest(pv.name())) {
			std::cout << "Matched name: " << pv.name() << std::endl;
			pv.claim();
			log_debug_printf(_logname, "Claiming '%s'\n", pv.name());
		}
	}
}

void SingleSource::show(std::ostream& strm) {
}

/**
 * Handle the get operation
 *
 * @param channel the channel that the request comes in on
 * @param operation the current executing operation
 * @param valuePrototype a value prototype that is made based on the expected type to be returned
 */
void SingleSource::onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& operation,
		const Value& valuePrototype) {
	const char* pvName = channel->name;

	epicsAny valueBuffer[100]; // value buffer to store the field we will get from the database.
	void* pValueBuffer = &valueBuffer[0];
	DBADDR dbAddress;   // Special struct for storing database addresses
	long options = 0;   // For options returned from database read along with data
	long nElements;     // Calculated number of elements to retrieve.  For single values its 1 but for arrays it can be any number

	// Convert pvName to a dbAddress
	if (DBErrMsg err = nameToAddr(pvName, &dbAddress)) {
		operation->error(err.c_str());
		return;
	}

	if (dbAddress.precord->lset == nullptr) {
		operation->error("pvName no specified in request");
		return;
	}

	// Calculate number of elements to retrieve as lowest of actual number of elements and max number
	// of elements we can store in the buffer we've allocated
	nElements = MIN(dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

	if (dbAddress.dbr_field_type == DBR_ENUM) {
		onGetEnum(operation, valuePrototype, pValueBuffer, dbAddress, options, nElements);
	} else {
		onGetNonEnum(operation, valuePrototype, pValueBuffer, dbAddress, options, nElements);
	}
}

void SingleSource::onGetEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
		DBADDR& dbAddress, long& options, long& nElements) {
	if (DBErrMsg err = dbGetField(&dbAddress, DBR_STRING, pValueBuffer, &options, &nElements, nullptr)) {
		operation->error(err.c_str());
	}

//						prototype = *pbuffer;
	operation->reply(valuePrototype);
}

void SingleSource::onGetNonEnum(std::unique_ptr<server::ExecOp>& operation, const Value& valuePrototype, void* pValueBuffer,
		DBADDR& dbAddress, long& options, long& nElements) {// Get the field value
	if (DBErrMsg err = dbGetField(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, &options, &nElements,
			nullptr)) {
		operation->error(err.c_str());
	}

	// Create a placeholder for the return value
	auto value = valuePrototype.cloneEmpty();
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

long SingleSource::nameToAddr(const char* pname, DBADDR* paddr) {
	long status = dbNameToAddr(pname, paddr);

	if (status) {
		printf("PV '%s' not found\n", pname);
	}
	return status;
}

void SingleSource::setValue(Value& value, void* pValueBuffer) {
	auto valueType = value["value"].type();
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
	auto valueType = value["value"].type();
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
