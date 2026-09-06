#ifndef DUEL6_NETWORK_NETWORKTRUSTPOLICY_H
#define DUEL6_NETWORK_NETWORKTRUSTPOLICY_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Duel6::Network::Trust {
    constexpr std::size_t MaxAdmissionPayloadBytes = 262144;
    constexpr std::size_t MaxProperties = 4096;
    constexpr std::size_t MaxKeyBytes = 128;
    constexpr std::size_t MaxStringBytes = 4096;
    constexpr std::size_t MaxHostnameBytes = 253;
    constexpr std::size_t MaxParticipantNameBytes = 64;
    constexpr std::size_t MaxReasonBytes = 256;
    constexpr std::size_t MaxCollectionEntries = 256;
    constexpr std::size_t MaxManifestEntries = 256;
    constexpr std::size_t MaxLogicalPathBytes = 240;
    constexpr std::size_t MaxLogicalPathSegments = 16;
    constexpr std::size_t MaxParticipants = 15;
    constexpr std::size_t MaxResolverIpv4Addresses = 64;

    constexpr std::size_t MaxPendingAdmissions = 8;
    constexpr std::size_t MaxPendingAdmissionsPerSource = 4;
    constexpr std::size_t MaxAdmissionAttemptsPerSourcePerMinute = 20;
    constexpr std::size_t AdmissionAttemptBurst = 4;
    constexpr std::size_t MaxTrackedAdmissionSources = 256;
    constexpr std::size_t MaxConcurrentManifestValidations = 2;
    constexpr std::size_t MaxAggregateQueuedBytes = 32 * 1024 * 1024;

    constexpr std::size_t GuestToHostBytesPerSecond = 512 * 1024;
    constexpr std::size_t GuestToHostBurstBytes = 1024 * 1024;
    constexpr std::size_t HostToGuestBytesPerSecond = 4 * 1024 * 1024;
    constexpr std::size_t HostToGuestBurstBytes = 4 * 1024 * 1024;
    constexpr std::size_t NonInputActionsPerSecond = 30;
    constexpr std::size_t NonInputActionBurst = 60;
    constexpr std::size_t InputsPerOwnedSlotPerSecond = 120;
    constexpr std::size_t GlobalAcceptedInputsPerSecond = 1800;

    constexpr auto FirstAdmissionRequestDeadline = std::chrono::seconds(3);
    constexpr auto ReconnectCredentialLifetime = std::chrono::seconds(30);
    constexpr std::size_t ReconnectCredentialBytes = 16;
    constexpr std::size_t MaxReconnectCredentialGenerationAttempts = 4;

    inline constexpr std::string_view LoopbackExposureCopy =
            "Network session is limited to this machine. No authentication or encryption is used.";
    inline constexpr std::string_view PrivateLanExposureCopy =
            "Network session is limited to a private LAN. No authentication or encryption is used. Do not expose this port to the Internet.";
    inline constexpr std::string_view UnsupportedAddressCopy =
            "Network session cannot use a public or wildcard address. Use loopback or a private LAN address.";
    inline constexpr std::string_view ReconnectAuthorizationFailureCopy =
            "Reconnect authorization failed. This session cannot be restored.";

    enum class EndpointScope { Invalid, Loopback, PrivateLan, Unsupported };
    EndpointScope classifyIpv4(const std::array<std::uint8_t, 4> &address);
    EndpointScope classifyIpv4Literal(std::string_view value, std::array<std::uint8_t, 4> *address = nullptr);
    struct Ipv4InterfaceRecord {
        std::array<std::uint8_t, 4> address{};
        std::uint8_t prefixLength = 0;
        std::optional<std::array<std::uint8_t, 4>> broadcastAddress;
    };
    enum class LocalListenerBindDecision {
        Allowed,
        UnsupportedAddress,
        NotAssigned,
        InvalidPrefix,
        NetworkAddress,
        BroadcastAddress,
        InterfaceEnumerationFailed
    };
    LocalListenerBindDecision decideLocalListenerBind(
            const std::array<std::uint8_t, 4> &address, const std::vector<Ipv4InterfaceRecord> &interfaces);
    LocalListenerBindDecision localListenerBindDecision(const std::array<std::uint8_t, 4> &address);
    bool isLocalIpv4AddressAssigned(const std::array<std::uint8_t, 4> &address);
    bool validHostname(std::string_view value);
    bool validGuestEndpointName(std::string_view value);

    bool validPropertyCount(std::size_t count);
    bool validPropertyKey(std::string_view value);
    bool validGeneralString(std::string_view value);
    bool validAsciiReason(std::string_view value);
    bool validCollectionSize(std::size_t count);
    bool validManifestEntryCount(std::size_t count);
    bool validParticipantName(std::string_view value);
    bool validLogicalPath(std::string_view value);

    enum class AdmissionOutcome {
        Accepted,
        MalformedRequest,
        NotAuthorized,
        HostPolicyRejected,
        SessionPolicyViolation
    };
    std::string_view outcomeCode(AdmissionOutcome outcome);
    std::string_view outcomeUserCopy(AdmissionOutcome outcome);

    using TimePoint = std::chrono::steady_clock::time_point;
    using Clock = std::function<TimePoint()>;
    using AdmissionHook = std::function<AdmissionOutcome(const std::vector<std::uint8_t> &)>;

    class AdmissionGate {
    public:
        enum class State { AwaitingRequest, Accepted, Rejected, Expired };
        explicit AdmissionGate(AdmissionHook hook, Clock clock = {});
        AdmissionOutcome submit(const std::vector<std::uint8_t> &payload);
        bool expireIfDue();
        State state() const;
        AdmissionOutcome outcome() const;
    private:
        AdmissionHook hook;
        Clock clock;
        TimePoint deadline;
        State current = State::AwaitingRequest;
        AdmissionOutcome result = AdmissionOutcome::MalformedRequest;
    };

    class PendingAdmissionLimiter {
    public:
        class Reservation {
        public:
            ~Reservation();
            Reservation(const Reservation &) = delete;
            Reservation &operator=(const Reservation &) = delete;
            void release();
        private:
            Reservation(PendingAdmissionLimiter *owner, std::uint32_t source);
            PendingAdmissionLimiter *owner;
            std::uint32_t source;
            friend class PendingAdmissionLimiter;
        };

        explicit PendingAdmissionLimiter(Clock clock = {});
        std::shared_ptr<Reservation> reserve(std::uint32_t source);
    private:
        struct SourceState {
            std::size_t pending = 0;
            double burstTokens = AdmissionAttemptBurst;
            TimePoint lastRefill{};
            std::deque<TimePoint> attempts;
        };
        Clock clock;
        std::mutex mutex;
        std::size_t pending = 0;
        std::unordered_map<std::uint32_t, SourceState> sources;
        void release(std::uint32_t source);
    };

    class ConcurrentWorkLimiter {
    public:
        explicit ConcurrentWorkLimiter(std::size_t limit = MaxConcurrentManifestValidations);
        bool reserve();
        void release();
        std::size_t active() const;
    private:
        const std::size_t limit;
        mutable std::mutex mutex;
        std::size_t count = 0;
    };

    class AggregateQueueBudget {
    public:
        bool reserve(std::size_t bytes);
        void release(std::size_t bytes);
        std::size_t used() const;
    private:
        mutable std::mutex mutex;
        std::size_t bytes = 0;
    };
    AggregateQueueBudget &processQueueBudget();

    class TokenBucket {
    public:
        TokenBucket(double tokensPerSecond, double burst, Clock clock = {});
        bool consume(double tokens = 1.0);
    private:
        double rate;
        double capacity;
        double available;
        Clock clock;
        TimePoint updated;
        std::mutex mutex;
    };

    class PerSecondRateCounter {
    public:
        explicit PerSecondRateCounter(std::size_t limit, Clock clock = {});
        bool consume(std::size_t count = 1);
    private:
        const std::size_t limit;
        Clock clock;
        TimePoint window;
        std::size_t used = 0;
        std::mutex mutex;
    };

    class ConsecutiveWindowLimit {
    public:
        explicit ConsecutiveWindowLimit(Clock clock = {});
        bool recordOverLimit();
        void recordWithinLimit();
    private:
        Clock clock;
        std::int64_t windowIndex;
        bool over = false;
        std::mutex mutex;
    };

    class AppliedInputGate {
    public:
        bool reserve(std::uint32_t slot, std::uint64_t tick);
        void clearBefore(std::uint64_t tick);
    private:
        std::set<std::pair<std::uint64_t, std::uint32_t>> applied;
    };

    using ConnectionId = std::uint64_t;
    using ParticipantId = std::uint64_t;
    using PlayerSlotId = std::uint64_t;
    enum class AuthorityAction { HostOnly, OwnReadiness, OwnProposal, Leave, PlayerInput, ReplicatedStateMutation };
    struct AuthorizationDecision {
        bool allowed = false;
        AdmissionOutcome outcome = AdmissionOutcome::SessionPolicyViolation;
        bool closeConnection = true;
    };

    class AuthorizationPolicy {
    public:
        void createLocalHost(ConnectionId connection, ParticipantId participant);
        bool bindGuest(ConnectionId connection, ParticipantId participant);
        bool setOwnedSlots(ParticipantId participant, const std::vector<PlayerSlotId> &slots);
        bool authorize(ConnectionId connection, AuthorityAction action,
                       std::optional<PlayerSlotId> slot = std::nullopt) const;
        AuthorizationDecision decide(ConnectionId connection, AuthorityAction action,
                                     std::optional<PlayerSlotId> slot = std::nullopt) const;
        void disconnect(ConnectionId connection);
        void removeParticipant(ParticipantId participant);
    private:
        std::optional<ParticipantId> host;
        std::unordered_map<ConnectionId, ParticipantId> bindings;
        std::unordered_map<ParticipantId, std::set<PlayerSlotId>> ownership;
    };

    enum class DiagnosticStage { Listener, Transport, Admission, Authorization, RateLimit, Reconnect, Resolver };
    enum class DiagnosticCategory { Started, Accepted, Rejected, Expired, Backpressure, Closed, Failure };
    enum class DiagnosticLimit { None, Connections, PendingAdmissions, SourcePending, SourceAttempts, Payload,
        QueueBytes, Bandwidth, Actions, Inputs, ManifestWork, ResolverAddresses };
    struct DiagnosticEvent {
        std::uint64_t trustedTimestamp = 0;
        std::uint64_t localConnectionNumber = 0;
        DiagnosticStage stage = DiagnosticStage::Transport;
        DiagnosticCategory category = DiagnosticCategory::Failure;
        DiagnosticLimit limit = DiagnosticLimit::None;
        std::uint32_t current = 0;
        std::uint32_t maximum = 0;
    };
    std::string formatDiagnostic(const DiagnosticEvent &event);

    using RandomFill = std::function<bool(std::uint8_t *, std::size_t)>;
    struct ReconnectCredential {
        std::array<std::uint8_t, ReconnectCredentialBytes> bytes{};
        ReconnectCredential() = default;
        ~ReconnectCredential();
        ReconnectCredential(const ReconnectCredential &other);
        ReconnectCredential &operator=(const ReconnectCredential &other);
        ReconnectCredential(ReconnectCredential &&other) noexcept;
        ReconnectCredential &operator=(ReconnectCredential &&other) noexcept;
        void clear() noexcept;
    };
    struct ReconnectAuthorizationResult {
        bool accepted = false;
        bool ratePolicyFailure = true;
        std::string_view userCopy = ReconnectAuthorizationFailureCopy;
    };
    class ReconnectReservation {
    public:
        ReconnectReservation(std::uint64_t session, ParticipantId participant, std::uint64_t reservation,
                             Clock clock = {}, RandomFill random = {});
        ~ReconnectReservation();
        ReconnectReservation(const ReconnectReservation &) = delete;
        ReconnectReservation &operator=(const ReconnectReservation &) = delete;
        bool valid();
        ReconnectCredential credential();
        ReconnectAuthorizationResult authorizeAndConsume(const ReconnectCredential &candidate,
                                                          std::uint64_t session, ParticipantId participant,
                                                          std::uint64_t reservation);
        bool consume(const ReconnectCredential &candidate, std::uint64_t session, ParticipantId participant,
                     std::uint64_t reservation);
        bool expireIfDue();
        bool cancel(std::uint64_t session, ParticipantId participant, std::uint64_t reservation);
        bool participantRemoved(std::uint64_t session, ParticipantId participant);
        bool sessionEnded(std::uint64_t session);
        bool replace(std::uint64_t session, ParticipantId participant, std::uint64_t reservation,
                     std::uint64_t replacementReservation);
        void invalidate();
    private:
        Clock clock;
        RandomFill random;
        std::optional<ReconnectCredential> value;
        TimePoint expiry;
        std::uint64_t session;
        ParticipantId participant;
        std::uint64_t reservation;
        std::mutex mutex;
        bool generateLocked(const ReconnectCredential *disallowed = nullptr);
        bool expireIfDueLocked();
        void invalidateLocked();
    };

    bool guestContentMayLoadOrExecute();
}

#endif
