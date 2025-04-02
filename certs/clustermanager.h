/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CLUSTER_MANAGER_H
#define CLUSTER_MANAGER_H

#include <cstdint>
#include <ctime>
#include <unordered_map>

#include <epicsTime.h>

#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "openssl/sha.h"
#include "openssl/x509.h"

#include "ownedptr.h"
#include "pvacms.h"

namespace pvxs {
namespace certs {

#define CLUSTER_VERSION 1

#define SKID_BINARY_LENGTH 8
#define SKID_HEX_LENGTH (SKID_BINARY_LENGTH * 2)
#define SKID_STRING_LENGTH (SKID_HEX_LENGTH + 1)

#define CERT_LIST_PV_PREFIX "CERT:LIST"                              // Full cert database
#define CERT_UPDATES_PV_PREFIX "CERT:UPDATES"                        // Updates stream

#pragma pack(push, 1)

// Full certificate information for creation/initial sync
struct CertStatusSyncEntry {
    epicsTimeStamp ts;              // Update timestamp
    uint64_t serial;                // Certificate serial
    char skid[SKID_STRING_LENGTH];  // SKID as hex string
    uint8_t status;                 // enum status
    time_t status_date;             // Status change date
    time_t not_before;              // Validity start
    time_t not_after;               // Validity end
    bool approved;                  // Approval flag
    char CN[256];                   // Common Name
    char O[256];                    // Organization
    char OU[256];                   // Organizational Unit
    char C[3];                      // Country (2 chars + null)
};

// Minimal structure for status updates
struct CertStatusUpdateSyncEntry {
    epicsTimeStamp ts;   // Update timestamp
    uint64_t serial;     // Certificate serial
    uint8_t status;      // enum status
    time_t status_date;  // Status change date
    bool approved;       // Approval flag
};

// Header for both types of sync
struct CertDbSyncHeader {
    uint8_t version{CLUSTER_VERSION};
    bool is_update;         // true for update, false for create
    uint32_t record_size;   // Size depends on is_update
    uint32_t record_count;  // Number of records
    uint32_t sig_length;
};

#pragma pack(pop)

class ClusterManager {
 public:
    ClusterManager(const ossl_ptr<EVP_PKEY>& cert_auth_pkey,
                   sql_ptr& certs_db,
                   const std::string& issuer_id,
                   const std::string& my_node_id)
        : cert_auth_pkey_(cert_auth_pkey),
          sig_size(EVP_PKEY_size(cert_auth_pkey.get())),
          issuer_id(issuer_id),
          my_node_id_(my_node_id),
          cert_list_pv_name_((SB() << CERT_LIST_PV_PREFIX << ":" << issuer_id << ":????????").str()),
          cert_updates_pv_name_((SB() << CERT_UPDATES_PV_PREFIX << ":" << issuer_id << ":????????").str()),
          my_cert_list_pv_name_((SB() << CERT_LIST_PV_PREFIX << ":" << issuer_id << ":" << my_node_id).str()),
          my_cert_updates_pv_name_((SB() << CERT_UPDATES_PV_PREFIX << ":" << issuer_id << ":" << my_node_id).str()),
          client_(client::Context::fromEnv()) {
        // Load all certificates from the database
        loadAllCertificates(certs_db);

        // Join the cluster
        joinCluster();
    }

    ~ClusterManager() {
        client_.close();
    }

    // Create a new update
    shared_array<uint8_t> createDbSyncUpdateList(const CertStatusUpdateSyncEntry& new_update = nullptr);
    void setDbSyncUpdateListValue(shared_array<uint8_t>& new_update);

    // Create a new create
    shared_array<uint8_t> createDbSyncCertsList(const CertStatusSyncEntry& new_create = nullptr);

    // Merge incoming certificates with existing ones
    void mergeCertificates(const std::vector<CertStatusSyncEntry>& incoming_creates);

    // Merge incoming updates with existing ones
    void mergeUpdates(const std::vector<CertStatusUpdateSyncEntry>& incoming_updates);

 private:
    std::string nodeIdFromPvName(const std::string& pv_name) {
        return pv_name.substr(pv_name.find_last_of(":") + 1);
    }

    void joinCluster();

    void onClusterUpdate(const std::vector<std::string>& node_ids);
    bool shouldSubscribeTo(const std::string& my_node_id,
                           const std::string& target_node_id,
                           const std::vector<std::string>& all_node_ids);

    void subscribeToCertList(const std::string& node_id);
    void subscribeToUpdates(const std::string& node_id);
    void unsubscribeFrom(const std::string& node_id);
    bool isSubscribedTo(const std::string& node_id);

    void loadAllCertificates(sql_ptr& certs_db);

    bool verifyDbSyncUpdate(const uint8_t* buffer,
                            size_t bufferSize,
                            std::vector<CertStatusUpdateSyncEntry>& updates,
                            const ossl_ptr<EVP_PKEY>& pkey);

