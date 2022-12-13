/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SINGLESOURCE_H
#define PVXS_SINGLESOURCE_H

#include <dbEvent.h>
#include "pvxs/source.h"

namespace pvxs {
namespace ioc {

class SingleSource : public server::Source {
	List allrecords;

	template<typename valueType> static void setValue(Value& value, void* pValueBuffer);
	template<typename valueType> static void setValue(Value& value, void* pValueBuffer, long nElements);

	template<typename valueType> static void setValueFromBuffer(Value& value, valueType* pValueBuffer);
	template<typename valueType> static void setValueFromBuffer(Value& value, valueType* pValueBuffer, long nElements);

public:
	SingleSource();
	void onSearch(Search& op) final;
	void onCreate(std::unique_ptr<server::ChannelControl>&& op) final;
	List onList() final;
	void show(std::ostream& strm) final;
	static void onGet(const std::shared_ptr<dbChannel>& channel, std::unique_ptr<server::ExecOp>& operation,
			const Value& valuePrototype);
	static void setValue(Value& value, void* pValueBuffer);
	static void setValue(Value& value, void* pValueBuffer, long nElements);
};

} // ioc
} // pvxs


#endif //PVXS_SINGLESOURCE_H
