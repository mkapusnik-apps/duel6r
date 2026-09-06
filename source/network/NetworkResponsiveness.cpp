#include "NetworkResponsiveness.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace Duel6::Network::Responsiveness {
    namespace {
        constexpr const char *DegradedText = "Network connection degraded.";
        constexpr const char *RetainedStateText = "Last confirmed state";
        constexpr const char *SynchronizationText = "Synchronizing current state\xE2\x80\xA6";
        constexpr const char *ReconnectingText = "Reconnecting\xE2\x80\xA6";

        bool subtractMilliseconds(TimePoint from, std::chrono::milliseconds amount,
                                  TimePoint &result) noexcept {
            if (amount < std::chrono::milliseconds::zero()
                || amount > std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::duration::max())) return false;
            const auto delta = std::chrono::duration_cast<Clock::duration>(amount).count();
            const auto fromCount = from.time_since_epoch().count();
            const auto minimum = Clock::duration::min().count();
            if (fromCount < minimum + delta) return false;
            result = TimePoint{Clock::duration{fromCount - delta}};
            return true;
        }

        bool addMilliseconds(TimePoint from, std::chrono::milliseconds amount,
                             TimePoint &result) noexcept {
            if (amount < std::chrono::milliseconds::zero()
                || amount > std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::duration::max())) return false;
            const auto delta = std::chrono::duration_cast<Clock::duration>(amount).count();
            const auto fromCount = from.time_since_epoch().count();
            const auto maximum = Clock::duration::max().count();
            if (fromCount > maximum - delta) return false;
            result = TimePoint{Clock::duration{fromCount + delta}};
            return true;
        }

        bool deadlineReached(TimePoint startedAt, std::chrono::milliseconds delay,
                             TimePoint now) noexcept {
            TimePoint deadline;
            return addMilliseconds(startedAt, delay, deadline) && now >= deadline;
        }

        std::optional<std::chrono::milliseconds> elapsedMilliseconds(
                TimePoint startedAt, TimePoint now) noexcept {
            if (now < startedAt) return std::nullopt;
            using Rep = Clock::duration::rep;
            using Unsigned = std::make_unsigned_t<Rep>;
            static_assert(std::numeric_limits<Rep>::is_signed,
                          "The responsiveness clock must use a signed duration");
            const Rep started = startedAt.time_since_epoch().count();
            const Rep finished = now.time_since_epoch().count();
            Unsigned ticks;
            if (started < 0 && finished >= 0) {
                const Unsigned beforeEpoch = static_cast<Unsigned>(-(started + 1)) + 1;
                const Unsigned afterEpoch = static_cast<Unsigned>(finished);
                const Unsigned maximum = static_cast<Unsigned>(
                        std::numeric_limits<Rep>::max());
                if (beforeEpoch > maximum - afterEpoch) return std::nullopt;
                ticks = beforeEpoch + afterEpoch;
            } else {
                ticks = static_cast<Unsigned>(finished - started);
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::duration{static_cast<Rep>(ticks)});
            return elapsed;
        }

        std::int64_t interpolateValue(std::int64_t from, std::int64_t to, long double progress) noexcept {
            const long double value = static_cast<long double>(from)
                                      + (static_cast<long double>(to)
                                         - static_cast<long double>(from)) * progress;
            if (value >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
                return std::numeric_limits<std::int64_t>::max();
            if (value <= static_cast<long double>(std::numeric_limits<std::int64_t>::min()))
                return std::numeric_limits<std::int64_t>::min();
            return static_cast<std::int64_t>(std::llround(value));
        }

        PresentedPlayerPose pose(const Replication::PlayerState &player) noexcept {
            return {player.playerId, player.positionX, player.positionY,
                    player.facingLeft, player.crouching};
        }
    }

    ConditionBudget budget(Environment environment) noexcept {
        if (environment == Environment::SameMachine)
            return {std::chrono::milliseconds(20), std::chrono::milliseconds(5), 0.0,
                    std::chrono::milliseconds(100)};
        return {std::chrono::milliseconds(100), std::chrono::milliseconds(30), 1.0,
                std::chrono::milliseconds(150)};
    }

    ConnectionQualityMonitor::ConnectionQualityMonitor(Environment environment)
            : limits(budget(environment)) {}

    bool ConnectionQualityMonitor::observeNetworkSample(
            const NetworkSample &sample, TimePoint observedAt) noexcept {
        if (sample.roundTripLatency < std::chrono::milliseconds::zero()
            || sample.sentPackets == 0 || sample.lostPackets > sample.sentPackets
            || (latestNetworkSampleAt && observedAt < *latestNetworkSampleAt)) return false;
        if (latestNetworkSample)
            latestJitter = sample.roundTripLatency > latestNetworkSample->roundTripLatency
                           ? sample.roundTripLatency - latestNetworkSample->roundTripLatency
                           : latestNetworkSample->roundTripLatency - sample.roundTripLatency;
        else latestJitter = std::chrono::milliseconds::zero();
        latestNetworkSample = sample;
        latestNetworkSampleAt = observedAt;
        if (networkInsideBudget()) networkExceededSince.reset();
        else if (!networkExceededSince) networkExceededSince = observedAt;
        return true;
    }

    bool ConnectionQualityMonitor::observeCanonicalState(
            Replication::StateVersion version, TimePoint acceptedAt) noexcept {
        return observeCanonicalState(version, std::chrono::milliseconds::zero(), acceptedAt);
    }

    bool ConnectionQualityMonitor::observeCanonicalVersion(
            Replication::StateVersion version, TimePoint acceptedAt) noexcept {
        if (version == 0 || version < latestVersion
            || (version == latestVersion && !resynchronizing)
            || (latestCanonicalAcceptanceAt && acceptedAt < *latestCanonicalAcceptanceAt)) return false;
        latestVersion = version;
        latestCanonicalAcceptanceAt = acceptedAt;
        resynchronizing = false;
        reconnecting = false;
        return true;
    }

    bool ConnectionQualityMonitor::observeCanonicalState(
            Replication::StateVersion version, std::chrono::milliseconds stateAgeAtAcceptance,
            TimePoint acceptedAt) noexcept {
        TimePoint producedAt;
        if (!subtractMilliseconds(acceptedAt, stateAgeAtAcceptance, producedAt)) return false;
        if (version == 0 || version < latestVersion
            || (version == latestVersion && !resynchronizing && latestCanonicalStateAt)
            || stateAgeAtAcceptance < std::chrono::milliseconds::zero()
            || (latestCanonicalAcceptanceAt && acceptedAt < *latestCanonicalAcceptanceAt)
            || (latestCanonicalStateAt && producedAt < *latestCanonicalStateAt)) return false;
        latestVersion = version;
        latestCanonicalStateAt = producedAt;
        latestCanonicalAcceptanceAt = acceptedAt;
        if (stateAgeAtAcceptance <= MaximumCurrentStateAge) stateAgeExceededSince.reset();
        else if (!stateAgeExceededSince) {
            TimePoint thresholdCrossedAt;
            if (!subtractMilliseconds(acceptedAt,
                    stateAgeAtAcceptance - MaximumCurrentStateAge,
                    thresholdCrossedAt)) return false;
            stateAgeExceededSince = thresholdCrossedAt;
        }
        resynchronizing = false;
        reconnecting = false;
        return true;
    }

    void ConnectionQualityMonitor::beginResynchronization() noexcept {
        resynchronizing = true;
        supportedSince.reset();
    }

    void ConnectionQualityMonitor::transportClosed() noexcept {
        reconnecting = true;
        resynchronizing = true;
        degraded = false;
        supportedSince.reset();
    }

    bool ConnectionQualityMonitor::networkInsideBudget() const noexcept {
        if (!latestNetworkSample || !latestJitter) return false;
        const double loss = 100.0 * static_cast<double>(latestNetworkSample->lostPackets)
                            / static_cast<double>(latestNetworkSample->sentPackets);
        return latestNetworkSample->roundTripLatency <= limits.roundTripLatency
               && *latestJitter <= limits.jitter && loss <= limits.packetLossPercent;
    }

    ConnectionPresentationState ConnectionQualityMonitor::update(TimePoint now) noexcept {
        bool stateInsideBudget = false;
        if (latestCanonicalStateAt && now >= *latestCanonicalStateAt) {
            TimePoint threshold;
            stateInsideBudget = !addMilliseconds(
                    *latestCanonicalStateAt, MaximumCurrentStateAge, threshold)
                                || now <= threshold;
            if (!stateInsideBudget && !stateAgeExceededSince) stateAgeExceededSince = threshold;
            else if (stateInsideBudget) stateAgeExceededSince.reset();
        }

        const bool staleLongEnough = stateAgeExceededSince
                                     && deadlineReached(*stateAgeExceededSince,
                                             StateAgeDegradedDelay, now);
        const bool networkExceededLongEnough = networkExceededSince
                                                && deadlineReached(*networkExceededSince,
                                                        NetworkBudgetDegradedDelay, now);
        if (!reconnecting && (staleLongEnough || networkExceededLongEnough)) degraded = true;

        const bool fullySupported = !reconnecting && !resynchronizing
                                    && stateInsideBudget && networkInsideBudget();
        if (degraded && fullySupported) {
            if (!supportedSince) supportedSince = now;
            else if (deadlineReached(*supportedSince, SupportedRecoveryDelay, now)) degraded = false;
        } else supportedSince.reset();

        ConnectionPresentationState result;
        result.canonicalStateCurrent = latestVersion != 0 && !resynchronizing && !reconnecting;
        result.degraded = degraded;
        result.resynchronizing = resynchronizing;
        result.reconnecting = reconnecting;
        result.retainingLastConfirmedState = resynchronizing && latestVersion != 0;
        if (degraded) result.degradedText = DegradedText;
        if (result.retainingLastConfirmedState) {
            result.retainedStateText = RetainedStateText;
            if (reconnecting) result.reconnectingText = ReconnectingText;
            else result.synchronizationText = SynchronizationText;
        }
        return result;
    }

    std::optional<std::chrono::milliseconds> ConnectionQualityMonitor::currentStateAge(
            TimePoint now) const noexcept {
        if (!latestCanonicalStateAt || now < *latestCanonicalStateAt) return std::nullopt;
        return elapsedMilliseconds(*latestCanonicalStateAt, now);
    }

    std::optional<std::chrono::milliseconds> ConnectionQualityMonitor::currentJitter() const noexcept {
        return latestJitter;
    }

    std::optional<std::chrono::milliseconds> ConnectionQualityMonitor::currentRoundTripLatency() const noexcept {
        if (!latestNetworkSample) return std::nullopt;
        return latestNetworkSample->roundTripLatency;
    }

    std::optional<double> ConnectionQualityMonitor::currentPacketLossPercent() const noexcept {
        if (!latestNetworkSample) return std::nullopt;
        return 100.0 * static_cast<double>(latestNetworkSample->lostPackets)
               / static_cast<double>(latestNetworkSample->sentPackets);
    }

    Replication::StateVersion ConnectionQualityMonitor::acceptedVersion() const noexcept {
        return latestVersion;
    }

    bool CanonicalUpdatePacer::shouldPublish(
            std::uint64_t authoritativeTick, bool lifecycleTransition) noexcept {
        if (!lastPublishedTick || lifecycleTransition
            || authoritativeTick < *lastPublishedTick
            || authoritativeTick - *lastPublishedTick >= TicksPerCanonicalUpdate) {
            lastPublishedTick = authoritativeTick;
            return true;
        }
        return false;
    }

    void CanonicalUpdatePacer::reset(std::uint64_t authoritativeTick) noexcept {
        lastPublishedTick = authoritativeTick;
    }

    void CanonicalMovementPresentation::setLocallyControlledPlayers(
            std::set<Replication::Identity> playerIds) {
        localPlayers = std::move(playerIds);
    }

    bool CanonicalMovementPresentation::accept(
            Replication::StateVersion version, const Replication::CanonicalState &state,
            TimePoint acceptedAt) {
        if (version == 0 || version < acceptedVersion
            || (version == acceptedVersion && !waitingForFullState)
            || !Replication::validateCanonicalState(state))
            return false;
        const Replication::Identity roundId = state.round ? state.round->roundId : 0;
        if (acceptedVersion && state.sessionId != acceptedSessionId) return false;
        if (acceptedVersion && state.matchId != 0 && state.matchId == acceptedMatchId
            && roundId == acceptedRoundId && state.phaseTime < acceptedPhaseTime) return false;

        const bool sameRound = acceptedVersion && state.matchId == acceptedMatchId
                               && roundId != 0 && roundId == acceptedRoundId;
        std::map<Replication::Identity, Motion> next;
        std::set<Replication::Identity> nextPredictableLocalPlayers;
        for (const auto &player: state.players) {
            const PresentedPlayerPose target = pose(player);
            PresentedPlayerPose from = target;
            const auto prior = motion.find(player.playerId);
            if (sameRound && prior != motion.end()) from = interpolate(prior->second, acceptedAt);
            const bool local = localPlayers.count(player.playerId) != 0
                               && player.lifeState == Replication::LifeState::Alive;
            if (local) nextPredictableLocalPlayers.insert(player.playerId);
            next.emplace(player.playerId, Motion{from, target, acceptedAt,
                    sameRound ? (local ? MaximumCorrectionTime : CanonicalUpdateInterval)
                              : std::chrono::milliseconds::zero()});
        }
        motion = std::move(next);
        predictableLocalPlayers = std::move(nextPredictableLocalPlayers);
        acceptedVersion = version;
        acceptedSessionId = state.sessionId;
        acceptedMatchId = state.matchId;
        acceptedRoundId = roundId;
        acceptedPhaseTime = state.phaseTime;
        waitingForFullState = false;
        return true;
    }

    bool CanonicalMovementPresentation::predictLocalMovement(
            const PresentedPlayerPose &predicted, TimePoint sampledAt) noexcept {
        if (waitingForFullState || predicted.playerId == 0
            || !predictableLocalPlayers.count(predicted.playerId)) return false;
        const auto found = motion.find(predicted.playerId);
        if (found == motion.end()) return false;
        found->second = {predicted, predicted, sampledAt, std::chrono::milliseconds::zero()};
        return true;
    }

    PresentedPlayerPose CanonicalMovementPresentation::interpolate(
            const Motion &value, TimePoint now) noexcept {
        if (value.duration <= std::chrono::milliseconds::zero() || now <= value.startedAt)
            return now <= value.startedAt ? value.from : value.to;
        const auto elapsed = now - value.startedAt;
        if (elapsed >= value.duration) return value.to;
        const long double progress = std::chrono::duration<long double>(elapsed).count()
                                     / std::chrono::duration<long double>(value.duration).count();
        PresentedPlayerPose result = value.to;
        result.positionX = interpolateValue(value.from.positionX, value.to.positionX, progress);
        result.positionY = interpolateValue(value.from.positionY, value.to.positionY, progress);
        return result;
    }

    std::vector<PresentedPlayerPose> CanonicalMovementPresentation::sample(TimePoint now) noexcept {
        std::vector<PresentedPlayerPose> result;
        result.reserve(motion.size());
        for (auto &entry: motion) {
            PresentedPlayerPose presented = interpolate(entry.second, now);
            if (now >= entry.second.startedAt + entry.second.duration)
                entry.second = {entry.second.to, entry.second.to, now, std::chrono::milliseconds::zero()};
            result.push_back(presented);
        }
        return result;
    }

    void CanonicalMovementPresentation::beginResynchronization() noexcept {
        waitingForFullState = true;
    }

    bool CanonicalMovementPresentation::resynchronizing() const noexcept { return waitingForFullState; }
    Replication::StateVersion CanonicalMovementPresentation::version() const noexcept { return acceptedVersion; }
}