    bool verifyDbSyncCreate(const uint8_t* buffer,
                            size_t bufferSize,
                            std::vector<CertStatusSyncEntry>& creates,
                            const ossl_ptr<EVP_PKEY>& pkey);

    /**
     * @brief Server for hosting the certs and updates PVs
     */
    server::Server server_;

    /**
     * @brief Name of the cert list PV: CERT:LIST:{issuer_id}:????????
     */
    const std::string cert_list_pv_name_;

    /**
     * @brief Name of the cert updates PV: CERT:UPDATES:{issuer_id}:????????
     */
    const std::string cert_updates_pv_name_;

    /**
     * @brief Name of the cert list PV: CERT:LIST:{issuer_id}:{my_node_id}
     */
    const std::string my_cert_list_pv_name_;

    /**
     * @brief Name of the peer cert list PV: CERT:LIST:{issuer_id}:{peer_node_id}
     */
    const std::string peer_cert_list_pv_name_;

    /**
     * @brief Name of the cert updates PV: CERT:UPDATES:{issuer_id}:{my_node_id}
     */ 
    const std::string my_cert_updates_pv_name_;

    /**
     * @brief Name of the peer cert updates PV: CERT:UPDATES:{issuer_id}:{peer_node_id}
     */
    const std::string peer_cert_updates_pv_name_;

    Value certs_list_value_ = nt::NTScalar{TypeCode::UInt8Array}.create();
    Value cert_updates_value_ = nt::NTScalar{TypeCode::UInt8Array}.create();

    /**
     * @brief SharedWildcardPV for the cert list PV
     *
     * This PV is used to send the certificate list and any new certificates created in this node to the cluster.
     *
     * It is important to know that when we use these PVs we need to add an exclusion for
     * our own node id. Otherwise, we will end up eating our own updates.
     *
     * Also we need to make sure that connection and disconnection events are NOT masked.
     * Otherwise, we will not be able to detect when a node joins or leaves the cluster.
     */
    server::SharedWildcardPV cert_list_pv_{server::SharedWildcardPV::buildMailbox()};

    /**
     * @brief SharedWildcardPV for the cert updates PV
     *
     * This PV is used to send certificate status updates to the cluster.
     *
     * It is important to know that when we use this PV we need to add an exclusion for
     * our own node id. Otherwise, we will end up eating our own updates.
     */
    server::SharedWildcardPV cert_updates_pv_{server::SharedWildcardPV::buildMailbox()};

    /**
     * @brief Issuer id
     *
     * This is the id of the Certificate Authority that is used to sign the certificates.
     */
    const std::string issuer_id;

    /**
     * @brief My node id
     *
     * This is the id of this node which is the skid of this node's certificate.
     */
    const std::string my_node_id_;

    /**
     * @brief Certificate authority private key
     *
     * This is the private key of the Certificate Authority that is used to sign certificate list and the updates.
     * This signature is used to verify the integrity of the certificate list and the updates.
     */
    const ossl_ptr<EVP_PKEY>& cert_auth_pkey_;

    /**
     * @brief Certificate status updates
     *
     * This is a list of certificate status updates.
     */
    std::vector<CertStatusUpdateSyncEntry> cert_status_updates_{};

    /**
     * @brief Certificate list
     *
     * This is a list of certificates.  Will eventually be consistent across all nodes.
     */
    std::vector<CertStatusSyncEntry> certs_{};

    /**
     * @brief Header size
     *
     * This is the size of the header of the wire format of the certificate and the updates lists.
     */
    constexpr static size_t kHeaderSize = sizeof(CertDbSyncHeader);

    /**
     * @brief Update size
     *
     * This is the size of a single entry in the wire format of the update list.
     */
    constexpr static size_t kUpdateEntrySize = sizeof(CertStatusUpdateSyncEntry);

    /**
     * @brief Create size
     *
     * This is the size of a single entry in the wire format of the certificates list.
     */
    constexpr static size_t kCertsEntrySize = sizeof(CertStatusSyncEntry);

    /**
     * @brief Signature size
     *
     * This is the size of the signature for the wire format of the certificate and updates lists.
     */
    const size_t sig_size;

    /**
     * @brief Client context
     *
     * This is the client context used to subscribe for updates from the cluster.
     */
    client::Context client_;

    /**
     * @brief Updates subscription
     *
     * This is the subscription to the cluster's certificate status updates.
     */
    std::shared_ptr<client::Subscription> updates_sub_;

    /**
     * @brief Certificates subscription
     *
     * This is the subscription to the cluster's certificate list and its updates.
     */
    std::shared_ptr<client::Subscription> certs_sub_;

    /**
     * @brief Subscribed nodes
     *
     * This is a map of subscribed nodes.
     */
    std::unordered_map<std::string, std::shared_ptr<client::Subscription>> subscribed_nodes_{};
};

}  // namespace certs
}  // namespace pvxs

#endif  // CLUSTER_MANAGER_H
