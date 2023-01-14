/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPPV_H
#define PVXS_GROUPPV_H

#include <map>

namespace pvxs {
namespace ioc {

class GroupPV {
	GroupPV() = default;
	virtual ~GroupPV() = default;

public:
	virtual void show(int lvl) {}
};

typedef std::map<std::string, std::shared_ptr<GroupPV>> GroupPvMap;

} // pvxs
} // ioc

#endif //PVXS_GROUPPV_H
