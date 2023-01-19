/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_TYPEUTILS_H
#define PVXS_TYPEUTILS_H

#include <pvxs/source.h>
#include <dbStaticLib.h>
#include "typeutils.h"

/**
 * To switch the given `TypeCode` for a statically typed call to the given function with the appropriate template type
 * e.g.
 *   `SwitchTypeCodeForTemplate(typeCode, setValue,(value, pBuffer))`
 * will convert a typeCode of TypeCode::Int8 into a call to
 *   `setValue<char>(value, pBuffer)`
 *
 * @param _typeCode the typecode to be used in the switch statement - should be of type TypeCode or short
 * @param _function the templated function to call
 * @param _arguments the list of arguments to be passed to the templated function.  include the parentheses
 */
#define SwitchTypeCodeForTemplatedCall(_typeCode, _function, _arguments) \
switch (_typeCode) {                               \
    case TypeCode::Int8:    case TypeCode::Int8A:       return _function<int8_t>_arguments ;    \
    case TypeCode::UInt8:   case TypeCode::UInt8A:      return _function<uint8_t>_arguments ;    \
    case TypeCode::Int16:   case TypeCode::Int16A:      return _function<int16_t>_arguments ;    \
    case TypeCode::UInt16:  case TypeCode::UInt16A:     return _function<uint16_t>_arguments ;    \
    case TypeCode::Int32:   case TypeCode::Int32A:      return _function<int32_t>_arguments ;    \
    case TypeCode::UInt32:  case TypeCode::UInt32A:     return _function<uint32_t>_arguments ;    \
    case TypeCode::Int64:   case TypeCode::Int64A:      return _function<int64_t>_arguments ;    \
    case TypeCode::UInt64:  case TypeCode::UInt64A:     return _function<uint64_t>_arguments ;    \
    case TypeCode::Float32: case TypeCode::Float32A:    return _function<float>_arguments ;    \
    case TypeCode::Float64: case TypeCode::Float64A:    return _function<double>_arguments ;  \
    case TypeCode::String:  case TypeCode::StringA:       \
    case TypeCode::Struct:  case TypeCode::StructA:       \
    case TypeCode::Union:  case TypeCode::UnionA:         \
    case TypeCode::Any:  case TypeCode::AnyA:             \
    default:                                              \
        throw std::logic_error("Unsupported type" );      \
}

namespace pvxs {

TypeCode::code_t toTypeCode(dbfType databaseTypeCode);
}

#endif //PVXS_TYPEUTILS_H
