/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * The PVAccess Certificate Management Service.
 *
 *   pvacms.h
 *
 */
#ifndef PVXS_PVACMS_H
#define PVXS_PVACMS_H

#include <ctime>
#include <iostream>
#include <vector>

#include <openssl/evp.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <pvxs/sharedpv.h>
#include <pvxs/sharedwildcardpv.h>

#include "certfactory.h"
#include "certmgmtservice.h"
#include "certstatus.h"
#include "configcms.h"
#include "ownedptr.h"

#define RPC_CERT_CREATE "CERT:CREATE"
#define RPC_CERT_ROTATE_PV "CERT:ROTATE"
#define RPC_CERT_REVOKE_PV "CERT:REVOKE:????????:*"

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

#define DEFAULT_KEYCHAIN_FILE "server.p12"
#define DEFAULT_CA_KEYCHAIN_FILE "ca.p12"
#define DEFAULT_ACF_FILE "pvacms.acf"

#define RPC_CERT_REVOKE_ROOT "CERT:REVOKE"

#define PVXS_HOSTNAME_MAX 1024
#define PVXS_ORG_UNIT_MAME "Certificate Authority"
#define PVXS_SERVICE_NAME "PVACMS Service"
#define PVXS_SERVICE_ORG_UNIT_NAME "EPICS PVA Certificate Management Service"

#define SQL_CREATE_DB_FILE              \
    "BEGIN TRANSACTION;"                \
    "CREATE TABLE IF NOT EXISTS certs(" \
    "     serial INTEGER,"              \
    "     skid TEXT,"                   \
    "     CN TEXT,"                     \
    "     O TEXT,"                      \
    "     OU TEXT,"                     \
    "     C TEXT,"                      \
    "     not_before INTEGER,"          \
    "     not_after INTEGER,"           \
    "     status INTEGER,"              \
    "     status_date INTEGER"          \
    ");"                                \
    "COMMIT;"

#define SQL_CREATE_CERT    \
    "INSERT INTO certs ( " \
    "     serial,"         \
    "     skid,"           \
    "     CN,"             \
    "     O,"              \
    "     OU,"             \
    "     C,"              \
    "     not_before,"     \
    "     not_after,"      \
    "     status,"         \
    "     status_date"     \
    ") "                   \
    "VALUES ("             \
    "     :serial,"        \
    "     :skid,"          \
    "     :CN,"            \
    "     :O,"             \
    "     :OU,"            \
    "     :C,"             \
    "     :not_before,"    \
    "     :not_after,"     \
    "     :status,"        \
    "     :status_date"    \
    ")"

#define SQL_DUPS_SUBJECT \
    "SELECT COUNT(*) "   \
    "FROM certs "        \
    "WHERE CN = :CN "    \
    "  AND O = :O "      \
    "  AND OU = :OU "    \
    "  AND C = :C "

#define SQL_DUPS_SUBJECT_KEY_IDENTIFIER \
    "SELECT COUNT(*) "                  \
    "FROM certs "                       \
    "WHERE skid = :skid "

#define SQL_CERT_STATUS   \
    "SELECT status "      \
    "     , status_date " \
    "FROM certs "         \
    "WHERE serial = :serial"

#define SQL_CERT_VALIDITY \
    "SELECT not_before "  \
    "     , not_after "   \
    "FROM certs "         \
    "WHERE serial = :serial"

#define SQL_CERT_SET_STATUS \
    "UPDATE certs "         \
    "SET status = :status " \
    "WHERE serial = :serial "

#define SQL_CERT_TO_VALID                        \
    "SELECT serial "                             \
    "FROM certs "                                \
    "WHERE not_before <= strftime('%s', 'now') " \
    "  AND not_after > strftime('%s', 'now') "

#define SQL_CERT_TO_EXPIRED \
    "SELECT serial "        \
    "FROM certs "           \
    "WHERE not_after <= strftime('%s', 'now') "

