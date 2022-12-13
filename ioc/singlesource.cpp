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

#include <dbStaticLib.h>
#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>

#include "pvxs/dbentry.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.search");

#define MAXLINE 80
#define MAXMESS 128

struct msgBuff {    /* line output structure */
	char out_buff[MAXLINE + 1];
	char* pNext;
	char* pLast;
	char* pNexTab;
	char message[MAXMESS];
};
typedef struct msgBuff TAB_BUFFER;

static long nameToAddr(const char* pname, DBADDR* paddr) {
	long status = dbNameToAddr(pname, paddr);

	if (status) {
		printf("PV '%s' not found\n", pname);
	}
	return status;
}

static TypeCode::code_t toTypeCode(dbfType dbfTypeCode) {
	switch (dbfTypeCode) {
	case DBF_CHAR:
		return TypeCode::Int8;
	case DBF_UCHAR:
		return TypeCode::UInt8;
	case DBF_SHORT:
		return TypeCode::Int16;
	case DBF_USHORT:
		return TypeCode::UInt16;
	case DBF_LONG:
		return TypeCode::Int32;
	case DBF_ULONG:
		return TypeCode::UInt32;
	case DBF_INT64:
		return TypeCode::Int64;
	case DBF_UINT64:
		return TypeCode::UInt64;
	case DBF_FLOAT:
		return TypeCode::Float32;
	case DBF_DOUBLE:
		return TypeCode::Float64;
	case DBF_STRING:
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK:
		return TypeCode::String;
	case DBF_ENUM:
	case DBF_MENU:
	case DBF_DEVICE:
	case DBF_NOACCESS:
	default:
		return TypeCode::Null;
	}
}

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

void SingleSource::setValue(Value& val, void* pValueBuffer) {
	auto valueType = val["value"].type();
	std::cout << "code" << (short)valueType.code << std::endl;
	switch (valueType.code) {
	case TypeCode::Int8:
		return setValue<char>(val, pValueBuffer);
	case TypeCode::UInt8:
		return setValue<unsigned char>(val, pValueBuffer);
	case TypeCode::Int16:
		return setValue<short>(val, pValueBuffer);
	case TypeCode::UInt16:
		return setValue<unsigned short>(val, pValueBuffer);
	case TypeCode::Int32:
		return setValue<int>(val, pValueBuffer);
	case TypeCode::UInt32:
		return setValue<unsigned int>(val, pValueBuffer);
	case TypeCode::Int64:
		return setValue<long>(val, pValueBuffer);
	case TypeCode::UInt64:
		return setValue<unsigned long>(val, pValueBuffer);
	case TypeCode::Float32:
		return setValue<float>(val, pValueBuffer);
	case TypeCode::Float64:
		return setValue<double>(val, pValueBuffer);
	case TypeCode::String:
		val["value"] = ((const char *)pValueBuffer);
	case TypeCode::Null:
	default:
		;
	}
}

/**
 * Set a value from the given database value buffer.  This is the array version of the function
 *
 * @param val the value to set
 * @param pValueBuffer the database value buffer
 * @param nElements the number of elements in the buffer
 */
void SingleSource::setValue(Value& val, void* pValueBuffer, long nElements) {
	auto valueType = val["value"].type();
	switch (valueType.code) {
	case TypeCode::Int8A:
		return setValue<int8_t>(val, pValueBuffer, nElements);
	case TypeCode::UInt8A:
		return setValue<uint8_t>(val, pValueBuffer, nElements);
	case TypeCode::Int16A:
		return setValue<int16_t>(val, pValueBuffer, nElements);
	case TypeCode::UInt16A:
		return setValue<uint16_t>(val, pValueBuffer, nElements);
	case TypeCode::Int32A:
		return setValue<int32_t>(val, pValueBuffer, nElements);
	case TypeCode::UInt32A:
		return setValue<uint32_t>(val, pValueBuffer, nElements);
	case TypeCode::Int64A:
		return setValue<int64_t>(val, pValueBuffer, nElements);
	case TypeCode::UInt64A:
		return setValue<uint64_t>(val, pValueBuffer, nElements);
	case TypeCode::Float32A:
		return setValue<float>(val, pValueBuffer, nElements);
	case TypeCode::Float64A:
		return setValue<double>(val, pValueBuffer, nElements);
	case TypeCode::StringA:
		return setValue<std::string>(val, pValueBuffer, nElements);
	case TypeCode::Null:
	default:
		;
	}
}

