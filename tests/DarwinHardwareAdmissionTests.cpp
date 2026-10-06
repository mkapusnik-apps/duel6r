#include "TestHarness.h"
#include <sys/sysctl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <cerrno>
#include <cstring>

namespace {
    enum class Restriction { None, NoAes, NoSimd, QueryFailure, ShortResult };
    Restriction restriction = Restriction::None;
    unsigned initializations = 0;
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

D6R_TEST_CASE("Darwin physical admission rejects restricted AES SIMD failed and malformed sysctl before TLS and RNG") {
    using namespace Duel6::Network;
    D6R_REQUIRE(SecureSession::supported());
    for (const auto denied : {Restriction::NoAes, Restriction::NoSimd,
                             Restriction::QueryFailure, Restriction::ShortResult}) {
        restriction = denied;
        initializations = 0;
        D6R_REQUIRE(!SecureSession::supported());
        SecureSession session(-1, false, {});
        D6R_REQUIRE(session.expired());
        D6R_REQUIRE_EQ(0u, initializations);
    }
    restriction = Restriction::None;
    D6R_REQUIRE(SecureSession::supported());
}
