/*
 * Minimal performance test harness.
 * Starts pvacms from project_root/bin, prints progress messages, then stops it.
 * C++11, helpers in anonymous namespace inside pvxs namespace as requested.
 */

#include <algorithm>
#include <csignal>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

#ifdef __linux__
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <time.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <iostream>
#endif

#include <libgen.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <epicsVersion.h>
#include <osiFileName.h>

#include <pvxs/client.h>
#include <pvxs/log.h>
#include <pvxs/nt.h>
#include <pvxs/server.h>
#include <pvxs/sharedpv.h>

#include "openssl.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <pcap.h>
#endif

DEFINE_LOGGER(perf, "pvxs.perf");

namespace pvxs {
namespace {
using namespace pvxs::members;


#ifdef __linux__
// Return resident set size in bytes
std::uint64_t getRssBytes() {
    // Fast path: /proc/self/statm (field 2 = resident pages)
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages_res = 0;
    long pages_total = 0;
    if (std::fscanf(f, "%ld %ld", &pages_total, &pages_res) != 2) {
        std::fclose(f);
        return 0;
    }
    std::fclose(f);
    const long page_size = sysconf(_SC_PAGESIZE); // bytes per page
    return static_cast<std::uint64_t>(pages_res) * static_cast<std::uint64_t>(page_size);
}

// Return process CPU time (user+sys) in seconds
double procCPUSeconds() {
    timespec ts{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) return 0.0;
    return ts.tv_sec + ts.tv_nsec/1e9;
}

#elif defined(__APPLE__) || defined(__FreeBSD__)

// Return resident set size in bytes
std::uint64_t getRssBytes() {
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
        return 0;
    }
    return info.resident_size;
}

// Return process CPU time (user+sys) in seconds
double procCPUSeconds() {
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    double user = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec/1e6;
    double sys  = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec/1e6;
    return user + sys;
}

#endif

// Return wall clock (monotonic) in seconds
double wallSeconds() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec)/1e9;
}

// Sample CPU usage since prior reading (percentage of one core)
double cpuPercentSince(const double w0, const double c0) {
    const double w1 = wallSeconds();
    const double c1 = procCPUSeconds();
    const double dw = w1 - w0;
    const double dc = c1 - c0;
    return dw > 0.0 ? dc/dw*100.0 : 0.0;
}

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
// Packet capture helper to measure bytes for specific ports during an interval
struct PortSniffer {
    std::vector<pcap_t*> handles;
    std::string bpf;
    std::uint64_t total{0};
    PortSniffer()
        : bpf("(tcp or udp) and (port 55075 or port 55076)") {}

    static void onPacket(u_char* user, const struct pcap_pkthdr* h, const u_char* /*bytes*/) {
        auto* total = reinterpret_cast<std::uint64_t*>(user);
        *total += static_cast<std::uint64_t>(h->len);
    }

    bool openAll(std::string& err) {
        char ebuf[PCAP_ERRBUF_SIZE] = {0};
        pcap_if_t* alldevs = nullptr;
        if (pcap_findalldevs(&alldevs, ebuf) != 0) {
            err = ebuf;
            return false;
        }
        for (pcap_if_t* d = alldevs; d; d = d->next) {
            // skip interfaces that are down or not running capture
            // but include loopback as pvacms may use it
            pcap_t* h = pcap_open_live(d->name, 65535, 1 /*promisc*/, 100 /*ms*/, ebuf);
            if (!h) continue;
            bpf_program prog{};
            if (pcap_compile(h, &prog, bpf.c_str(), 1, PCAP_NETMASK_UNKNOWN) != 0) {
                pcap_close(h);
                continue;
            }
            if (pcap_setfilter(h, &prog) != 0) {
                pcap_freecode(&prog);
                pcap_close(h);
                continue;
            }
            pcap_freecode(&prog);
            handles.push_back(h);
        }
        pcap_freealldevs(alldevs);
        if (handles.empty()) {
            err = "pcap: no capture handles opened";
            return false;
        }
        return true;
    }

    void closeAll() {
        for (auto* h : handles) pcap_close(h);
        handles.clear();
    }

    void startCapture() {
        total = 0;

        std::string err;
        if (handles.empty() && !openAll(err)) {
            std::cerr << "PortSniffer init failed: " << err << std::endl;
            return;
        }
        for (auto* h : handles) {
            // process up to some number of packets per iteration to yield
            pcap_dispatch(h, 64, &PortSniffer::onPacket, reinterpret_cast<u_char*>(&total));
        }
    }

