#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <iterator>
#include <sstream>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <sys/random.h>
#endif

namespace Duel6::Network::Trust {
    namespace {
        TimePoint realNow() { return std::chrono::steady_clock::now(); }
        Clock selectedClock(Clock clock) { return clock ? std::move(clock) : Clock(realNow); }

        bool parseOctet(std::string_view value, std::uint8_t &result) {
            if (value.empty() || value.size() > 3 || (value.size() > 1 && value.front() == '0')) return false;
            unsigned parsed = 0;
            for (char character: value) {
                if (character < '0' || character > '9') return false;
                parsed = parsed * 10u + static_cast<unsigned>(character - '0');
            }
            if (parsed > 255) return false;
            result = static_cast<std::uint8_t>(parsed);
            return true;
        }

        bool validUtf8(std::string_view value) {
            for (std::size_t index = 0; index < value.size();) {
                const auto lead = static_cast<unsigned char>(value[index]);
                std::uint32_t point = 0;
                std::size_t continuation = 0;
                if (lead < 0x80) point = lead;
                else if ((lead & 0xe0) == 0xc0) { point = lead & 0x1f; continuation = 1; }
                else if ((lead & 0xf0) == 0xe0) { point = lead & 0x0f; continuation = 2; }
                else if ((lead & 0xf8) == 0xf0) { point = lead & 0x07; continuation = 3; }
                else return false;
                if (index + continuation >= value.size()) return false;
                for (std::size_t offset = 1; offset <= continuation; ++offset) {
                    const auto next = static_cast<unsigned char>(value[index + offset]);
                    if ((next & 0xc0) != 0x80) return false;
                    point = (point << 6u) | (next & 0x3f);
                }
                if ((continuation == 1 && point < 0x80) || (continuation == 2 && point < 0x800)
                    || (continuation == 3 && point < 0x10000) || point > 0x10ffff
                    || (point >= 0xd800 && point <= 0xdfff)) return false;
                if (point < 0x20 || (point >= 0x7f && point <= 0x9f)
                    || point == 0x2028 || point == 0x2029
                    || (point >= 0x202a && point <= 0x202e) || (point >= 0x2066 && point <= 0x2069)
                    || point == 0x061c || point == 0x200e || point == 0x200f) return false;
                index += continuation + 1;
            }
            return true;
        }

        bool secureRandom(std::uint8_t *target, std::size_t size) {
#ifdef D6R_TRANSPORT_WINDOWS
            return BCryptGenRandom(nullptr, target, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
            std::size_t offset = 0;
            while (offset < size) {
                const ssize_t count = getrandom(target + offset, size - offset, 0);
                if (count > 0) offset += static_cast<std::size_t>(count);
                else if (count < 0 && errno == EINTR) continue;
                else return false;
            }
            return true;
#endif
        }

        bool constantTimeEqual(const ReconnectCredential &left, const ReconnectCredential &right) {
            std::uint8_t difference = 0;
            for (std::size_t index = 0; index < left.bytes.size(); ++index)
                difference |= left.bytes[index] ^ right.bytes[index];
            return difference == 0;
        }
    }

    EndpointScope classifyIpv4(const std::array<std::uint8_t, 4> &address) {
        if (address[3] == 255) return EndpointScope::Unsupported;
        if (address[0] == 127) return EndpointScope::Loopback;
        if (address[0] == 10 || (address[0] == 172 && address[1] >= 16 && address[1] <= 31)
            || (address[0] == 192 && address[1] == 168)) return EndpointScope::PrivateLan;
        return EndpointScope::Unsupported;
    }

    EndpointScope classifyIpv4Literal(std::string_view value, std::array<std::uint8_t, 4> *result) {
        std::array<std::uint8_t, 4> address{};
        std::size_t start = 0;
        for (std::size_t index = 0; index < address.size(); ++index) {
            const std::size_t separator = value.find('.', start);
            if ((index < 3 && separator == std::string_view::npos)
                || (index == 3 && separator != std::string_view::npos)) return EndpointScope::Invalid;
            const std::size_t end = separator == std::string_view::npos ? value.size() : separator;
            if (!parseOctet(value.substr(start, end - start), address[index])) return EndpointScope::Invalid;
            start = end + 1;
        }
        if (result) *result = address;
        return classifyIpv4(address);
    }

