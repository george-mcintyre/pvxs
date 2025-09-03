/**
* Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "serverx.h"

#include <atomic>
#include <cstdlib>
#include <functional>

#include <dbDefs.h>
#include <envDefs.h>
#include <epicsString.h>
#include <signal.h>

#include <pvxs/client.h>
#include <pvxs/log.h>
#include <pvxs/server.h>
#include <pvxs/sharedpv.h>

#include "configcerts.h"
#include "certstatusmanager.h"
#include "evhelper.h"
#include "serverconn.h"

namespace pvxs {
namespace serverx {

DEFINE_LOGGER(serverio, "pvxs.svr.io");
DEFINE_LOGGER(serversetup, "pvxs.svr.init");


Server Server::fromEnv(CustomServerCallback &custom_event_callback)
{
    return certs::Config::fromEnv().build(custom_event_callback);
}

Server::Server(const certs::Config &config, const CustomServerCallback &custom_cert_event_callback) : server::Server(config) {
    auto internal(std::make_shared<Impl>(*this, config, custom_cert_event_callback));
    internal->self = internal;

    // external
    impl.reset(internal.get(), [internal](Impl*) mutable {
        const auto trash(std::move(internal));
    });
}

Server& Server::addWildcardPV(const std::string& name, const SharedWildcardPV& pv)
{
    if(!pvt)
        throw std::logic_error("NULL Server");
    pvt->builtinsrc.add(name, pv);
    ++pvt->beaconChange;
    return *this;
}

Server::Impl::Impl(Server &svr, const certs::Config& conf, const CustomServerCallback &custom_cert_event_callback)
    : custom_server_callback(custom_cert_event_callback)
    , custom_server_callback_timer(__FILE__, __LINE__, event_new(svr.pvt->acceptor_loop.base, -1, EV_TIMEOUT, doCustomServerCallback, this)) {
    {
        // Clean out guid created by base
        svr.pvt->effective.guid.fill(0);

        // simplified GUID.
        // treat as 3x 32-bit unsigned.
        union {
            std::array<uint32_t, 3> i;
            std::array<uint8_t, 3*4> b;
        } pun{};
        static_assert (sizeof(pun)==12, "");

        // For PVACMS, generate a deterministic GUID based on "pvacms/cluster"
        const std::string input = "pvacms/cluster";

        // Simple deterministic hash function
        for (size_t idx = 0; idx < input.size(); idx++) {
            pun.b[idx % pun.b.size()] ^= input[idx];
            // Rotate bits to spread the entropy
            if ((idx + 1) % 4 == 0) {
                uint32_t& val = pun.i[idx / 4];
                val = (val << 13) | (val >> 19);
            }
        }

        // Add some fixed bits to ensure uniqueness from random GUIDs
        pun.b[0] |= 0x80; // Set high bit to mark as deterministic
        pun.b[11] = 0x42; // Magic number for PVACMS

        std::copy(pun.b.begin(), pun.b.end(), svr.pvt->effective.guid.begin());
    }
}


void Server::Impl::doCustomServerCallback(evutil_socket_t fd, short evt, void* raw) {
    try {
        const auto pvt = static_cast<Impl*>(raw);
        if (pvt && pvt->custom_server_callback) {
            auto next_timeval = pvt->custom_server_callback(evt);
            if (next_timeval.tv_sec == 0 && next_timeval.tv_usec == 0) {
                next_timeval = kCustomCallbackInterval;
            }
            if (next_timeval.tv_sec > 0 || next_timeval.tv_usec > 0) {
                if (event_add(pvt->custom_server_callback_timer.get(), &next_timeval))
                    log_err_printf(serverio, "Error re-enabling custom server callback%s\n", "");
            }
        }
    } catch (std::exception& e) {
        log_err_printf(serverio, "Unhandled error in custom server callback: %s\n", e.what());
    }
}

void Server::startCb() const {
    // begin running custom server callback if configured
    if ( impl->custom_server_callback )
        pvt->acceptor_loop.call([this]()
        {
             // Trigger the first custom server callback, with the initial interval period
             if(event_add(impl->custom_server_callback_timer.get(), &kCustomCallbackIntervalInitial))
                 log_err_printf(serversetup, "Error enabling file monitor\n%s", "");
        });
}

void Server::stopCb() const {
    pvt->acceptor_loop.call([this]()
    {
        if (impl->custom_server_callback_timer) {
            if (event_del(impl->custom_server_callback_timer.get()))
                log_warn_printf(serversetup, "Error disabling custom server callback timer\n%s", "");
        }
    });
}

} // server
} // pvxs
