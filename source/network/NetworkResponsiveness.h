#ifndef DUEL6_NETWORK_NETWORKRESPONSIVENESS_H
#define DUEL6_NETWORK_NETWORKRESPONSIVENESS_H

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "StateReplication.h"

namespace Duel6::Network::Responsiveness {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    constexpr std::uint32_t CanonicalUpdatesPerSecond = 20;
    constexpr std::uint32_t AuthoritativeTicksPerSecond = 60;
    constexpr std::uint32_t TicksPerCanonicalUpdate =
            AuthoritativeTicksPerSecond / CanonicalUpdatesPerSecond;
    constexpr auto CanonicalUpdateInterval = std::chrono::milliseconds(50);
    constexpr auto MaximumCurrentStateAge = std::chrono::milliseconds(250);
    constexpr auto StateAgeDegradedDelay = std::chrono::seconds(1);
    constexpr auto NetworkBudgetDegradedDelay = std::chrono::seconds(3);
    constexpr auto SupportedRecoveryDelay = std::chrono::seconds(3);
    constexpr auto MaximumRecoveryTime = std::chrono::seconds(5);
    constexpr auto MaximumCorrectionTime = std::chrono::milliseconds(150);

    enum class Environment { SameMachine, PrivateLan };

    struct ConditionBudget {
        std::chrono::milliseconds roundTripLatency;
        std::chrono::milliseconds jitter;
        double packetLossPercent = 0.0;
        std::chrono::milliseconds localResponse;
    };

    ConditionBudget budget(Environment environment) noexcept;

    struct NetworkSample {
        std::chrono::milliseconds roundTripLatency{0};
        std::uint64_t sentPackets = 0;
        std::uint64_t lostPackets = 0;
    };

    struct ConnectionPresentationState {
        bool degraded = false;
        bool resynchronizing = false;
        bool reconnecting = false;
        bool retainingLastConfirmedState = false;
        std::string degradedText;
        std::string retainedStateText;
        std::string synchronizationText;
    };

    // Tracks only an admitted network participant. Local Play never constructs this type.
    class ConnectionQualityMonitor final {
    public:
        explicit ConnectionQualityMonitor(Environment environment);

        bool observeNetworkSample(const NetworkSample &sample, TimePoint observedAt) noexcept;
        bool observeCanonicalState(Replication::StateVersion version, TimePoint acceptedAt) noexcept;
        bool observeCanonicalState(Replication::StateVersion version,
                                   std::chrono::milliseconds stateAgeAtAcceptance,
                                   TimePoint acceptedAt) noexcept;
        void beginResynchronization() noexcept;
        void transportClosed() noexcept;
        ConnectionPresentationState update(TimePoint now) noexcept;

        std::optional<std::chrono::milliseconds> currentStateAge(TimePoint now) const noexcept;
        std::optional<std::chrono::milliseconds> currentRoundTripLatency() const noexcept;
        std::optional<std::chrono::milliseconds> currentJitter() const noexcept;
        std::optional<double> currentPacketLossPercent() const noexcept;
        Replication::StateVersion acceptedVersion() const noexcept;

    private:
        ConditionBudget limits;
        std::optional<NetworkSample> latestNetworkSample;
        std::optional<std::chrono::milliseconds> latestJitter;
        std::optional<TimePoint> latestNetworkSampleAt;
        std::optional<TimePoint> latestCanonicalStateAt;
        std::optional<TimePoint> latestCanonicalAcceptanceAt;
        std::optional<TimePoint> networkExceededSince;
        std::optional<TimePoint> stateAgeExceededSince;
        std::optional<TimePoint> supportedSince;
        Replication::StateVersion latestVersion = 0;
        bool degraded = false;
        bool resynchronizing = false;
        bool reconnecting = false;

        bool networkInsideBudget() const noexcept;
    };

    class CanonicalUpdatePacer final {
    public:
        bool shouldPublish(std::uint64_t authoritativeTick, bool lifecycleTransition) noexcept;
        void reset(std::uint64_t authoritativeTick = 0) noexcept;
    private:
        std::optional<std::uint64_t> lastPublishedTick;
    };

    struct PresentedPlayerPose {
        Replication::Identity playerId = 0;
        std::int64_t positionX = 0;
        std::int64_t positionY = 0;
        bool facingLeft = false;
        bool crouching = false;
    };

    // Produces presentation-only movement. It never mutates or synthesizes canonical outcomes.
    class CanonicalMovementPresentation final {
    public:
        void setLocallyControlledPlayers(std::set<Replication::Identity> playerIds);
        bool accept(Replication::StateVersion version, const Replication::CanonicalState &state,
                    TimePoint acceptedAt);
        bool predictLocalMovement(const PresentedPlayerPose &pose, TimePoint sampledAt) noexcept;
        std::vector<PresentedPlayerPose> sample(TimePoint now) noexcept;
        void beginResynchronization() noexcept;
        bool resynchronizing() const noexcept;
        Replication::StateVersion version() const noexcept;

    private:
        struct Motion {
            PresentedPlayerPose from;
            PresentedPlayerPose to;
            TimePoint startedAt{};
            std::chrono::milliseconds duration{0};
        };

        std::set<Replication::Identity> localPlayers;
        std::set<Replication::Identity> predictableLocalPlayers;
        std::map<Replication::Identity, Motion> motion;
        Replication::StateVersion acceptedVersion = 0;
        Replication::Identity acceptedSessionId = 0;
        Replication::Identity acceptedMatchId = 0;
        Replication::Identity acceptedRoundId = 0;
        std::uint64_t acceptedPhaseTime = 0;
        bool waitingForFullState = false;

        static PresentedPlayerPose interpolate(const Motion &motion, TimePoint now) noexcept;
    };
}

#endif
