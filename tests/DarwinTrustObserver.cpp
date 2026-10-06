#include <Security/Security.h>
#include <atomic>

namespace {
    std::atomic<unsigned> modern{0}, legacy{0};
    bool evaluateModern(SecTrustRef trust, CFErrorRef *error) {
        ++modern;
        return SecTrustEvaluateWithError(trust, error);
    }
    OSStatus evaluateLegacy(SecTrustRef trust, SecTrustResultType *result) {
        ++legacy;
        return SecTrustEvaluate(trust, result);
    }
    // A separate linked image makes the observer independently exercisable
    // from the test executable. Calls in this image delegate to the real API.
    __attribute__((used, section("__DATA,__interpose")))
    static struct { const void *replacement; const void *original; } observers[] = {
        {reinterpret_cast<const void *>(&evaluateModern), reinterpret_cast<const void *>(&SecTrustEvaluateWithError)},
        {reinterpret_cast<const void *>(&evaluateLegacy), reinterpret_cast<const void *>(&SecTrustEvaluate)}
    };
}
extern "C" void d6rResetTrustObservation() { modern = 0; legacy = 0; }
extern "C" unsigned d6rModernTrustEvaluations() { return modern.load(); }
extern "C" unsigned d6rLegacyTrustEvaluations() { return legacy.load(); }
