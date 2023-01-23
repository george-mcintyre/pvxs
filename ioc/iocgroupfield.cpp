/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "iocgroupfield.h"

namespace pvxs {
namespace ioc {

IOCGroupField::IOCGroupField()
		:had_initial_VALUE(false), had_initial_PROPERTY(false), allowProc(false) {
}
IOCGroupField::IOCGroupField(const std::string& fieldName, const std::string& channelName)
		:had_initial_VALUE(false), had_initial_PROPERTY(false), allowProc(false), channel(channelName), fieldName(fieldName) {

}

} // pvxs
} // ioc
