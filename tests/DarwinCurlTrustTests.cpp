#include <Security/Security.h>
#include <curl/curl.h>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstring>
#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>

extern "C" void d6rResetTrustObservation();
extern "C" unsigned d6rModernTrustEvaluations();
extern "C" unsigned d6rLegacyTrustEvaluations();

int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--observer-control") {
        std::ifstream input(argv[2], std::ios::binary);
        std::vector<unsigned char> der{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (der.empty()) return 2;
        CFDataRef data = CFDataCreate(nullptr, der.data(), static_cast<CFIndex>(der.size()));
        SecCertificateRef certificate = data ? SecCertificateCreateWithData(nullptr, data) : nullptr;
        SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("localhost"));
        SecTrustRef trust = nullptr;
        const OSStatus created = certificate && policy
            ? SecTrustCreateWithCertificates(certificate, policy, &trust) : errSecParam;
        d6rResetTrustObservation();
        CFErrorRef error = nullptr;
        const bool accepted = created == errSecSuccess && SecTrustEvaluateWithError(trust, &error);
        const auto calls = d6rModernTrustEvaluations();
        if (error) CFRelease(error);
        if (trust) CFRelease(trust);
        if (policy) CFRelease(policy);
        if (certificate) CFRelease(certificate);
        if (data) CFRelease(data);
        std::cout << "observer-control;created=" << created << ";accepted=" << accepted << ";calls=" << calls << '\n';
        return created == errSecSuccess && !accepted && calls > 0 ? 0 : 1;
    }
    if (argc != 1 && argc != 5) return 2;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 2;
    const auto *version = curl_version_info(CURLVERSION_NOW);
    if (!version || version->version_num < 0x075500 || !(version->features & CURL_VERSION_ASYNCHDNS)) return 2;
    const std::string versionText = version->version;
    const std::string backend = version->ssl_version ? version->ssl_version : "none";
    Dl_info image{};
    dladdr(reinterpret_cast<const void *>(&curl_easy_init), &image);
    const std::string library = image.dli_fname ? image.dli_fname : "unknown";
    if (const char *expected = std::getenv("D6R_EXPECT_CURL_LIBRARY")) {
        std::error_code error;
        if (!std::filesystem::equivalent(library, expected, error) || error) {
            std::cerr << "Packaged trust probe loaded a different curl image: " << library << '\n';
            return 2;
        }
    }
    d6rResetTrustObservation();
    CURL *curl = curl_easy_init();
    if (!curl) return 2;
    bool configured = true;
    const auto option = [&](CURLoption key, auto value) {
        if (curl_easy_setopt(curl, key, value) != CURLE_OK) configured = false;
    };
    const std::string mode = argc == 1 ? "system-trust" : argv[1];
    const bool nativeRequired = mode == "system-trust" || mode == "fixture-untrusted";
    option(CURLOPT_URL, argc == 1 ? "https://curl.se/" : argv[2]);
    option(CURLOPT_PROTOCOLS_STR, "https");
    option(CURLOPT_FOLLOWLOCATION, 0L);
    option(CURLOPT_PROXY, "");
    option(CURLOPT_NOSIGNAL, 1L);
    option(CURLOPT_TIMEOUT_MS, 9000L);
    option(CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    option(CURLOPT_SSL_VERIFYPEER, 1L);
    option(CURLOPT_SSL_VERIFYHOST, 2L);
    if (nativeRequired) {
        option(CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
        option(CURLOPT_CAINFO, static_cast<const char *>(nullptr));
        option(CURLOPT_CAPATH, static_cast<const char *>(nullptr));
    }
    option(CURLOPT_NOBODY, 1L); // Approved bounded read-only request; no cookies/credentials/body.
    curl_slist *resolve = nullptr;
    if (argc == 5) {
        resolve = curl_slist_append(nullptr, argv[3]);
        if (!resolve) configured = false;
        option(CURLOPT_RESOLVE, resolve);
        if (std::strcmp(argv[4], "-") != 0) option(CURLOPT_CAINFO, argv[4]);
    }
    const CURLcode result = configured ? curl_easy_perform(curl) : CURLE_FAILED_INIT;
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(resolve);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    const bool expectSuccess = mode == "system-trust" || mode == "fixture-trusted";
    const bool passed = expectSuccess ? result == CURLE_OK && status == 200
                                     : result == CURLE_PEER_FAILED_VERIFICATION;
    // A version/backend string or ignored option cannot satisfy native trust.
    // Observe the actual evaluator invocation on both system-trust paths.
    const auto calls = d6rModernTrustEvaluations() + d6rLegacyTrustEvaluations();
    std::cout << "curl=" << versionText << ";backend=" << backend << ";library=" << library << ";mode=" << mode
              << ";modern-evaluations=" << d6rModernTrustEvaluations() << ";legacy-evaluations=" << d6rLegacyTrustEvaluations()
              << ";configured=" << configured << ";result=" << static_cast<int>(result) << '\n';
    return passed && (!nativeRequired || calls > 0) ? 0 : 1;
}
