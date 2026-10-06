#include "TestHarness.h"
#include <sys/sysctl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <cstdlib>
#include <optional>
#include <string>
#include "source/server/HeadlessServer.h"
#include "source/server/AuthoritativeMatchCli.h"
#include <sstream>

extern "C" void d6rRandomFailureAt(unsigned);
extern "C" unsigned d6rRandomCalls();

namespace {
    enum class Restriction { None, NoAes, NoSimd, QueryFailure, ShortResult };
    Restriction restriction = Restriction::None;
    unsigned initializations = 0;
    unsigned curlInitializations = 0;
    CURLcode observedCurlInit(long flags) { ++curlInitializations; return curl_global_init(flags); }
    struct SocketPair {
        int descriptors[2]{-1, -1};
        SocketPair() { D6R_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0); }
        ~SocketPair() { for (int descriptor : descriptors) if (descriptor >= 0) close(descriptor); }
    };
    int restrictedQuery(const char *name, void *value, size_t *size, void *newValue, size_t newSize) {
        const int physical = sysctlbyname(name, value, size, newValue, newSize);
        if (physical != 0) return physical;
        if (restriction == Restriction::QueryFailure) { errno = ENOENT; return -1; }
        if (restriction == Restriction::ShortResult) { *size = 0; return 0; }
        if ((restriction == Restriction::NoAes && std::strcmp(name, "hw.optional.arm.FEAT_AES") == 0)
            || (restriction == Restriction::NoSimd && std::strcmp(name, "hw.optional.neon") == 0))
            if (*size == sizeof(int)) *static_cast<int *>(value) = 0;
        return physical; // Only remove capabilities; never turn physical failure into support.
    }
    void observedSslInit(mbedtls_ssl_context *context) { ++initializations; mbedtls_ssl_init(context); }
    void observedEntropyInit(mbedtls_entropy_context *context) { ++initializations; mbedtls_entropy_init(context); }
    void observedDrbgInit(mbedtls_ctr_drbg_context *context) { ++initializations; mbedtls_ctr_drbg_init(context); }
}

// Compile the production implementation into this test executable with narrow
// syscall/init observers. No test switch or interposer enters shipped targets.
#define sysctlbyname restrictedQuery
#define mbedtls_ssl_init observedSslInit
#define mbedtls_entropy_init observedEntropyInit
#define mbedtls_ctr_drbg_init observedDrbgInit
#include "source/network/SecureSession.cpp"
#undef sysctlbyname
#undef mbedtls_ssl_init
#undef mbedtls_entropy_init
#undef mbedtls_ctr_drbg_init

#define curl_global_init observedCurlInit
#include "source/client/HostDirectory.cpp"
#undef curl_global_init

D6R_TEST_CASE("Darwin physical admission rejects restricted AES SIMD failed and malformed sysctl before TLS and RNG") {
    using namespace Duel6::Network;
    D6R_REQUIRE(SecureSession::supported());
    // The identical instrumentation must observe initialization with a usable
    // native socket. An invalid FD would let SO_NOSIGPIPE hide a missing gate.
    {
        SocketPair sockets;
        initializations = 0;
        SecureSession session(sockets.descriptors[0], false, {});
        D6R_REQUIRE(!session.expired());
        D6R_REQUIRE_EQ(3u, initializations);
    }
    for (const auto denied : {Restriction::NoAes, Restriction::NoSimd,
                             Restriction::QueryFailure, Restriction::ShortResult}) {
        restriction = denied;
        initializations = 0;
        D6R_REQUIRE(!SecureSession::supported());
        SocketPair sockets;
        SecureSession session(sockets.descriptors[0], false, {});
        D6R_REQUIRE(session.expired());
        D6R_REQUIRE_EQ(0u, initializations);
    }
    restriction = Restriction::None;
    D6R_REQUIRE(SecureSession::supported());
    SocketPair sockets;
    initializations = 0;
    SecureSession session(sockets.descriptors[0], false, {});
    D6R_REQUIRE(!session.expired());
    D6R_REQUIRE_EQ(3u, initializations);
}

D6R_TEST_CASE("Darwin directory capability admission precedes actual curl initialization") {
    std::optional<std::string> previous;
    if (const auto *value = std::getenv("D6R_DIRECTORY_URL")) previous = value;
    D6R_REQUIRE(setenv("D6R_DIRECTORY_URL", "", 1) == 0); // No network request in this ordering test.
    struct Restore {
        std::optional<std::string> previous;
        ~Restore() {
            restriction = Restriction::None;
            if (previous) setenv("D6R_DIRECTORY_URL", previous->c_str(), 1);
            else unsetenv("D6R_DIRECTORY_URL");
        }
    } restore{previous};
    D6R_REQUIRE(Duel6::Network::SecureSession::supported());
    for (const auto denied : {Restriction::NoAes, Restriction::NoSimd,
                              Restriction::QueryFailure, Restriction::ShortResult}) {
        // Each child starts before the function-local curl initialization. A
        // previous positive call must not conceal a missing ordering gate.
        const auto child = fork();
        D6R_REQUIRE(child >= 0);
        if (!child) {
            restriction = denied; curlInitializations = 0; initializations = 0;
            (void) Duel6::Client::directoryRequest("GET", "/v1/sessions", "", "", 0, nullptr);
            _exit(curlInitializations == 0 && initializations == 0 ? 0 : 1);
        }
        int status = 0;
        D6R_REQUIRE(waitpid(child, &status, 0) == child);
        D6R_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    restriction = Restriction::None; curlInitializations = 0;
    (void) Duel6::Client::directoryRequest("GET", "/v1/sessions", "", "", 0, nullptr);
    D6R_REQUIRE_EQ(1u, curlInitializations);
    curl_global_cleanup();
}

D6R_TEST_CASE("Darwin missing hardware prevents host and CLI seed entropy before listener or world startup") {
    using namespace Duel6;
    struct Restore { ~Restore() { restriction = Restriction::None; d6rRandomFailureAt(0); } } restore;
    restriction = Restriction::NoAes;
    initializations = 0;
    d6rRandomFailureAt(0); // Observe real OS calls, without forcing a random failure.
    Server::ServerConfig config;
    config.transportEnabled = true; config.listenEndpoint = {"127.0.0.1", 26660};
    config.resourcePath = D6R_HARDWARE_RESOURCES;
    std::ostringstream output;
    bool validationBegan = false;
    Server::AdmissionRuntimeDependencies dependencies;
    dependencies.lifecycleObserver = [&](const auto &event) {
        if (event.stage == Server::AdmissionLifecycleStage::ManifestBuildStarted) validationBegan = true;
        return true;
    };
    Server::HeadlessServer server(config, dependencies);
    D6R_REQUIRE_EQ(2, server.run(output));
    D6R_REQUIRE(!validationBegan);
    D6R_REQUIRE_EQ(0u, d6rRandomCalls());
    D6R_REQUIRE_EQ(0u, initializations);
    char name[] = "unsupported-hardware", option[] = "--authoritative-match";
    std::string resource = std::string("--resources=") + D6R_HARDWARE_RESOURCES;
    char *arguments[] = {name, option, resource.data()};
    std::istringstream input;
    D6R_REQUIRE(Server::Authoritative::runAuthoritativeMatchCli(3, arguments, input, output) != 0);
    D6R_REQUIRE_EQ(0u, d6rRandomCalls());
    D6R_REQUIRE_EQ(0u, initializations);
}
