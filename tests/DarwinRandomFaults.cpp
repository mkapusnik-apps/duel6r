#include <atomic>
#include <cerrno>
#include <unistd.h>
#include <sys/random.h>

namespace {
    std::atomic<unsigned> calls{0}, failAt{0};
    int observedEntropy(void *data, size_t size) {
        const auto call = ++calls;
        if (failAt && call >= failAt) { errno = EIO; return -1; }
        return getentropy(data, size); // Positive control always uses the real OS.
    }
    __attribute__((used, section("__DATA,__interpose")))
    static struct { const void *replacement; const void *original; } observer = {
        reinterpret_cast<const void *>(&observedEntropy), reinterpret_cast<const void *>(&getentropy)};
}
extern "C" void d6rRandomFailureAt(unsigned call) { calls = 0; failAt = call; }
extern "C" unsigned d6rRandomCalls() { return calls.load(); }
