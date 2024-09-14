/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * The certificate status manager class
 *
 *   certstatusmanager.h
 *
 */
#ifndef PVXS_CERTSTATUSMANAGER_H_
#define PVXS_CERTSTATUSMANAGER_H_

#include <functional>

#include <openssl/evp.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

// #include <pvxs/client.h>
#include <pvxs/sharedArray.h>

#include "certstatus.h"
#include "evhelper.h"
#include "ownedptr.h"

#define DO_CERT_STATUS_VALIDITY_EVENT_HANDLER(TYPE)                                         \
    void TYPE::doCertStatusValidityEventhandler(evutil_socket_t fd, short evt, void* raw) { \
        auto pvt = static_cast<TYPE*>(raw);                                                 \
        if (pvt->current_status.isValid()) return;                                          \
        if (!pvt->cert_status_manager)                                                      \
            pvt->disableTls();                                                              \
        else {                                                                              \
            try {                                                                           \
                pvt->current_status = pvt->cert_status_manager->getStatus();                \
                if (pvt->current_status.isGood())                                           \
                    pvt->startStatusValidityTimer();                                        \
                else                                                                        \
                    pvt->disableTls();                                                      \
            } catch (...) {                                                                 \
                pvt->disableTls();                                                          \
            }                                                                               \
        }                                                                                   \
    }

#define CUSTOM_FILE_EVENT_CALL                \
    if (pvt->custom_cert_event_callback) {    \
        pvt->custom_cert_event_callback(evt); \
    }
#define _FILE_EVENT_CALL

#define DO_CERT_EVENT_HANDLER(TYPE, LOG, ...)                                                                                              \
    void TYPE::doCertEventHandler(evutil_socket_t fd, short evt, void* raw) {                                                              \
        try {                                                                                                                              \
            auto pvt = static_cast<TYPE*>(raw);                                                                                            \
            __VA_ARGS__##_FILE_EVENT_CALL pvt->fileEventCallback(evt);                                                                     \
            if (pvt->first_cert_event) pvt->first_cert_event = false;                                                                      \
            timeval interval(statusIntervalShort);                                                                                         \
            if (event_add(pvt->cert_event_timer.get(), &interval)) log_err_printf(LOG, "Error re-enabling cert file event timer\n%s", ""); \
        } catch (std::exception & e) {                                                                                                     \
            log_exc_printf(LOG, "Unhandled error in cert file event timer callback: %s\n", e.what());                                      \
        }                                                                                                                                  \
    }

#define FILE_EVENT_CALLBACK(TYPE)                              \
    void TYPE::fileEventCallback(short evt) {                  \
        if (!first_cert_event) file_watcher.checkFileStatus(); \
    }

#define GET_CERT(TYPE)                                                      \
    X509* TYPE::getCert(ossl::SSLContext* context_ptr) {                    \
        auto context = context_ptr == nullptr ? &tls_context : context_ptr; \
        if (!context->ctx) return nullptr;                                  \
        return SSL_CTX_get0_certificate(context->ctx);                      \
    }

#define START_STATUS_VALIDITY_TIMER(TYPE, LOOP)                                                                                                           \
    void TYPE::startStatusValidityTimer() {                                                                                                               \
        (LOOP).dispatch([this]() {                                                                                                                        \
            auto now = time(nullptr);                                                                                                                     \
            timeval validity_end = {current_status.status_valid_until_date.t - now, 0};                                                                   \
            if (event_add(cert_validity_timer.get(), &validity_end)) log_err_printf(watcher, "Error starting certificate status validity timer\n%s", ""); \
        });                                                                                                                                               \
    }

#define SUBSCRIBE_TO_CERT_STATUS(TYPE, LOOP)                                                                                  \
    void TYPE::subscribeToCertStatus() {                                                                                      \
        if (auto ctx_cert = getCert()) {                                                                                      \
            try {                                                                                                             \
                cert_status_manager = certs::CertStatusManager::subscribe(ctx_cert, [this](certs::CertificateStatus status) { \
                    if (status.isGood()) {                                                                                    \
                        tls_context.cert_valid = true;                                                                        \
                        log_debug_printf(watcher, "Set cert_valid: %s\n", "true");                                            \
                        (LOOP).dispatch([this]() mutable { enableTls(); });                                                   \
                    } else {                                                                                                  \
                        tls_context.cert_valid = false;                                                                       \
                        log_debug_printf(watcher, "Set cert_valid: %s\n", "false");                                           \
                        (LOOP).dispatch([this]() mutable { disableTls(); });                                                  \
                    }                                                                                                         \
                });                                                                                                           \
            } catch (certs::CertStatusSubscriptionException & e) {                                                            \
                log_warn_printf(watcher, "TLS Disabled: %s\n", e.what());                                                     \
            } catch (certs::CertStatusNoExtensionException & e) {                                                             \
                log_debug_printf(watcher, "Status monitoring not configured correctly: %s\n", e.what());                      \
            }                                                                                                                 \
        }                                                                                                                     \
    }