    std::uint64_t endCapture() {
        for (auto* h : handles) {
            // process up to some number of packets per iteration to yield
            pcap_dispatch(h, 64, &PortSniffer::onPacket, reinterpret_cast<u_char*>(&total));
        }
        return total;
    }

    ~PortSniffer() { closeAll(); }
};
#endif


enum ScenarioType {
    TCP,
    TLS,
    TLS_CMS,
    TLS_CMS_STAPLED
};

enum PayloadType {
    Scalar,
    SmallArray,
    LargeArray,
};

struct Result {
    epicsMutex lock;
    std::array<uint64_t, 60> counts;
    std::array<double, 60> values;
    double min;
    double max;

    void add(const uint index, const double value) {
        Guard G(lock);
        auto count = counts[index]++;
        if ( count ) {
            values[index] = value;
            min = max = value;
        } else {
            // Caluclate moving average
            values[index] = (values[index] * count + value)/(count+1);
            if (value < min) min = value;
            if (value > max) max = value;
        }
    }

    void print() const {
        for (const auto value: values) {
            std::cout << value << ", ";
        }
        std::cout << ", " << min << ", " << max;
    }
};

struct Scenario {
    server::Server serv;
    client::Context cli;
    server::SharedPV scalar_pv;
    server::SharedPV small_array_pv;
    server::SharedPV large_array_pv;
    Value scalar_value;
    Value small_array_value;
    Value large_array_value;

    Scenario(ScenarioType scenario_type) {
        // Build Server
        auto serv_conf = pvxs::server::Config::fromEnv();
        serv_conf.tls_keychain_file = "server1.p12";
        serv_conf.udp_port = 55076;
        serv_conf.tls_disabled = scenario_type == TCP;
        serv_conf.tls_disable_status_check = scenario_type < TLS_CMS;
        serv_conf.tls_disable_stapling = scenario_type < TLS_CMS_STAPLED;
        serv = serv_conf.build();

        // Build Client
        auto cli_conf(serv.clientConfig());
        cli_conf.tls_keychain_file = "client1.p12";
        cli_conf.tls_disable_status_check = scenario_type < TLS_CMS;
        cli = cli_conf.build();

        // Build PVs
        scalar_pv = server::SharedPV::buildReadonly();
        serv.addPV("PERF:SCALAR", scalar_pv);
        small_array_pv = server::SharedPV::buildReadonly();
        serv.addPV("PERF:SMALL_ARRAY", small_array_pv);
        large_array_pv = server::SharedPV::buildReadonly();
        serv.addPV("PERF:LARGE_ARRAY", large_array_pv);

        // Build data
        // 4 byte data payload (plus NT scaffolding)
        scalar_value = pvxs::nt::NTScalar{pvxs::TypeCode::Int32}.create();

        auto def(pvxs::nt::NTNDArray{}.build());
        def += { StructA("dimensions", { Int32("value"), }), };

        // 1k payloads (plus NT scaffolding)
        small_array_value = def.create();
        shared_array<const uint8_t> small_array_data({0,1,2,3,4,5,6,7,8,9});
        shared_array<pvxs::Value> small_dimensions;
        small_dimensions.resize(2);
        small_dimensions[0] = small_array_value["dimension"].allocMember() .update("size", 10);
        small_dimensions[1] = small_dimensions[0].cloneEmpty() .update("size", 10);

        small_array_value["value->ubyteValue"] = small_array_data;
        small_array_value["dimension"] = small_dimensions.freeze();

        // 100k payloads (plus NT scaffolding)
        large_array_value = def.create();
        pvxs::shared_array<const uint8_t> large_array_data({0,1,2,3,4,5,6,7,8,9});
        pvxs::shared_array<pvxs::Value> large_dimensions;
        large_dimensions.resize(3);
        large_dimensions[0] = large_array_value["dimension"].allocMember() .update("size", 100);
        large_dimensions[1] = large_dimensions[0].cloneEmpty() .update("size", 10);
        large_dimensions[2] = large_dimensions[1].cloneEmpty() .update("size", 10);

        large_array_value["value->ubyteValue"] = large_array_data;
        large_array_value["dimension"] = large_dimensions.freeze();

        serv.start();
    }

    ~Scenario() {
        serv.stop();
    }

