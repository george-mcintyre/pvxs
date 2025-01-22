// Created on 19/09/2024.
//
#include "ccrmanager.h"

#include <pvxs/nt.h>

#include "client.h"
#include "pvacms.h"
#include "security.h"

DEFINE_LOGGER(auths, "pvxs.certs.auth");

namespace pvxs {
namespace certs {

using namespace members;

/**
 * @brief Create a certificate
 *
 * @param cert_creation_request Certificate creation request
 * @param timeout Timeout for the request
 * @return std::string PEM format Certificate.
 */
std::string CCRManager::createCertificate(const std::shared_ptr<CertCreationRequest> &cert_creation_request, double timeout) const {
    auto uri = nt::NTURI({}).build();
    uri += {Struct("query", CCR_PROTOTYPE(cert_creation_request->verifier_fields))};
    auto arg = uri.create();

    // Set values of request argument
    arg["path"] = RPC_CERT_CREATE;
    arg["query"].from(cert_creation_request->ccr);

    auto conf = client::Config::fromEnv();
    conf.tls_disabled = true;
    auto client = conf.build();
    auto value(client.rpc(RPC_CERT_CREATE, arg).exec()->wait(timeout));

    log_info_printf(auths, "X.509 CLIENT certificate%s\n", "");
    log_info_printf(auths, "%s\n", value["status.value.index"].as<std::string>().c_str());
    log_info_printf(auths, "%s\n", value["state"].as<std::string>().c_str());
    log_info_printf(auths, "%llu\n", value["serial"].as<serial_number_t>());
    log_info_printf(auths, "%s\n", value["issuer"].as<std::string>().c_str());
    log_info_printf(auths, "%s\n", value["certid"].as<std::string>().c_str());
    log_info_printf(auths, "%s\n", value["statuspv"].as<std::string>().c_str());
    return value["cert"].as<std::string>();
}
}  // namespace certs
}  // namespace pvxs
