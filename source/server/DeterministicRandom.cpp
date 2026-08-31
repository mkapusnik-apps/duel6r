#include "DeterministicRandom.h"

#include <limits>
#include <stdexcept>

namespace Duel6::Server::Authoritative {
    DeterministicRandom::DeterministicRandom(std::uint64_t seed) : state(seed) {
        if (seed == 0) throw std::invalid_argument("Authoritative random seed must be nonzero");
    }

    std::uint64_t DeterministicRandom::next() {
        // SplitMix64 has a fully specified unsigned-integer transition on every supported platform.
        std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31u);
    }

    std::uint64_t DeterministicRandom::bounded(std::uint64_t exclusiveUpperBound) {
        if (exclusiveUpperBound == 0) throw std::invalid_argument("Random bound must be positive");
        const std::uint64_t rejectionThreshold = (0u - exclusiveUpperBound) % exclusiveUpperBound;
        for (;;) {
            const std::uint64_t value = next();
            if (value >= rejectionThreshold) return value % exclusiveUpperBound;
        }
    }
}