    void run(const Scenario &scenario, const ScenarioType scenario_type, const PayloadType payload_type) {
        const auto payload_label = (payload_type == LargeArray ? "Large Array" : payload_type == SmallArray ? "Small Array" : "Scalar");
        run(scenario, scenario_type, payload_type, 1, payload_label, "  1 Hz");
        run(scenario, scenario_type, payload_type, 10, payload_label, " 10 Hz");
        run(scenario, scenario_type, payload_type, 100, payload_label, "100 Hz");
        run(scenario, scenario_type, payload_type, 1000, payload_label, "  1KHz");
        run(scenario, scenario_type, payload_type, 10000, payload_label, " 10KHz");
        run(scenario, scenario_type, payload_type, 100000, payload_label, "100KHz");
        run(scenario, scenario_type, payload_type, 1000000, payload_label, "  1MHz");
    }

    void run(const Scenario &scenario, const ScenarioType scenario_type, const PayloadType payload_type, const long updates_per_second, const std::string &payload_label, const std::string &speed_label) {
        Result result{};

        // Collect Data
        const double w0 = wallSeconds();
        const double c0 = procCPUSeconds();
        std::uint64_t bytes_captured = 0;
        {
            PortSniffer sniffer;
            sniffer.startCapture();

            // Run Tests
            sleep (5);
            // End of Tests
            bytes_captured = sniffer.endCapture();
        }

        const double rss_mb = static_cast<double>(getRssBytes()) / (1024 * 1024);
        const auto cpu_percent = cpuPercentSince(w0, c0);

        // Display Data
        std::cout << payload_label << ", "  << speed_label << ", ";
        result.print();
        std::cout << ", " << cpu_percent << ", " << rss_mb << ",  " << bytes_captured << std::endl;
    }
};

/**
 * Extract target architecture from the given test executable path name
 *
 * @param path
 * @return
 */
std::string extractTargetArch(const std::string& path)
{
    if ( path.empty() ) return std::string();
    auto terminated_by_path_separator = ( path.back() == '/' ) ;
    const auto base_path = path.substr(0,std::string::npos - (terminated_by_path_separator?1:0));
    std::string target_arch = basename(const_cast<char *>(base_path.c_str()));
    const auto ta= target_arch.substr(2,std::string::npos);
    log_debug_printf(perf, "Target architecture: %s\n", ta.c_str());
    return ta;
}

struct Child {
    pid_t pid;
    const std::vector<std::string> env{};

    Child() : pid(-1) {}

    explicit Child(const std::initializer_list<std::string> &key_value_pairs)
        : pid(-1), env(key_value_pairs) {}

};

bool startChild(const std::string& child_process, Child& child)
{
    const pid_t pid = fork();
    if (pid < 0) {
        // Failure to create the subprocess
        return false;
    }

    if (pid == 0) {
        // Child process
        // Detach from any controlling terminal group so signals don't propagate unexpectedly
        setsid();

        // Apply environment setup
        std::string key{};
        for (const auto &env_part : child.env) {
            if (key.empty()) {
                key = env_part;
            } else {
                if ( env_part.empty()) {
                    if (unsetenv(key.c_str()) != 0) {
                        log_err_printf(perf, "Failed to unset environment variable: %s \n", key.c_str());
                    }
                } else {
                    if (setenv(key.c_str(), env_part.c_str(), 1) != 0) {
                        log_err_printf(perf, "Failed to set environment variable: %s = \"%s\"\n", key.c_str(), env_part.c_str());
                    }
                }
                key = {};
            }
        }

        const char* argv0 = child_process.c_str();
        log_info_printf(perf, "Starting child process: %s %s\n", child_process.c_str(), "pvacms");
        execlp(argv0, "pvacms",  nullptr);

        // If exec fails
        log_err_printf(perf, "Failed to start child process: %s %s\n", child_process.c_str(), "pvacms");
        _exit(127);
    }

    // Parent process
    child.pid = pid;
    return true;
}

 void stopChild(Child& child)
{
    if (child.pid > 0) {
        // Try SIGTERM first
        kill(child.pid, SIGTERM);
        // Wait briefly
        for (int i = 0; i < 30; ++i) {
            int status = 0;
            pid_t r = waitpid(child.pid, &status, WNOHANG);
            if (r == child.pid) {
                child.pid = -1;
                return;
            }
            usleep(100000); // 100ms
        }
        // Force kill if still running
        kill(child.pid, SIGKILL);
        waitpid(child.pid, 0, 0);
        child.pid = -1;
    }
}

} // anonymous namespace
} // namespace pvxs

