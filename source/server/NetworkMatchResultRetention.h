#ifndef DUEL6_SERVER_NETWORKMATCHRESULTRETENTION_H
#define DUEL6_SERVER_NETWORKMATCHRESULTRETENTION_H

#include <cstdint>
#include <optional>
#include <vector>

#include "AuthoritativeMatchTypes.h"

namespace Duel6::Server::Authoritative {
    enum class ResultRetentionStatus {
        Retained,
        Duplicate,
        Rejected
    };

    class NetworkMatchResultRetention final {
    public:
        std::optional<std::uint64_t> beginMatch() noexcept;
        ResultRetentionStatus retain(std::uint64_t generation, const SessionResult &result);
        bool markParticipantsDeparted(const std::vector<Identity> &participantIds);
        void discard() noexcept;

        const std::optional<SessionResult> &current() const noexcept;
        static constexpr bool persistenceEligible() noexcept { return false; }

    private:
        std::uint64_t nextGeneration = 1;
        std::uint64_t activeGeneration = 0;
        std::optional<SessionResult> retained;
        std::optional<std::string> serialized;
    };
}

#endif