    bool validHostname(std::string_view value) {
        if (value.empty() || value.size() > MaxHostnameBytes || value.front() == '.' || value.back() == '.') return false;
        std::size_t labelLength = 0;
        bool labelStartsWithHyphen = false;
        char previous = 0;
        for (unsigned char character: value) {
            if (character > 0x7f) return false;
            if (character == '.') {
                if (labelLength == 0 || labelLength > 63 || labelStartsWithHyphen || previous == '-') return false;
                labelLength = 0;
                labelStartsWithHyphen = false;
            } else {
                const bool valid = (character >= 'a' && character <= 'z')
                                   || (character >= 'A' && character <= 'Z')
                                   || (character >= '0' && character <= '9') || character == '-';
                if (!valid) return false;
                if (labelLength == 0) labelStartsWithHyphen = character == '-';
                ++labelLength;
            }
            previous = static_cast<char>(character);
        }
        return labelLength > 0 && labelLength <= 63 && !labelStartsWithHyphen && previous != '-';
    }

    bool validGuestEndpointName(std::string_view value) {
        return classifyIpv4Literal(value) != EndpointScope::Invalid || validHostname(value);
    }

    bool validAsciiReason(std::string_view value) {
        if (value.size() > MaxReasonBytes) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return character >= 0x20 && character <= 0x7e;
        });
    }

    bool validParticipantName(std::string_view value) {
        return !value.empty() && value.size() <= MaxParticipantNameBytes && validUtf8(value);
    }

    bool validLogicalPath(std::string_view value) {
        if (value.empty() || value.size() > MaxLogicalPathBytes || value.front() == '/' || value.back() == '/') return false;
        std::size_t segments = 0;
        std::size_t start = 0;
        while (start < value.size()) {
            const std::size_t separator = value.find('/', start);
            const std::size_t end = separator == std::string_view::npos ? value.size() : separator;
            const auto segment = value.substr(start, end - start);
            if (segment.empty() || segment.size() > 64 || ++segments > MaxLogicalPathSegments) return false;
            for (std::size_t index = 0; index < segment.size(); ++index) {
                const unsigned char character = segment[index];
                const bool alphaNumeric = (character >= 'a' && character <= 'z')
                                          || (character >= 'A' && character <= 'Z')
                                          || (character >= '0' && character <= '9');
                if (!alphaNumeric && (index == 0 || (character != '.' && character != '_' && character != '-')))
                    return false;
            }
            if (separator == std::string_view::npos) break;
            start = separator + 1;
        }
        return true;
    }

    std::string_view outcomeCode(AdmissionOutcome outcome) {
        switch (outcome) {
            case AdmissionOutcome::MalformedRequest: return "malformed-request";
            case AdmissionOutcome::NotAuthorized: return "not-authorized";
            case AdmissionOutcome::HostPolicyRejected: return "host-policy-rejected";
            case AdmissionOutcome::SessionPolicyViolation: return "session-policy-violation";
            case AdmissionOutcome::Accepted: return "accepted";
        }
        return "malformed-request";
    }

    std::string_view outcomeUserCopy(AdmissionOutcome outcome) {
        switch (outcome) {
            case AdmissionOutcome::MalformedRequest: return "Connection request rejected.";
            case AdmissionOutcome::NotAuthorized: return "Connection not authorized.";
            case AdmissionOutcome::HostPolicyRejected: return "Host rejected the connection.";
            case AdmissionOutcome::SessionPolicyViolation: return "Connection ended.";
            case AdmissionOutcome::Accepted: return {};
        }
        return "Connection request rejected.";
    }

    AdmissionGate::AdmissionGate(AdmissionHook hook, Clock clock)
            : hook(std::move(hook)), clock(selectedClock(std::move(clock))),
              deadline(this->clock() + FirstAdmissionRequestDeadline) {}

    AdmissionOutcome AdmissionGate::submit(const std::vector<std::uint8_t> &payload) {
        if (current != State::AwaitingRequest) {
            current = State::Rejected;
            return result = AdmissionOutcome::SessionPolicyViolation;
        }
        if (clock() >= deadline) {
            current = State::Expired;
            return result = AdmissionOutcome::MalformedRequest;
        }
        if (payload.empty() || payload.size() > MaxAdmissionPayloadBytes || !hook) {
            current = State::Rejected;
            return result = AdmissionOutcome::MalformedRequest;
        }
        result = hook(payload);
        current = result == AdmissionOutcome::Accepted ? State::Accepted : State::Rejected;
        return result;
    }

    bool AdmissionGate::expireIfDue() {
        if (current != State::AwaitingRequest || clock() < deadline) return false;
        current = State::Expired;
        result = AdmissionOutcome::MalformedRequest;
        return true;
    }
    AdmissionGate::State AdmissionGate::state() const { return current; }
    AdmissionOutcome AdmissionGate::outcome() const { return result; }

    PendingAdmissionLimiter::Reservation::Reservation(PendingAdmissionLimiter *owner, std::uint32_t source)
            : owner(owner), source(source) {}
    PendingAdmissionLimiter::Reservation::~Reservation() { release(); }
    void PendingAdmissionLimiter::Reservation::release() {
        if (!owner) return;
        owner->release(source);
        owner = nullptr;
    }

    PendingAdmissionLimiter::PendingAdmissionLimiter(Clock clock) : clock(selectedClock(std::move(clock))) {}
    std::shared_ptr<PendingAdmissionLimiter::Reservation> PendingAdmissionLimiter::reserve(std::uint32_t source) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        for (auto iterator = sources.begin(); iterator != sources.end();) {
            const bool attemptWindowExpired = iterator->second.attempts.empty()
                                              ? current - iterator->second.lastRefill >= std::chrono::seconds(60)
                                              : current - iterator->second.attempts.back() >= std::chrono::seconds(60);
            const bool stale = iterator->second.pending == 0 && attemptWindowExpired;
            iterator = stale ? sources.erase(iterator) : std::next(iterator);
        }
        if (!sources.count(source) && sources.size() >= MaxTrackedAdmissionSources) return {};
        auto &state = sources[source];
        if (state.lastRefill == TimePoint{}) state.lastRefill = current;
        const double elapsed = std::max(0.0, std::chrono::duration<double>(current - state.lastRefill).count());
        state.burstTokens = std::min<double>(AdmissionAttemptBurst,
                state.burstTokens + elapsed * MaxAdmissionAttemptsPerSourcePerMinute / 60.0);
        state.lastRefill = current;
        while (!state.attempts.empty() && current - state.attempts.front() >= std::chrono::seconds(60))
            state.attempts.pop_front();
        if (pending >= MaxPendingAdmissions || state.pending >= MaxPendingAdmissionsPerSource
            || state.attempts.size() >= MaxAdmissionAttemptsPerSourcePerMinute || state.burstTokens < 1.0) return {};
        --state.burstTokens;
        state.attempts.push_back(current);
        ++state.pending;
        ++pending;
        return std::shared_ptr<Reservation>(new Reservation(this, source));
    }
    void PendingAdmissionLimiter::release(std::uint32_t source) {
        std::lock_guard<std::mutex> lock(mutex);
        auto iterator = sources.find(source);
        if (iterator == sources.end() || iterator->second.pending == 0 || pending == 0) return;
        --iterator->second.pending;
        --pending;
    }

    ConcurrentWorkLimiter::ConcurrentWorkLimiter(std::size_t limit) : limit(limit) {}
    bool ConcurrentWorkLimiter::reserve() {
        std::lock_guard<std::mutex> lock(mutex);
        if (count >= limit) return false;
        ++count;
        return true;
    }
    void ConcurrentWorkLimiter::release() { std::lock_guard<std::mutex> lock(mutex); if (count) --count; }
    std::size_t ConcurrentWorkLimiter::active() const { std::lock_guard<std::mutex> lock(mutex); return count; }

    bool AggregateQueueBudget::reserve(std::size_t amount) {
        std::lock_guard<std::mutex> lock(mutex);
        if (amount > MaxAggregateQueuedBytes - bytes) return false;
        bytes += amount;
        return true;
    }
    void AggregateQueueBudget::release(std::size_t amount) {
        std::lock_guard<std::mutex> lock(mutex);
        bytes = amount > bytes ? 0 : bytes - amount;
    }
    std::size_t AggregateQueueBudget::used() const { std::lock_guard<std::mutex> lock(mutex); return bytes; }
    AggregateQueueBudget &processQueueBudget() { static AggregateQueueBudget budget; return budget; }

    TokenBucket::TokenBucket(double tokensPerSecond, double burst, Clock clock)
            : rate(tokensPerSecond), capacity(burst), available(burst), clock(selectedClock(std::move(clock))),
              updated(this->clock()) {}
    bool TokenBucket::consume(double tokens) {
        if (tokens < 0) return false;
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        const double elapsed = std::max(0.0, std::chrono::duration<double>(current - updated).count());
        available = std::min(capacity, available + elapsed * rate);
        updated = current;
        if (tokens > available) return false;
        available -= tokens;
        return true;
    }

    PerSecondRateCounter::PerSecondRateCounter(std::size_t limit, Clock clock)
            : limit(limit), clock(selectedClock(std::move(clock))), window(this->clock()) {}
    bool PerSecondRateCounter::consume(std::size_t count) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        if (current - window >= std::chrono::seconds(1)) {
            window = current;
            used = 0;
        }
        if (count > limit - std::min(used, limit)) return false;
        used += count;
        return true;
    }

    ConsecutiveWindowLimit::ConsecutiveWindowLimit(Clock clock)
            : clock(selectedClock(std::move(clock))), window(this->clock()) {}
    bool ConsecutiveWindowLimit::recordOverLimit() {
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        if (current - window >= std::chrono::seconds(1)) {
            consecutive = over ? consecutive + 1 : 0;
            window = current;
            over = false;
        }
        over = true;
        return consecutive >= 1;
    }
    void ConsecutiveWindowLimit::recordWithinLimit() {
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        if (current - window >= std::chrono::seconds(1)) { consecutive = 0; over = false; window = current; }
    }

    bool AppliedInputGate::reserve(std::uint32_t slot, std::uint64_t tick) { return applied.emplace(tick, slot).second; }
    void AppliedInputGate::clearBefore(std::uint64_t tick) {
        applied.erase(applied.begin(), applied.lower_bound({tick, 0}));
    }

    void AuthorizationPolicy::createLocalHost(ConnectionId connection, ParticipantId participant) {
        if (host || bindings.count(connection) || participant == 0) return;
        host = participant;
        bindings.emplace(connection, participant);
    }
    bool AuthorizationPolicy::bindGuest(ConnectionId connection, ParticipantId participant) {
        if (participant == 0 || bindings.count(connection)
            || std::any_of(bindings.begin(), bindings.end(), [participant](const auto &entry) {
                return entry.second == participant;
            })) return false;
        bindings.emplace(connection, participant);
        return true;
    }
    bool AuthorizationPolicy::setOwnedSlots(ParticipantId participant, const std::vector<PlayerSlotId> &slots) {
        if (slots.size() > MaxParticipants) return false;
        std::set<PlayerSlotId> unique(slots.begin(), slots.end());
        if (unique.size() != slots.size()) return false;
        for (const auto &entry: ownership) {
            if (entry.first == participant) continue;
            for (PlayerSlotId slot: unique) if (entry.second.count(slot)) return false;
        }
        ownership[participant] = std::move(unique);
        return true;
    }
    bool AuthorizationPolicy::authorize(ConnectionId connection, AuthorityAction action,
                                         std::optional<PlayerSlotId> slot) const {
        const auto binding = bindings.find(connection);
        if (binding == bindings.end()) return false;
        if (action == AuthorityAction::HostOnly) return host && binding->second == *host;
        if (action == AuthorityAction::PlayerInput) {
            const auto owned = ownership.find(binding->second);
            return slot && owned != ownership.end() && owned->second.count(*slot) != 0;
        }
        return action == AuthorityAction::OwnReadiness || action == AuthorityAction::OwnProposal
               || action == AuthorityAction::Leave;
    }
    AuthorizationDecision AuthorizationPolicy::decide(ConnectionId connection, AuthorityAction action,
                                                       std::optional<PlayerSlotId> slot) const {
        const bool allowed = authorize(connection, action, slot);
        return {allowed, allowed ? AdmissionOutcome::Accepted : AdmissionOutcome::SessionPolicyViolation,
                !allowed};
    }
    void AuthorizationPolicy::disconnect(ConnectionId connection) {
        const auto binding = bindings.find(connection);
        if (binding == bindings.end()) return;
        if (host && *host == binding->second) host.reset();
        bindings.erase(binding);
    }
    void AuthorizationPolicy::removeParticipant(ParticipantId participant) {
        ownership.erase(participant);
        for (auto iterator = bindings.begin(); iterator != bindings.end();) {
            iterator = iterator->second == participant ? bindings.erase(iterator) : std::next(iterator);
        }
        if (host && *host == participant) host.reset();
    }

    std::string formatDiagnostic(const DiagnosticEvent &event) {
        const auto stage = [](DiagnosticStage value) {
            switch (value) {
                case DiagnosticStage::Listener: return "listener";
                case DiagnosticStage::Transport: return "transport";
                case DiagnosticStage::Admission: return "admission";
                case DiagnosticStage::Authorization: return "authorization";
                case DiagnosticStage::RateLimit: return "rate-limit";
                case DiagnosticStage::Reconnect: return "reconnect";
                case DiagnosticStage::Resolver: return "resolver";
            }
            return "transport";
        };
        const auto category = [](DiagnosticCategory value) {
            switch (value) {
                case DiagnosticCategory::Started: return "started";
                case DiagnosticCategory::Accepted: return "accepted";
                case DiagnosticCategory::Rejected: return "rejected";
                case DiagnosticCategory::Expired: return "expired";
                case DiagnosticCategory::Backpressure: return "backpressure";
                case DiagnosticCategory::Closed: return "closed";
                case DiagnosticCategory::Failure: return "failure";
            }
            return "failure";
        };
        const auto limit = [](DiagnosticLimit value) {
            switch (value) {
                case DiagnosticLimit::None: return "none";
                case DiagnosticLimit::Connections: return "connections";
                case DiagnosticLimit::PendingAdmissions: return "pending-admissions";
                case DiagnosticLimit::SourcePending: return "source-pending";
                case DiagnosticLimit::SourceAttempts: return "source-attempts";
                case DiagnosticLimit::Payload: return "payload";
                case DiagnosticLimit::QueueBytes: return "queue-bytes";
                case DiagnosticLimit::Bandwidth: return "bandwidth";
                case DiagnosticLimit::Actions: return "actions";
                case DiagnosticLimit::Inputs: return "inputs";
                case DiagnosticLimit::ManifestWork: return "manifest-work";
                case DiagnosticLimit::ResolverAddresses: return "resolver-addresses";
            }
            return "none";
        };
        std::ostringstream output;
        output << "timestamp=" << event.trustedTimestamp << " connection=" << event.localConnectionNumber
               << " stage=" << stage(event.stage)
               << " category=" << category(event.category)
               << " limit=" << limit(event.limit)
               << " current=" << event.current << " maximum=" << event.maximum;
        return output.str();
    }

    ReconnectReservation::ReconnectReservation(std::uint64_t session, ParticipantId participant,
                                               std::uint64_t reservation, Clock clock, RandomFill random)
            : clock(selectedClock(std::move(clock))), expiry(this->clock() + ReconnectCredentialLifetime),
              session(session), participant(participant), reservation(reservation), active(false) {
        RandomFill fill = random ? std::move(random) : RandomFill(secureRandom);
        active = session != 0 && participant != 0 && reservation != 0 && fill(value.bytes.data(), value.bytes.size());
    }
    ReconnectReservation::~ReconnectReservation() { invalidate(); }
    bool ReconnectReservation::valid() const { return active && clock() < expiry; }
    const ReconnectCredential &ReconnectReservation::credential() const { return value; }
    bool ReconnectReservation::consume(const ReconnectCredential &candidate, std::uint64_t expectedSession,
                                       ParticipantId expectedParticipant, std::uint64_t expectedReservation) {
        const bool credentialMatches = constantTimeEqual(value, candidate);
        const bool accepted = valid() && session == expectedSession && participant == expectedParticipant
                              && reservation == expectedReservation && credentialMatches;
        if (accepted) invalidate();
        return accepted;
    }
    void ReconnectReservation::invalidate() { active = false; std::fill(value.bytes.begin(), value.bytes.end(), 0); }

    bool guestContentMayLoadOrExecute() { return false; }
}
