#include "source/network/SecureSession.h"
#include <mbedtls/aes.h>
#include <array>
#include <chrono>
#include <iostream>
#include <string>

#if defined(D6R_TEST_WRAP_AUXV)
#include <sys/auxv.h>
#include <asm/hwcap.h>
namespace { unsigned long permittedCapabilities = ~0UL; }
extern "C" unsigned long __real_getauxval(unsigned long type);
extern "C" unsigned long __wrap_getauxval(unsigned long type) {
    const auto physical = __real_getauxval(type);
    return type == AT_HWCAP ? physical & permittedCapabilities : physical;
}
#endif

namespace {
    bool rejectedBeforeHandshake() {
        using namespace Duel6::Network;
        if (SecureSession::supported()) return false;
        SecureSession session(-1, false, {});
        unsigned char byte = 0;
        return session.expired() && session.send(&byte, 1) == -1 && session.receive(&byte, 1) == -1
            && !session.handshake(std::chrono::steady_clock::now(), [] { return false; });
    }
}

// Run separately on a CPU/emulator without AES using --expect-unsupported.
// This never overrides a negative physical CPU check to force crypto execution.
int main(int argc, char **argv) {
    using namespace Duel6::Network;
    const bool unsupported = argc == 2 && std::string(argv[1]) == "--expect-unsupported";
    if (argc > 1 && !unsupported) return 2;
    if (SecureSession::supported(false)) return 1;
    if (unsupported) {
        if (!rejectedBeforeHandshake()) return 1;
        std::cout << "Unsupported CPU rejected before crypto initialization\n";
        return 0;
    }
#if defined(D6R_TEST_WRAP_AUXV)
    // The link wrapper only removes OS-advertised capabilities, never invents
    // support. Test AES and SIMD separately, plus a missing HWCAP entry.
    for (const auto mask: {~static_cast<unsigned long>(HWCAP_AES),
                           ~static_cast<unsigned long>(HWCAP_ASIMD), 0UL}) {
        permittedCapabilities = mask;
        if (!rejectedBeforeHandshake()) return 1;
    }
    permittedCapabilities = ~0UL;
    std::cout << "ARM admission rejects missing AES, ASIMD and HWCAP\n";
#endif
    if (!SecureSession::supported()) {
        std::cerr << "Hardware AES is required for the positive crypto tests\n";
        return 1;
    }
    // FIPS 197 AES known-answer vectors: CCM uses 128-bit AES and CTR-DRBG
    // uses 256-bit AES. Exercise key expansion and both block directions.
    const std::array<unsigned char, 16> plain = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const std::array<std::array<unsigned char, 16>, 2> expected = {{
        {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a},
        {0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89}}};
    std::array<unsigned char, 32> key{};
    for (unsigned i = 0; i < key.size(); ++i) key[i] = static_cast<unsigned char>(i);
    for (unsigned variant = 0; variant < 2; ++variant) {
        mbedtls_aes_context context;
        mbedtls_aes_init(&context);
        std::array<unsigned char, 16> encrypted{}, decrypted{};
        const unsigned bits = variant == 0 ? 128 : 256;
        const bool ok = mbedtls_aes_setkey_enc(&context, key.data(), bits) == 0
            && mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_ENCRYPT, plain.data(), encrypted.data()) == 0
            && encrypted == expected[variant]
            && mbedtls_aes_setkey_dec(&context, key.data(), bits) == 0
            && mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_DECRYPT, encrypted.data(), decrypted.data()) == 0
            && decrypted == plain;
        mbedtls_aes_free(&context);
        if (!ok) return 1;
    }
    std::cout << "Hardware AES-128/AES-256 known-answer tests passed\n";
    return 0;
}
