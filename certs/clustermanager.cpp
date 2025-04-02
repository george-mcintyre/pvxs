/**
* Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "clustermanager.h"

#include <openssl/evp.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>

#include "pvacms.h"
#include "utilpvt.h"

namespace pvxs {
namespace certs {

void ClusterManager::joinCluster() {
    // 1. Create a new server for hosting the certs and updates PVs
    server_ = server::Server::fromEnv();


    // 2. Add the PVs to the server make sure to exclude own PVs so we'll get updates only from other nodes
    // We will add peers to the exlusions list when they connect
    server_
    .addPV(cert_list_pv_name_, my_cert_list_pv_name_, cert_list_pv_)
    .addPV(cert_updates_pv_name_, my_cert_updates_pv_name_, cert_updates_pv_);


    // 3. Create the initial values for the PVs
    auto certs_list_bytes = createDbSyncCertsList();
    certs_list_value_["value"] = certs_list_bytes.freeze();

    auto cert_updates_bytes = createDbSyncUpdateList();
    cert_updates_value_["value"] = cert_updates_bytes.freeze();

    // 4. Subscribe to cluster PV to discover nodes
    certs_sub_ = client_.monitor(my_cert_list_pv_name_)
                       .maskConnected(false)
                       .maskDisconnected(false)
                       .event([](client::Subscription& sub) {
                           onClusterUpdate(sub.pop());
                       })
                       .exec();

    if (nodes.empty()) {
        // First node - just publish everything
        publishClusterList({my_node_id_});
        publishCertList(certs_);
        publishUpdates(cert_status_updates_);
    } else if (nodes.size() == 1) {
        // Second node - subscribe to first node and merge
        subscribeToCertList(nodes[0]);
        subscribeToUpdates(nodes[0]);

        // After merge, publish with updated cluster list
        publishClusterList({nodes[0], my_node_id_});
        publishCertList(certs_);
        publishUpdates(cert_status_updates_);
    } else {
        // Third+ node - subscribe to all existing nodes
        for (const auto& node : nodes) {
            subscribeToCertList(node);
            subscribeToUpdates(node);
        }

        // After merge, publish with updated cluster list
        auto new_cluster = nodes;
        new_cluster.push_back(my_node_id_);
        publishClusterList(new_cluster);
        publishCertList(certs_);
        publishUpdates(cert_status_updates_);
    }
}

    // Called when cluster PV updates
void ClusterManager::onClusterUpdate(const std::vector<std::string>& node_ids) {
    if (node_ids.size() >= 2) {
        // Rebalance subscriptions
        for (const auto& node_id : node_ids) {
            if (node_id != my_node_id_ &&
                shouldSubscribeTo(my_node_id_, node_id, node_ids)) {
                if (!isSubscribedTo(node_id)) {
                    subscribeToCertList(node_id);
                    subscribeToUpdates(node_id);
                }
            } else {
                if (isSubscribedTo(node_id)) {
                    unsubscribeFrom(node_id);
                }
            }
        }
    }
}

bool ClusterManager::shouldSubscribeTo(
    const std::string& my_node_id, const std::string& target_node_id,
    const std::vector<std::string>& all_node_ids) {
    // Your balancing algorithm here
    // For 3+ nodes, ensure optimal subscription pattern
    auto sorted_nodes = all_node_ids;
    std::sort(sorted_nodes.begin(), sorted_nodes.end());

    size_t my_pos =
        std::find(sorted_nodes.begin(), sorted_nodes.end(), my_node_id) -
        sorted_nodes.begin();
    size_t target_pos =
        std::find(sorted_nodes.begin(), sorted_nodes.end(), target_node_id) -
        sorted_nodes.begin();

    // With 3 nodes, everyone connects to everyone
    if (sorted_nodes.size() == 3) return true;

    // With more nodes, use ring with redundancy
    size_t total_nodes = sorted_nodes.size();
    size_t subs_per_node = 2;

    for (size_t i = 1; i <= subs_per_node; i++) {
        if ((my_pos + i) % total_nodes == target_pos) {
            return true;
        }
    }
    return false;
}

void ClusterManager::subscribeToCertList(const std::string& node_id) {
    const std::string issuer_id;
    auto pv_name = std::string.format(CERT_LIST_PV_FMT, issuer_id.c_str(), node_id.c_str());


    // with TLS disabled to avoid recursive loop
    auto client(std::make_shared<client::Context>(client::Context::fromEnv(true)));
    cert_status_ptr<CertStatusManager> cert_status_manager(new CertStatusManager(std::move(client)));
    cert_status_manager->callback_ref = std::move(fn);
    std::weak_ptr<CertStatusManager> weak_cert_status_manager(cert_status_manager);

    log_debug_printf(status, "Subscribing to peer status: %s", "");
    auto sub = cert_status_manager->client_->monitor(status_pv)
                    .maskConnected(true)
                    .maskDisconnected(true)
                    .event([trusted_store_ptr, weak_cert_status_manager](client::Subscription &sub) {
                        try {
                            auto cert_status_manager = weak_cert_status_manager.lock();
                            if (!cert_status_manager) return;
                            auto update = sub.pop();
                            if (update) {
                                try {
                                    auto status_update{PVACertificateStatus(update, trusted_store_ptr)};
                                    log_debug_printf(status, "Status subscription received: %s\n", status_update.status.s.c_str());
                                    cert_status_manager->status_ = std::make_shared<CertificateStatus>(status_update);
                                    (*cert_status_manager->callback_ref)(status_update);
                                } catch (OCSPParseException &e) {
                                    log_debug_printf(status, "Ignoring invalid status update: %s\n", e.what());
                                } catch (std::invalid_argument &e) {
                                    log_debug_printf(status, "Ignoring invalid status update: %s\n", e.what());
                                } catch (std::exception &e) {
                                    log_err_printf(status, "%s\n", e.what());
                                }
                            }
                        } catch (client::Finished &conn) {
                            log_debug_printf(status, "Subscription Finished: %s\n", conn.what());
                        } catch (client::Connected &conn) {
                            log_debug_printf(status, "Connected Subscription: %s\n", conn.peerName.c_str());
                        } catch (client::Disconnect &conn) {
                            log_debug_printf(status, "Disconnected Subscription: %s\n", conn.what());
                        } catch (std::exception &e) {
                            log_err_printf(status, "Error Getting Subscription: %s\n", e.what());
                        }
                    })
                    .exec();
    cert_status_manager->subscribe(sub);
    log_debug_printf(status, "subscription address: %p\n", cert_status_manager.get());
    return cert_status_manager;
}



bool ClusterManager::verifyDbSyncUpdate(const uint8_t* buffer,
                                        size_t buffer_size,
                                        std::vector<CertStatusUpdateSyncEntry>& updates,
                                        const ossl_ptr<EVP_PKEY>& pkey) {
    if (buffer_size < sizeof(CertDbSyncHeader)) return false;

    const CertDbSyncHeader* header =
        reinterpret_cast<const CertDbSyncHeader*>(buffer);

    // Calculate expected sizes
    size_t headerSize = sizeof(CertDbSyncHeader);
    size_t recordsSize = header->record_count * header->record_size;
    size_t expectedSize = headerSize + recordsSize + header->sig_length;

    if (buffer_size != expectedSize) return false;

    // Verify signature
    if (!verify(buffer, headerSize + recordsSize,
                buffer + headerSize + recordsSize, header->sig_length, pkey))
        return false;

    // Copy records
    updates.resize(header->record_count);
    std::memcpy(updates.data(), buffer + headerSize, recordsSize);

    return true;
}

shared_array<uint8_t> ClusterManager::createDbSyncUpdateList(
    const CertStatusUpdateSyncEntry& new_update) {
    if (new_update) cert_status_updates_.push_back(new_update);

    // Calculate sizes
    size_t records_size = cert_status_updates_.size() * kUpdateEntrySize;
    size_t total_size = kHeaderSize + records_size + sig_size;

    // Prepare buffer
    shared_array<uint8_t> buffer(total_size);

    // Fill header
    CertDbSyncHeader* header =
        reinterpret_cast<CertDbSyncHeader*>(buffer.data());
    header->version = VERSION;
    header->is_update = true;
    header->record_size = sizeof(CertStatusUpdateSyncEntry);
    header->record_count = cert_status_updates_.size();
    header->sig_length = sig_size;

    // Copy records directly after header
    std::memcpy(buffer.data() + kHeaderSize, cert_status_updates_.data(), records_size);

    // Sign
    CertFactory::sign(cert_auth_pkey_, buffer);  // signature is plonked at the end

    return buffer;
}

shared_array<uint8_t> ClusterManager::createDbSyncCertsList(
    const CertStatusSyncEntry& new_create) {
    if (new_create) certs_.push_back(new_create);

    // Calculate sizes
    size_t records_size = certs_.size() * kCertsEntrySize;
    size_t total_size = kHeaderSize + records_size + sig_size;

    // Prepare buffer
    shared_array<uint8_t> buffer(total_size);

    // Fill header
    CertDbSyncHeader* header =
        reinterpret_cast<CertDbSyncHeader*>(buffer.data());
    header->version = VERSION;
    header->is_update = false;
    header->record_size = sizeof(CertStatusSyncEntry);
    header->record_count = certs_.size();
    header->sig_length = sig_size;

    // Copy records directly after header
    std::memcpy(buffer.data() + kHeaderSize, certs_.data(), records_size);

    // Sign
    CertFactory::sign(cert_auth_pkey_, buffer);  // signature is plonked at the end

    return buffer;
}

void ClusterManager::loadAllCertificates(sql_ptr& certs_db) {
    sqlite3_stmt* stmt;

    // Load all certificates from the database
    if (sqlite3_prepare_v2(certs_db.get(), SQL_LOAD_ALL_CERTIFICATES, -1, &stmt,
                           nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CertStatusSyncEntry cert{};
            cert.serial = *reinterpret_cast<const uint64_t*>(
                &sqlite3_column_int64(stmt, 0));
            strncpy(cert.skid,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                    SKID_STRING_LENGTH - 1);
            cert.status = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
            cert.status_date = sqlite3_column_int64(stmt, 3);
            cert.not_before = sqlite3_column_int64(stmt, 4);
            cert.not_after = sqlite3_column_int64(stmt, 5);
            cert.approved = sqlite3_column_int(stmt, 6) != 0;
            strncpy(cert.CN,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)),
                    255);
            strncpy(cert.O,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)),
                    255);
            strncpy(cert.OU,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)),
                    255);
            strncpy(
                cert.C,
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)),
                2);
            certs_.push_back(cert);
        }
        sqlite3_finalize(stmt);
    }
}

void ClusterManager::mergeCertificates(
    const std::vector<CertStatusSyncEntry>& incoming_creates, sql_ptr& certs_db,
    server::SharedWildcardPV& status_pv, const std::string& issuer_id,
    const CertStatusFactory& cert_status_creator) {
    // Create a map of existing certificates by serial number for quick lookup
    std::unordered_map<uint64_t, size_t> existing_map;
    for (size_t i = 0; i < certs_.size(); ++i) {
        existing_map[certs_[i].serial] = i;
    }

    // Process each incoming certificate
    for (const auto& incoming : incoming_creates) {
        auto it = existing_map.find(incoming.serial);

        if (it == existing_map.end()) {
            // New certificate - add it to memory and database
            certs_.push_back(incoming);

            // Insert into database
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(certs_db.get(), SQL_CREATE_CERT, -1, &stmt,
                                   nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":serial"),
                    incoming.serial);
                sqlite3_bind_text(stmt,
                                  sqlite3_bind_parameter_index(stmt, ":skid"),
                                  incoming.skid, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt,
                                  sqlite3_bind_parameter_index(stmt, ":CN"),
                                  incoming.CN, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt,
                                  sqlite3_bind_parameter_index(stmt, ":O"),
                                  incoming.O, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt,
                                  sqlite3_bind_parameter_index(stmt, ":OU"),
                                  incoming.OU, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt,
                                  sqlite3_bind_parameter_index(stmt, ":C"),
                                  incoming.C, -1, SQLITE_STATIC);
                sqlite3_bind_int(
                    stmt, sqlite3_bind_parameter_index(stmt, ":approved"),
                    incoming.approved ? 1 : 0);
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":not_before"),
                    incoming.not_before);
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":not_after"),
                    incoming.not_after);
                sqlite3_bind_int(stmt,
                                 sqlite3_bind_parameter_index(stmt, ":status"),
                                 incoming.status);
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":status_date"),
                    incoming.status_date);

                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    log_err_printf(pvacms, "Failed to insert certificate: %s\n",
                                   sqlite3_errmsg(certs_db.get()));
                }
                sqlite3_finalize(stmt);

                // Post status update for new certificate
                try {
                    const std::string pv_name(
                        getCertUri(GET_MONITOR_CERT_STATUS_ROOT, issuer_id,
                                   incoming.serial));
                    auto cert_status =
                        cert_status_creator.createPVACertificateStatus(
                            incoming.serial,
                            static_cast<certstatus_t>(incoming.status),
                            incoming.status_date);
                    postCertificateStatus(status_pv, pv_name, incoming.serial,
                                          cert_status);
                    log_debug_printf(
                        pvacmsmonitor, "%s ==> NEW\n",
                        getCertId(issuer_id, incoming.serial).c_str());
                } catch (const std::runtime_error& e) {
                    log_err_printf(pvacmsmonitor,
                                   "PVACMS Certificate Monitor Error: %s\n",
                                   e.what());
                }
            }
        } else {
            // Existing certificate - compare timestamps
            auto& existing = certs_[it->second];

            // Compare timestamps - if incoming is newer, update existing in
            // memory only No need to update DB since matching serials mean
            // identical data
            if (incoming.ts.secPastEpoch > existing.ts.secPastEpoch ||
                (incoming.ts.secPastEpoch == existing.ts.secPastEpoch &&
                 incoming.ts.nsec > existing.ts.nsec)) {
                existing = incoming;
            }
        }
    }
}

void ClusterManager::mergeUpdates(
    const std::vector<CertStatusUpdateSyncEntry>& incoming_updates, sql_ptr& certs_db,
    server::SharedWildcardPV& status_pv, const std::string& issuer_id,
    const CertStatusFactory& cert_status_creator) {
    // Create a map of existing updates by serial number for quick lookup
    std::unordered_map<uint64_t, size_t> existing_map;
    for (size_t i = 0; i < cert_status_updates_.size(); ++i) {
        existing_map[cert_status_updates_[i].serial] = i;
    }

    // Process each incoming update
    for (const auto& incoming : incoming_updates) {
        auto it = existing_map.find(incoming.serial);

        // Skip updates for non-existent certificates
        if (it == existing_map.end()) {
            log_debug_printf(pvacmsmonitor,
                             "%s ==> SKIPPED (no such certificate)\n",
                             getCertId(issuer_id, incoming.serial).c_str());
            continue;
        }

        // Existing update - compare timestamps
        auto& existing = cert_status_updates_[it->second];

        // Compare timestamps - if incoming is newer, update both memory and DB
        if (incoming.ts.secPastEpoch > existing.ts.secPastEpoch ||
            (incoming.ts.secPastEpoch == existing.ts.secPastEpoch &&
             incoming.ts.nsec > existing.ts.nsec)) {
            existing = incoming;

            // Update database since we have a newer timestamp
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(certs_db.get(),
                                   SQL_CERT_SET_STATUS_W_APPROVAL, -1, &stmt,
                                   nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt,
                                 sqlite3_bind_parameter_index(stmt, ":status"),
                                 incoming.status);
                sqlite3_bind_int(
                    stmt, sqlite3_bind_parameter_index(stmt, ":approved"),
                    incoming.approved ? 1 : 0);
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":status_date"),
                    incoming.status_date);
                sqlite3_bind_int64(
                    stmt, sqlite3_bind_parameter_index(stmt, ":serial"),
                    incoming.serial);

                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    log_err_printf(pvacms,
                                   "Failed to update certificate status: %s\n",
                                   sqlite3_errmsg(certs_db.get()));
                }
                sqlite3_finalize(stmt);

                // Post status update since we updated the DB
                try {
                    const std::string pv_name(
                        getCertUri(GET_MONITOR_CERT_STATUS_ROOT, issuer_id,
                                   incoming.serial));
                    auto cert_status =
                        cert_status_creator.createPVACertificateStatus(
                            incoming.serial,
                            static_cast<certstatus_t>(incoming.status),
                            incoming.status_date);
                    postCertificateStatus(status_pv, pv_name, incoming.serial,
                                          cert_status);
                    log_debug_printf(
                        pvacmsmonitor, "%s ==> UPDATED\n",
                        getCertId(issuer_id, incoming.serial).c_str());
                } catch (const std::runtime_error& e) {
                    log_err_printf(pvacmsmonitor,
                                   "PVACMS Certificate Monitor Error: %s\n",
                                   e.what());
                }
            }
        }
    }
}

}  // namespace certs
}  // namespace pvxs
