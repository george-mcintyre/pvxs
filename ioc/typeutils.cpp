/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pvxs/source.h>
#include <dbStaticLib.h>
#include "typeutils.h"

namespace pvxs {

/**
 * Convert the given database type code into a pvxs type code
 *
 * TODO support DBF_ENUM, DBF_MENU, DBF_DEVICE AND DBF_NOACCESS
 *
 * @param databaseTypeCode the database type code
 * @return a pvxs type code
 *
 */
TypeCode::code_t toTypeCode(dbfType databaseTypeCode) {
	switch (databaseTypeCode) {
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
	case DBF_ENUM:
		return TypeCode::Struct;
	case DBF_STRING:
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK:
		return TypeCode::String;
	case DBF_MENU:
	case DBF_DEVICE:
	case DBF_NOACCESS:
	default:
		return TypeCode::Null;
	}
}
}
