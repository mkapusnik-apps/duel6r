#include <Security/Security.h>
#include <curl/curl.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

namespace {
    std::atomic<unsigned> trustCalls{0};
    bool observedTrust(SecTrustRef trust, CFErrorRef *error) {
        ++trustCalls;
        // dyld interposition applies to other images, not calls from this
        // defining image. Always delegate to the real Apple evaluator.
        return SecTrustEvaluateWithError(trust, error);
    }
    __attribute__((used, section("__DATA,__interpose")))
    const struct { const void *replacement; const void *original; } trustObserver = {
        reinterpret_cast<const void *>(&observedTrust),
        reinterpret_cast<const void *>(&SecTrustEvaluateWithError)};
}

int main(int argc, char **argv) {
    if (argc != 1 && argc != 5) return 2;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 2;
    const auto *version = curl_version_info(CURLVERSION_NOW);
    if (!version || version->version_num < 0x075500 || !(version->features & CURL_VERSION_ASYNCHDNS)) return 2;
    const std::string versionText = version->version;
    CURL *curl = curl_easy_init();
    if (!curl) return 2;
    bool configured = true;
    const auto option = [&](CURLoption key, auto value) {
        if (curl_easy_setopt(curl, key, value) != CURLE_OK) configured = false;
    };
    const std::string mode = argc == 1 ? "system-trust" : argv[1];
    option(CURLOPT_URL, argc == 1 ? "https://curl.se/" : argv[2]);
    option(CURLOPT_PROTOCOLS_STR, "https");
    option(CURLOPT_FOLLOWLOCATION, 0L);
    option(CURLOPT_PROXY, "");
    option(CURLOPT_NOSIGNAL, 1L);
    option(CURLOPT_TIMEOUT_MS, 9000L);
    option(CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    option(CURLOPT_SSL_VERIFYPEER, 1L);
    option(CURLOPT_SSL_VERIFYHOST, 2L);
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
    const bool nativeRequired = mode == "system-trust" || mode == "fixture-untrusted";
    std::cout << "curl=" << versionText << ";mode=" << mode
              << ";native-evaluations=" << trustCalls << ";result=" << static_cast<int>(result) << '\n';
    return passed && (!nativeRequired || trustCalls > 0) ? 0 : 1;
}
