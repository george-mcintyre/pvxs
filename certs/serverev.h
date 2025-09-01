/**
* Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SERVEREV_H
#define PVXS_SERVEREV_H

#include <functional>
#include <memory>
#include <string>

#include <osiSock.h>

#include <pvxs/server.h>

#include "serverconn.h"
#include "sharedwildcardpv.h"

namespace pvxs {
namespace certs {
class Config;
}

namespace serverev {

using CustomServerCallback = std::function<timeval(short)>;
static constexpr timeval kCustomCallbackIntervalInitial{0, 0};
static constexpr timeval kCustomCallbackInterval{15, 0};

struct EnhancedConfig;

class ServerEv : public server::Server {

  public:
    constexpr ServerEv() = default;
    PVXS_API ServerEv(const certs::Config &config, const CustomServerCallback &custom_event_callback);
    ServerEv fromEnv(CustomServerCallback &custom_event_callback);
    ServerEv& addWildcardPV(const std::string& name, const SharedWildcardPV& pv);
    struct Pvt;

    private:
        std::shared_ptr<Pvt> pvt;
};

struct ServerEv::Pvt : Server::Pvt {
    CustomServerCallback custom_server_callback;
    evevent custom_server_callback_timer;
    Pvt(ServerEv &svr, const certs::Config& conf, const CustomServerCallback &custom_cert_event_callback = nullptr );
    static void doCustomServerCallback(evutil_socket_t fd, short evt, void* raw);
    void start();
    void stop();
};

} // serverev
} // pvxs

#endif //PVXS_SERVEREV_H
