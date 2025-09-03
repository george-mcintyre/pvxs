/**
* Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SERVERX_H
#define PVXS_SERVERX_H

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

namespace serverx {

using CustomServerCallback = std::function<timeval(short)>;
static constexpr timeval kCustomCallbackIntervalInitial{0, 0};
static constexpr timeval kCustomCallbackInterval{15, 0};

struct EnhancedConfig;

class Server : public server::Server {

  public:
    constexpr Server() = default;
    PVXS_API Server(const certs::Config &config, const CustomServerCallback &custom_event_callback);
    Server fromEnv(CustomServerCallback &custom_event_callback);
    Server& addWildcardPV(const std::string& name, const SharedWildcardPV& pv);
    struct Impl;

    private:
        std::shared_ptr<Impl> impl;
        void startCb() const;
        void stopCb() const;
};

struct Server::Impl {
    std::weak_ptr<Impl> self;
    CustomServerCallback custom_server_callback;
    evevent custom_server_callback_timer;
    Impl(Server &svr, const certs::Config& conf, const CustomServerCallback &custom_cert_event_callback = nullptr );
    static void doCustomServerCallback(evutil_socket_t fd, short evt, void* raw);
};

} // serverx
} // pvxs

#endif //PVXS_SERVERX_H
