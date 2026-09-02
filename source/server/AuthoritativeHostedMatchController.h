#ifndef DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H
#define DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H

#include <map>
#include <memory>

#include "AuthoritativeMatch.h"

namespace Duel6::Server::Authoritative {
    enum class HostedMatchStage {
        ServiceStarting,
        Lobby,
        MatchActive,
        ContentBlocked,
        UnexpectedStop,
        Ended
    };

    class AuthoritativeHostedMatchController final {
    public:
        AuthoritativeHostedMatchController(Identity hostParticipantId,
                                           MatchRuntimeDependencies dependencies = {});

        bool markServiceReady();
        bool setParticipantReady(Identity participantId, bool ready);
        TerminalOutcome start(const MatchConfig &config, const std::vector<PlayerDefinition> &roster,
                              const Network::GameplayManifest &manifest);
        TerminalOutcome end(Identity participantId);
        void observeMatchOutcome();

        HostedMatchStage stage() const noexcept;
        bool contentStartBlocked() const noexcept;
        bool participantReady(Identity participantId) const noexcept;
        AuthoritativeMatch *match() noexcept;
        const AuthoritativeMatch *match() const noexcept;

    private:
        MatchRuntimeDependencies dependencies;
        HostedMatchStage currentStage = HostedMatchStage::ServiceStarting;
        std::map<Identity, bool> readiness;
        const Identity hostParticipantId;
        std::unique_ptr<AuthoritativeMatch> activeMatch;

        void clearReadiness() noexcept;
        bool allParticipantsReady(const std::vector<PlayerDefinition> &roster) const noexcept;
    };
}

#endif
