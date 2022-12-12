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

#include <errSymTbl.h>

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

SingleSource::SingleSource()
		:dbe(db_init_events()) {
	if (!dbe)
		throw std::runtime_error("Unable to allocate dbEvent context");

	if (db_start_events(dbe.get(), "XSRVsingle", nullptr, nullptr, epicsThreadPriorityCAServerLow)) {
		throw std::runtime_error("Unable to start dbEvent context");
	}

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

	TypeCode valuetype(TypeCode::Null);
	short dbrType = dbChannelFinalFieldType(chan.get());
	bool longstring = false;

	if (dbChannelFieldType(chan.get()) == DBF_STRING && dbChannelElements(chan.get()) == 1
			&& dbrType && dbChannelFinalElements(chan.get()) > 1) {
		// single DBF_STRING being cast to DBF_CHAR array.  aka long string
		valuetype = TypeCode::String;
		longstring = true;

	} else if (dbrType == DBF_ENUM && dbChannelFinalElements(chan.get()) == 1) {

	} else {
		switch (dbrType) {
		case DBF_INLINK:
		case DBF_OUTLINK:
		case DBF_FWDLINK:
			dbrType = DBF_CHAR;
			longstring = true;
			break;
		}

		switch (dbrType) {
#define CASE(DBF, PVX) case DBF_ ## DBF: valuetype = TypeCode::PVX; break
		CASE(CHAR, Int8);
		CASE(UCHAR, UInt8);
		CASE(SHORT, Int16);
		CASE(USHORT, UInt16);
		CASE(LONG, Int32);
		CASE(ULONG, UInt32);
		CASE(INT64, Int64);
		CASE(UINT64, UInt64);
		CASE(FLOAT, Float32);
		CASE(DOUBLE, Float64);
		CASE(STRING, String);
		CASE(INLINK, String);
		CASE(OUTLINK, String);
		CASE(FWDLINK, String);
#undef CASE
		}

		if (valuetype != TypeCode::Null && dbChannelFinalElements(chan.get()) != 1) {
			valuetype = valuetype.arrayOf();
		}
	}

	Value prototype(nt::NTScalar{ valuetype }.create()); // TODO: enable meta

	op->onOp([chan, prototype](std::unique_ptr<server::ConnectOp>&& cop) {
		cop->onGet([chan, prototype](std::unique_ptr<server::ExecOp>&& eop) {
			const char* pname = chan->name;
			/* declare buffer long just to ensure correct alignment */
			long buffer[100];
			long* pbuffer = &buffer[0];
			DBADDR addr;
			long options = 0;
			long no_elements;
			static TAB_BUFFER msg_Buff;

			if (!nameToAddr(pname, &addr) && addr.precord->lset != NULL) {
				no_elements = MIN(addr.no_elements, sizeof(buffer) / addr.field_size);
				if (addr.dbr_field_type == DBR_ENUM) {
					long status = dbGetField(&addr, DBR_STRING, pbuffer, &options, &no_elements, nullptr);

//						prototype = *pbuffer;
					eop->reply(prototype);
				} else {
					long status = dbGetField(&addr, addr.dbr_field_type, pbuffer, &options, &no_elements, nullptr);
					auto val = prototype.cloneEmpty();
					val["value"] = ((double*)pbuffer)[0];
					val["alarm.severity"] = 0;
					val["alarm.status"] = 0;
					val["alarm.message"] = "";
					eop->reply(val);
				}
			}
		});

		cop->onPut([](std::unique_ptr<server::ExecOp>&& eop, Value&& val) {
			// TODO
			std::cout << "On Put" << std::endl;

			// dbPutField() ...
		});

		cop->connect(prototype);
	});

	op->onSubscribe([chan, prototype](std::unique_ptr<server::MonitorSetupOp>&& setup) {
		auto ctrl(setup->connect(prototype));

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