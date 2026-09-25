/* Private session transport configuration for Mbed TLS 3.6.7.
 * Based on upstream configs/config-thread.h (Apache-2.0 OR GPL-2.0-or-later).
 * Copyright The Mbed TLS Contributors.
 * Not the configuration for libcurl or directory HTTPS.
 */
#ifndef DUEL6R_SESSION_MBEDTLS_CONFIG_H
#define DUEL6R_SESSION_MBEDTLS_CONFIG_H

/* The application must reject CPUs without AES-NI before any TLS/RNG init.
 * Hardware-only mode removes public AES software fallback and bypasses the
 * library's CPU detection. GCC/MinGW x64 use assembly; MSVC uses intrinsics.
 */
#if !defined(__x86_64__) && !defined(_M_X64)
#error "The private session TLS profile requires x86_64 with runtime AES-NI admission."
#endif
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_AESNI_C
#define MBEDTLS_AES_USE_HARDWARE_ONLY
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECJPAKE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C

/* Upstream socket programs and error diagnostics; no test entropy sources. */
#define MBEDTLS_NET_C
#define MBEDTLS_TIMING_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_DEBUG_C

/* TLS 1.3, PSA crypto, DTLS, tickets and renegotiation are not enabled. */
#define MBEDTLS_MPI_MAX_SIZE 32

#endif
