#include "DeterministicRandom.h"

#include <limits>
#include <stdexcept>
#include <cctype>

namespace Duel6::Server::Authoritative {
    DeterministicRandom::DeterministicRandom(std::uint64_t seed) : state(seed) {
        if (seed == 0) throw std::invalid_argument("Authoritative random seed must be nonzero");
    }

    std::uint64_t DeterministicRandom::draw() {
        // SplitMix64 has a fully specified unsigned-integer transition on every supported platform.
        std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31u);
    }

    void DeterministicRandom::record(std::string_view purpose, std::uint64_t bound, std::uint64_t value) {
        if (purpose.empty() || purpose.size() > 64) throw std::invalid_argument("Random purpose is invalid");
        for (const unsigned char character: purpose)
            if (character < 0x21 || character > 0x7e) throw std::invalid_argument("Random purpose is invalid");
        ++decisions;
        const auto mix = [this](std::uint64_t item) {
            for (unsigned shift = 0; shift < 64; shift += 8) {
                digest ^= (item >> shift) & UINT64_C(0xff);
                digest *= UINT64_C(1099511628211);
            }
        };
        mix(decisions); mix(bound); mix(value);
        for (const unsigned char character: purpose) {
            digest ^= character;
            digest *= UINT64_C(1099511628211);
        }
        if (trace.size() < 256) trace.push_back({decisions, bound, value, std::string(purpose)});
    }

    std::uint64_t DeterministicRandom::next(std::string_view purpose) {
        const std::uint64_t value = draw();
        record(purpose, 0, value);
        return value;
    }

    std::uint64_t DeterministicRandom::bounded(std::uint64_t exclusiveUpperBound, std::string_view purpose) {
        if (exclusiveUpperBound == 0) throw std::invalid_argument("Random bound must be positive");
        const std::uint64_t rejectionThreshold = (0u - exclusiveUpperBound) % exclusiveUpperBound;
        for (;;) {
            const std::uint64_t value = draw();
            if (value >= rejectionThreshold) {
                const std::uint64_t selected = value % exclusiveUpperBound;
                record(purpose, exclusiveUpperBound, selected);
                return selected;
            }
        }
    }

    std::uint64_t DeterministicRandom::decisionCount() const noexcept { return decisions; }
    std::uint64_t DeterministicRandom::decisionDigest() const noexcept { return digest == 0 ? 1 : digest; }
    const std::vector<DeterministicRandom::Decision> &DeterministicRandom::decisionTrace() const noexcept {
        return trace;
    }
}