/**
 * Set the value of the given value parameter from the given database value buffer.  This version
 * of the function uses the template parameter to determine the type to use to set the value
 *
 * @tparam valueType the type to use to set the value
 * @param val the value to set
 * @param pValueBuffer the
 */
template<typename valueType> void SingleSource::setValue(Value& val, void* pValueBuffer) {
	val["value"] = ((valueType*)pValueBuffer)[0];
}

template<typename valueType> void SingleSource::setValue(Value& val, void* pValueBuffer, long nElements) {
	shared_array<valueType> values(nElements);
	for (auto i = 0; i < nElements; i++) {
		values[i] = ((valueType*)pValueBuffer)[i];
	}
	val["value"] = values.freeze().template castTo<const void>();
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

	// declare a value buffer to store the field we will get from the database.
	// make it of type long to ensure correct alignment as long buffers are the worst
	// case for alignment on any platform
	long valueBuffer[100];
	long* pValueBuffer = &valueBuffer[0];

	DBADDR dbAddress;   // Special struct for storing database addresses
	long options = 0;   // For options returned from database read along with data
	long nElements;     // Calculated number of elements to retrieve.  For single values its 1 but for arrays it can be any number

//	static TAB_BUFFER msg_Buff;

	if (!nameToAddr(pvName, &dbAddress) && // Convert the pvName to a db address
			dbAddress.precord->lset != nullptr) { // make sure that it has been correctly converted

		// Calculate number of elements to retrieve as lowest of actual number of elements and max number
		// of elements we can store in the buffer we've allocated
		nElements = MIN(dbAddress.no_elements, sizeof(valueBuffer) / dbAddress.field_size);

		if (dbAddress.dbr_field_type == DBR_ENUM) {
			long status = dbGetField(&dbAddress, DBR_STRING, pValueBuffer, &options, &nElements, nullptr);

//						prototype = *pbuffer;
			operation->reply(valuePrototype);
		} else {
			long status = dbGetField(&dbAddress, dbAddress.dbr_field_type, pValueBuffer, &options, &nElements,
					nullptr);
			auto val = valuePrototype.cloneEmpty();

			if (nElements == 1) {
				setValue(val, pValueBuffer);
			} else {
				setValue(val, pValueBuffer, nElements);
			}

			val["alarm.severity"] = 0;
			val["alarm.status"] = 0;
			val["alarm.message"] = "";
			auto ts(val["timeStamp"]);
			// TODO: KLUDGE use current time
			epicsTimeStamp now;
			if (!epicsTimeGetCurrent(&now)) {
				ts["secondsPastEpoch"] = now.secPastEpoch + POSIX_TIME_AT_EPICS_EPOCH;
				ts["nanoseconds"] = now.nsec;
			}

			operation->reply(val);
		}
	}
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
		// op->error(); // TODO:
		return;
	}

	TypeCode valueType(TypeCode::Null);
	short dbrType = dbChannelFinalFieldType(chan.get());
	bool longString = false;

	if (dbChannelFieldType(chan.get()) == DBF_STRING && dbChannelElements(chan.get()) == 1
			&& dbrType && dbChannelFinalElements(chan.get()) > 1) {
		// single DBF_STRING being cast to DBF_CHAR array.  aka long string
		valueType = TypeCode::String;
		longString = true;

	} else if (dbrType == DBF_ENUM && dbChannelFinalElements(chan.get()) == 1) {

	} else {
		if (dbrType == DBF_INLINK || dbrType == DBF_OUTLINK || dbrType == DBF_FWDLINK) {
			dbrType = DBF_CHAR;
			longString = true;
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

void SingleSource::show(std::ostream& strm) {
}

} // ioc
} // pvxs