namespace pvxs {
namespace certs {

void checkForDuplicates(sql_ptr &ca_db, CertFactory &cert_factory);

std::shared_ptr<KeyPair> createCaKey(ConfigCms &config);
void createCaCertificate(ConfigCms &config, sql_ptr &ca_db, std::shared_ptr<KeyPair> &key_pair);

ossl_ptr<X509> createCertificate(sql_ptr &ca_db, CertFactory &cert_factory);

std::string createCertificatePemString(sql_ptr &ca_db, CertFactory &cert_factory);

std::shared_ptr<KeyPair> createServerKey(const ConfigCms &config);
void createServerCertificate(const ConfigCms &config, sql_ptr &ca_db, ossl_ptr<X509> &ca_cert, ossl_ptr<EVP_PKEY> &ca_pkey,
                             const ossl_shared_ptr<STACK_OF(X509)> &ca_chain, std::shared_ptr<KeyPair> &key_pair);

void ensureServerCertificateExists(ConfigCms config, sql_ptr &ca_db, ossl_ptr<X509> &ca_cert, ossl_ptr<EVP_PKEY> &ca_pkey,
                                   const ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

void ensureValidityCompatible(CertFactory &cert_factory);

uint64_t generateSerial();

std::tuple<certstatus_t, time_t> getCertificateStatus(sql_ptr &ca_db, uint64_t serial);
std::tuple<time_t, time_t> getCertificateValidity(sql_ptr &ca_db, uint64_t serial);

std::string getCountryCode();

Value getCreatePrototype();

std::string getIPAddress();

std::string getIssuerId(const ossl_ptr<X509> &ca_cert);

time_t getNotAfterTimeFromCert(const X509 *cert);

time_t getNotBeforeTimeFromCert(const X509 *cert);

void getOrCreateCaCertificate(ConfigCms &config, sql_ptr &ca_db, ossl_ptr<X509> &ca_cert, ossl_ptr<EVP_PKEY> &ca_pkey,
                              ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

void initCertsDatabase(sql_ptr &ca_db, std::string &db_file);

void onCreateCertificate(ConfigCms &config, sql_ptr &ca_db, const server::SharedPV &pv, std::unique_ptr<server::ExecOp> &&op, Value &&args, const ossl_ptr<EVP_PKEY> &ca_pkey,
                         const ossl_ptr<X509> &ca_cert, const ossl_ptr<EVP_PKEY> &ca_pub_key, const ossl_shared_ptr<STACK_OF(X509)> &ca_chain,
                         std::string issuer_id);

void onGetStatus(ConfigCms &config, sql_ptr &ca_db, const std::string &our_issuer_id, server::SharedWildcardPV &status_pv, const std::string &pv_name,
                 const std::list<std::string> &parameters, const ossl_ptr<EVP_PKEY> &ca_pkey, const ossl_ptr<X509> &ca_cert,
                 const ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

void onRevoke(ConfigCms &config, sql_ptr &ca_db, const std::string &our_issuer_id, server::SharedWildcardPV &status_pv, std::unique_ptr<server::ExecOp> &&op,
              const std::string &pv_name, const std::list<std::string> &parameters, const ossl_ptr<EVP_PKEY> &ca_pkey, const ossl_ptr<X509> &ca_cert,
              const ossl_shared_ptr<STACK_OF(X509)> &ca_chain, bool post_results = true);

void onApprove(ConfigCms &config, sql_ptr &ca_db, const std::string &our_issuer_id, server::SharedWildcardPV &status_pv, std::unique_ptr<server::ExecOp> &&op,
               const std::string &pv_name, const std::list<std::string> &parameters, const ossl_ptr<EVP_PKEY> &ca_pkey, const ossl_ptr<X509> &ca_cert,
               const ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

void onDeny(ConfigCms &config, sql_ptr &ca_db, const std::string &our_issuer_id, server::SharedWildcardPV &status_pv, std::unique_ptr<server::ExecOp> &&op,
            const std::string &pv_name, const std::list<std::string> &parameters, const ossl_ptr<EVP_PKEY> &ca_pkey, const ossl_ptr<X509> &ca_cert,
            const ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

int readOptions(ConfigCms &config, int argc, char *argv[], bool &verbose);

void updateCertificateStatus(sql_ptr &ca_db, uint64_t serial, certstatus_t cert_status,
                             std::vector<certstatus_t> valid_status = {PENDING_APPROVAL, PENDING, VALID});

certstatus_t storeCertificate(sql_ptr &ca_db, CertFactory &cert_factory);

void usage(const char *argv0);

void certificateStatusMonitor(ConfigCms &config, sql_ptr &ca_db, std::string &our_issuer_id, server::SharedWildcardPV &status_pv, pvxs::ossl_ptr<X509> &ca_cert,
                              pvxs::ossl_ptr<EVP_PKEY> &ca_pkey, pvxs::ossl_shared_ptr<STACK_OF(X509)> &ca_chain);

Value postCertificateStatus(server::SharedWildcardPV &status_pv, const std::string &pv_name, uint64_t serial, const CertificateStatus &cert_status,
                            bool open_only = false);
void postCertificateErrorStatus(server::SharedWildcardPV &status_pv, const std::string &our_issuer_id, const uint64_t &serial, int32_t error_status,
                                int32_t error_severity, const std::string &error_message);

std::string getCertUri(const std::string &prefix, const std::string &issuer_id, const uint64_t &serial);
std::string getCertUri(const std::string &prefix, const std::string &cert_id);
std::string getCertId(const std::string &issuer_id, const uint64_t &serial);
std::string getValidStatusesClause(std::vector<certstatus_t> valid_status);
void bindValidStatusClauses(sqlite3_stmt *sql_statement, std::vector<certstatus_t> valid_status);
std::tuple<std::string, uint64_t> getParameters(const std::list<std::string> &parameters);

template <typename T>
void setValue(Value &target, const std::string &field, const T &source);

}  // namespace certs
}  // namespace pvxs

#endif  // PVXS_PVACMS_H