namespace pvxs {

// Forward def
namespace client {
class Context;
struct Subscription;
}  // namespace client

namespace certs {

template <typename T>
struct cert_status_delete;

template <typename T>
using cert_status_ptr = OwnedPtr<T, cert_status_delete<T>>;

/**
 * @brief This class is used to parse OCSP responses and to get/subscribe to certificate status
 *
 * Parsing OCSP responses is carried out by providing the OCSP response buffer
 * to the static `parse()` function. This function will verify the response comes
 * from a trusted source, is well formed, and then will return the `OCSPStatus`
 * it indicates.
 * @code
 *  auto ocsp_status(CertStatusManager::parse(ocsp_response);
 * @endcode
 *
 * To get certificate status call the status `getStatus()` method with the
 * the certificate you want to get status for.  It will make a request
 * to the PVACMS to get certificate status for the certificate. After verifying the
 * authenticity of the response and checking that it is from a trusted
 * source it will return `CertificateStatus`.
 * @code
 *  auto cert_status(CertStatusManager::getStatus(cert);
 * @endcode
 *
 * To subscribe, call the subscribe method with the certificate you want to
 * subscribe to status for and provide a callback that takes a `CertificateStatus`
 * to be notified of status changes.  It will subscribe to PVACMS to monitor changes to
 * to the certificate status for the given certificate. After verifying the
 * authenticity of each status update and checking that it is from a trusted
 * source it will call the callback with a `CertificateStatus` representing the
 * updated status.
 * @code
 *  auto csm = CertStatusManager::subscribe(cert, [] (CertificateStatus &&cert_status) {
 *      std::cout << "STATUS DATE: " << cert_status.status_date.s << std::endl;
 *  });
 *  ...
 *  csm.unsubscribe();
 *  // unsubscribe() automatically called when csm goes out of scope
 * @endcode
 */
class CertStatusManager {
   public:
    friend struct OCSPStatus;
    CertStatusManager() = delete;

    virtual ~CertStatusManager() = default;

    using StatusCallback = std::function<void(const CertificateStatus&)>;

    static bool shouldMonitor(const ossl_ptr<X509>& certificate);

    /**
     * @brief Get the status PV from a Cert.
     * This function gets the PVA extension that stores the status PV in the certificate
     * if the certificate must be used in conjunction with a status monitor to check for
     * revoked status.
     * @param cert the certificate to check for the status PV extension
     * @return a blank string if no extension exists, otherwise contains the status PV
     *         e.g. CERT:STATUS:0293823f:098294739483904875
     */
    static std::string getStatusPvFromCert(const ossl_ptr<X509>& cert);

    /**
     * @brief Used to create a helper that you can use to subscribe to certificate status with
     * Subsequently call subscribe() to subscribe
     *
     * @param cert_ptr pointer to certificate you want to subscribe to
     * @param callback the callback to callwhen a status change has appeared
     *
     * @see unsubscribe()
     */
    static cert_status_ptr<CertStatusManager> subscribe(X509* cert_ptr, StatusCallback&& callback);

    /**
     * @brief Get status for a given certificate
     * @param cert the certificate for which you want to get status
     * @return CertificateStatus
     */
    static CertificateStatus getStatus(const ossl_ptr<X509>& cert);

    /**
     * @brief Wait for status to become available or return the current status if it is still valid
     * @param loop the event loop base to use to wait
     * @return the status
     */
    CertificateStatus waitForStatus(const evbase& loop);

    /**
     * @brief Unsubscribe from listening to certificate status
     *
     * This function idempotent unsubscribe from the certificate status updates
     */
    void unsubscribe();

    /**
     * @brief Get status for a currently subscribed certificate
     * @return CertificateStatus
     */
    CertificateStatus getStatus();

    static uint64_t getSerialNumber(const ossl_ptr<X509>& cert);
    static uint64_t getSerialNumber(X509* cert);

   private:
    CertStatusManager(ossl_ptr<X509>&& cert, std::shared_ptr<client::Context>& client, std::shared_ptr<client::Subscription>& sub)
        : cert_(std::move(cert)), client_(client), sub_(sub) {};
    CertStatusManager(ossl_ptr<X509>&& cert, std::shared_ptr<client::Context>& client) : cert_(std::move(cert)), client_(client) {};
    inline void subscribe(std::shared_ptr<client::Subscription>& sub) { sub_ = sub; }
    inline bool isValid() noexcept {
        if (status_ == UNKNOWN) return false;
        auto now(std::time(nullptr));
        return status_valid_until_date_ > now;
    }
    inline bool isGood() noexcept { return isValid() && status_ == VALID; }

    const ossl_ptr<X509> cert_;
    std::shared_ptr<client::Context> client_;
    std::shared_ptr<client::Subscription> sub_;
    certstatus_t status_;
    time_t status_valid_until_date_;
    time_t revocation_date_;
    static ossl_ptr<OCSP_RESPONSE> getOCSPResponse(const shared_array<uint8_t>& ocsp_bytes);

    static bool verifyOCSPResponse(const ossl_ptr<OCSP_BASICRESP>& basic_response);
    static uint64_t ASN1ToUint64(ASN1_INTEGER* asn1_number);

    /**
     * @brief To parse OCSP responses
     *
     * Parsing OCSP responses is carried out by providing the OCSP response buffer.
     * This function will verify the response comes from a trusted source,
     * is well formed, and then will return the `ParsedOCSPStatus` it indicates.
     *
     * @param ocsp_bytes the ocsp response
     * @return the Parsed OCSP response status
     */
   public:
    static ParsedOCSPStatus parse(shared_array<uint8_t> ocsp_bytes);

   private:
    std::vector<uint8_t> ocspResponseToBytes(const pvxs::ossl_ptr<OCSP_BASICRESP>& basic_resp);
};

template <>
struct cert_status_delete<CertStatusManager> {
    inline void operator()(CertStatusManager* base_pointer) {
        if (base_pointer) {
            base_pointer->unsubscribe();  // Idempotent unsubscribe
            delete base_pointer;
        }
    }
};

}  // namespace certs
}  // namespace pvxs

#endif  // PVXS_CERTSTATUSMANAGER_H_
