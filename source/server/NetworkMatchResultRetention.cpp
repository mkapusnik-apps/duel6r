#include "NetworkMatchResultRetention.h"

#include <limits>

#include "AuthoritativeMatchSerialization.h"

namespace Duel6::Server::Authoritative {
    std::optional<std::uint64_t> NetworkMatchResultRetention::beginMatch() noexcept {
        if (nextGeneration == 0) return std::nullopt;
        activeGeneration = nextGeneration;
        if (nextGeneration == std::numeric_limits<std::uint64_t>::max()) nextGeneration = 0;
        else ++nextGeneration;
        retained.reset();
        serialized.reset();
        return activeGeneration;
    }

    ResultRetentionStatus NetworkMatchResultRetention::retain(
            std::uint64_t generation, const SessionResult &result) {
        if (generation == 0 || generation != activeGeneration || result.label != "Session only")
            return ResultRetentionStatus::Rejected;
        const auto candidate = serializeSessionResult(result);
        if (!candidate) return ResultRetentionStatus::Rejected;
        if (retained) return serialized && *serialized == *candidate
                             ? ResultRetentionStatus::Duplicate
                             : ResultRetentionStatus::Rejected;
        retained = result;
        serialized = std::move(candidate);
        return ResultRetentionStatus::Retained;
    }

    void NetworkMatchResultRetention::discard() noexcept {
        activeGeneration = 0;
        retained.reset();
        serialized.reset();
    }

    const std::optional<SessionResult> &NetworkMatchResultRetention::current() const noexcept {
        return retained;
    }
}