int main(int argc, char* argv[])
{
#if defined(EPICS_VERSION_INT) && EPICS_VERSION_INT >= VERSION_INT(7, 0, 3, 1)
    (void)argc; (void)argv;
    pvxs::logger_level_set(perf.name, pvxs::Level::Info);
    pvxs::logger_config_env();


    std::cout << "Starting Performance Tests" << std::endl;

    // Determine test install dir
    std::string test_dir;
    char *executable_path = epicsGetExecDir();
    if(executable_path) {
        try {
            test_dir = executable_path;
            free(executable_path);
        } catch(...) {
            free(executable_path);
            throw;
        }
    }

    // Change working dir to test dir
    if ( chdir(test_dir.c_str()) ) {
        std::cerr << "Failed to change to test directory: " <<test_dir << std::endl;
        return 2;
    }

    // Extract the target architecture from the test directory name
    const std::string target_arch = pvxs::extractTargetArch(test_dir);

    // Determine the pvacms executable location
    const std::string pvacms_executable_path = test_dir
        + ".."
        + OSI_PATH_SEPARATOR + ".."
        + OSI_PATH_SEPARATOR + "bin"
        + OSI_PATH_SEPARATOR + target_arch
        + OSI_PATH_SEPARATOR + "pvacms";
    std::cout << "pvacms executable: " << pvacms_executable_path << std::endl;

    // Create a child process to run PVACMS
    pvxs::Child pvacms_subprocess{
        "SSLKEYLOGFILE",                  {},
        "XDG_DATA_HOME",                    test_dir+"perf/data",
        "XDG_CONFIG_HOME",                  test_dir+"perf/config",
        "EPICS_PVA_BROADCAST_PORT",         "55076",
        "EPICS_PVA_SERVER_PORT",            "55075",
        "EPICS_PVA_TLS_PORT",               "55076",
        "EPICS_CERT_AUTH_TLS_KEYCHAIN",     "cert_auth.p12",
        "EPICS_PVAS_TLS_KEYCHAIN",          "superserver1.p12",
    };
    if (!pvxs::startChild(pvacms_executable_path, pvacms_subprocess)) {
        std::cerr << "Failed to start pvacms: " << pvacms_executable_path << std::endl;
        return 1;
    }

    // Wait for pvacms to start up
    std::cout << "Waiting for pvacms to start before running tests" << std::endl;
    sleep (2);
    std::cout << "PVACMS Ready" << std::endl;

    // Run all scenarios
    for (auto scenario_type = pvxs::TCP;
        scenario_type <= pvxs::TLS_CMS_STAPLED;
        scenario_type = static_cast<pvxs::ScenarioType>(static_cast<int>(scenario_type) + 1)) {
        std::cout << "+=======================================+=======================================" << std::endl;
        std::cout << "Scenario: " << (
            scenario_type == pvxs::TLS_CMS_STAPLED ? "TLS with stapled status" :
            scenario_type == pvxs::TLS_CMS ? "TLS with status":
            scenario_type == pvxs::TLS ? "TLS no status": "TCP") << std::endl;

        std::cout << "Configuring Performance Tests" << std::endl;
        auto scenario = pvxs::Scenario(scenario_type);

        std::cout << "Running Performance Tests" << std::endl;
        std::cout << "+=======================================+=======================================" << std::endl;

        std::cout << "Starting Test" << std::endl;
        std::cout << "+=======================================+=======================================" << std::endl;
        std::cout << "           ,  ,"
                  << "1,2,3,4,5,6,7,8,9,10,"
                  << "11,12,13,14,15,16,17,18,19,20,"
                  << "21,22,23,24,25,26,27,28,29,30,"
                  << "31,32,33,34,35,36,37,38,39,40,"
                  << "41,42,43,44,45,46,47,48,49,50,"
                  << "51,52,53,54,55,56,57,58,59,60,"
                  << "min,max,"
                  << "cpu(%),mem(MB),wire(bytes)"
                  << std::endl;
        for (auto payload_type = pvxs::Scalar;
            payload_type <= pvxs::LargeArray;
            payload_type = static_cast<pvxs::PayloadType>(static_cast<int>(payload_type) + 1)) {
            scenario.run(scenario, scenario_type, payload_type);
        }
        std::cout << "+=======================================+=======================================" << std::endl;
        std::cout << "Test Complete" << std::endl;
        std::cout << std::endl;
    }

    pvxs::stopChild(pvacms_subprocess);

    std::cout << "Performance Tests Complete" << std::endl;
#endif
    return 0;
}
