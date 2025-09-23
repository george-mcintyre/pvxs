/*
 * Minimal performance test harness.
 * Starts pvacms from project_root/bin, prints progress messages, then stops it.
 * C++11, helpers in anonymous namespace inside pvxs namespace as requested.
 */

#include <algorithm>
#include <csignal>
#include <signal.h>
#include <iostream>
#include <libgen.h>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <pvxs/log.h>

#include "osiFileName.h"

DEFINE_LOGGER(perf, "pvxs.perf");

namespace pvxs {
namespace {

volatile sig_atomic_t g_stop_requested = 0;

void on_signal(int sig)
{
    (void)sig;
    g_stop_requested = 1;
}

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
    (void)argc; (void)argv;
    pvxs::logger_level_set(perf.name, pvxs::Level::Info);
    pvxs::logger_config_env();

    // Install SIGINT handler to request stop
    struct sigaction sa{};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = pvxs::on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

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
        "EPICS_PVAS_BROADCAST_PORT",        "55076",
        "EPICS_PVAS_SERVER_PORT",           "55075",
        "EPICS_PVAS_TLS_PORT",              "55076",
        "EPICS_CERT_AUTH_TLS_KEYCHAIN",     "cert_auth.p12",
        "EPICS_PVAS_TLS_KEYCHAIN",          "superserver1.p12",
        // "PVXS_LOG",                         "pvxs.*=DEBUG",
    };
    if (!pvxs::startChild(pvacms_executable_path, pvacms_subprocess)) {
        std::cerr << "Failed to start pvacms: " << pvacms_executable_path << std::endl;
        return 1;
    }

    std::cout << "Running Performance Tests" << std::endl;

    // Wait until SIGINT is received to request stop
    while (!pvxs::g_stop_requested) {
        pause(); // interrupted by signal
    }

    pvxs::stopChild(pvacms_subprocess);

    std::cout << "Performance Tests Complete" << std::endl;

    return 0;
}
