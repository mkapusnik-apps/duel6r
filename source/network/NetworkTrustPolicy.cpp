#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#else
#include <cerrno>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/random.h>
#if defined(__GLIBC__) || defined(__linux__)
#include <strings.h>
#endif
#endif

namespace Duel6::Network::Trust {
    namespace {
        TimePoint realNow() { return std::chrono::steady_clock::now(); }
        Clock selectedClock(Clock clock) { return clock ? std::move(clock) : Clock(realNow); }

        void secureErase(void *target, std::size_t size) noexcept {
#ifdef D6R_TRANSPORT_WINDOWS
            SecureZeroMemory(target, size);
#elif defined(__GLIBC__) || defined(__linux__)
            explicit_bzero(target, size);
#else
            auto *bytes = static_cast<volatile unsigned char *>(target);
            while (size-- != 0) *bytes++ = 0;
#endif
        }

        std::int64_t secondWindowIndex(TimePoint value) {
            using Seconds = std::chrono::seconds;
            const auto elapsed = value.time_since_epoch();
            const auto truncated = std::chrono::duration_cast<Seconds>(elapsed);
            std::int64_t index = truncated.count();
            if (elapsed < truncated && index != (std::numeric_limits<std::int64_t>::min)()) --index;
            return index;
        }

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

        std::uint32_t ipv4Value(const std::array<std::uint8_t, 4> &address) {
            return static_cast<std::uint32_t>(address[0]) << 24u
                   | static_cast<std::uint32_t>(address[1]) << 16u
                   | static_cast<std::uint32_t>(address[2]) << 8u
                   | static_cast<std::uint32_t>(address[3]);
        }

#ifndef D6R_TRANSPORT_WINDOWS
        bool prefixLengthFromMask(const std::array<std::uint8_t, 4> &mask, std::uint8_t &prefixLength) {
            const std::uint32_t value = ipv4Value(mask);
            bool zeroSeen = false;
            std::uint8_t result = 0;
            for (int bit = 31; bit >= 0; --bit) {
                const bool set = (value & (std::uint32_t{1} << static_cast<unsigned>(bit))) != 0;
                if (!set) zeroSeen = true;
                else if (zeroSeen) return false;
                else ++result;
            }
            prefixLength = result;
            return true;
        }
#endif

        std::array<std::uint8_t, 4> socketIpv4(const sockaddr_in &address) {
            std::array<std::uint8_t, 4> result{};
            std::memcpy(result.data(), &address.sin_addr.s_addr, result.size());
            return result;
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

        bool allZero(const ReconnectCredential &credential) {
            std::uint8_t combined = 0;
            for (std::uint8_t byte: credential.bytes) combined |= byte;
            return combined == 0;
        }
    }

    EndpointScope classifyIpv4(const std::array<std::uint8_t, 4> &address) {
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

    LocalListenerBindDecision decideLocalListenerBind(
            const std::array<std::uint8_t, 4> &address, const std::vector<Ipv4InterfaceRecord> &interfaces) {
        const auto scope = classifyIpv4(address);
        if (scope != EndpointScope::Loopback && scope != EndpointScope::PrivateLan)
            return LocalListenerBindDecision::UnsupportedAddress;

        bool assigned = false;
        bool invalidPrefix = false;
        bool networkAddress = false;
        bool broadcastAddress = false;
        bool safeRecord = false;
        const std::uint32_t requested = ipv4Value(address);
        for (const auto &record: interfaces) {
            if (record.address != address) continue;
            assigned = true;
            if (record.prefixLength > 32) {
                invalidPrefix = true;
                continue;
            }
            if (record.broadcastAddress && *record.broadcastAddress == address) {
                broadcastAddress = true;
                continue;
            }
            if (record.prefixLength >= 31) {
                safeRecord = true;
                continue;
            }
            const std::uint32_t mask = record.prefixLength == 0
                                       ? 0 : ~std::uint32_t{0} << (32u - record.prefixLength);
            const std::uint32_t network = requested & mask;
            const std::uint32_t broadcast = network | ~mask;
            if (requested == network) {
                networkAddress = true;
                continue;
            }
            if (requested == broadcast) {
                broadcastAddress = true;
                continue;
            }
            safeRecord = true;
        }
        if (!assigned) return LocalListenerBindDecision::NotAssigned;
        if (broadcastAddress) return LocalListenerBindDecision::BroadcastAddress;
        if (networkAddress) return LocalListenerBindDecision::NetworkAddress;
        if (invalidPrefix) return LocalListenerBindDecision::InvalidPrefix;
        if (safeRecord) return LocalListenerBindDecision::Allowed;
        return LocalListenerBindDecision::NotAssigned;
    }

    LocalListenerBindDecision localListenerBindDecision(const std::array<std::uint8_t, 4> &address) {
        const auto scope = classifyIpv4(address);
        if (scope != EndpointScope::Loopback && scope != EndpointScope::PrivateLan)
            return LocalListenerBindDecision::UnsupportedAddress;
        std::vector<Ipv4InterfaceRecord> interfaces;
#ifdef D6R_TRANSPORT_WINDOWS
        ULONG size = 0;
        constexpr ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW
            || size == 0 || size > 1024 * 1024) return LocalListenerBindDecision::InterfaceEnumerationFailed;
        try {
            std::vector<std::max_align_t> storage(
                    (size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
            auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(storage.data());
            if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR)
                return LocalListenerBindDecision::InterfaceEnumerationFailed;
            for (auto *adapter = adapters; adapter; adapter = adapter->Next) {
                for (auto *entry = adapter->FirstUnicastAddress; entry; entry = entry->Next) {
                    if (!entry->Address.lpSockaddr || entry->Address.lpSockaddr->sa_family != AF_INET) continue;
                    const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(entry->Address.lpSockaddr);
                    interfaces.push_back({socketIpv4(*ipv4), entry->OnLinkPrefixLength, std::nullopt});
                }
            }
        } catch (const std::bad_alloc &) {
            return LocalListenerBindDecision::InterfaceEnumerationFailed;
        }
#else
        ifaddrs *interfaceList = nullptr;
        if (getifaddrs(&interfaceList) != 0) return LocalListenerBindDecision::InterfaceEnumerationFailed;
        try {
            for (auto *entry = interfaceList; entry; entry = entry->ifa_next) {
                if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET) continue;
                const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
                Ipv4InterfaceRecord record;
                record.address = socketIpv4(*ipv4);
                if (!entry->ifa_netmask || entry->ifa_netmask->sa_family != AF_INET) {
                    record.prefixLength = 33;
                } else {
                    const auto *mask = reinterpret_cast<const sockaddr_in *>(entry->ifa_netmask);
                    if (!prefixLengthFromMask(socketIpv4(*mask), record.prefixLength))
                        record.prefixLength = 33;
                }
                if ((entry->ifa_flags & IFF_BROADCAST) != 0 && entry->ifa_broadaddr
                    && entry->ifa_broadaddr->sa_family == AF_INET) {
                    const auto *broadcast = reinterpret_cast<const sockaddr_in *>(entry->ifa_broadaddr);
                    record.broadcastAddress = socketIpv4(*broadcast);
                }
                interfaces.push_back(record);
            }
        } catch (const std::bad_alloc &) {
            freeifaddrs(interfaceList);
            return LocalListenerBindDecision::InterfaceEnumerationFailed;
        }
        freeifaddrs(interfaceList);
#endif
        return decideLocalListenerBind(address, interfaces);
    }

    bool isLocalIpv4AddressAssigned(const std::array<std::uint8_t, 4> &address) {
        return localListenerBindDecision(address) == LocalListenerBindDecision::Allowed;
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

    bool validPropertyCount(std::size_t count) { return count <= MaxProperties; }

    bool validPropertyKey(std::string_view value) {
        if (value.empty() || value.size() > MaxKeyBytes) return false;
        for (std::size_t index = 0; index < value.size(); ++index) {
            const unsigned char character = value[index];
            const bool alphaNumeric = (character >= 'a' && character <= 'z')
                                      || (character >= 'A' && character <= 'Z')
                                      || (character >= '0' && character <= '9');
            if (!alphaNumeric && (index == 0 || (character != '.' && character != '_' && character != '-')))
                return false;
        }
        return true;
    }

    bool validGeneralString(std::string_view value) { return value.size() <= MaxStringBytes; }

    bool validAsciiReason(std::string_view value) {
        if (value.size() > MaxReasonBytes) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return character >= 0x20 && character <= 0x7e;
        });
    }

    bool validCollectionSize(std::size_t count) { return count <= MaxCollectionEntries; }
    bool validManifestEntryCount(std::size_t count) { return count <= MaxManifestEntries; }

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
        const double elapsed = (std::max)(0.0, std::chrono::duration<double>(current - state.lastRefill).count());
        state.burstTokens = (std::min<double>)(AdmissionAttemptBurst,
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
              updated(this->clock()) {
        if (!std::isfinite(rate) || !std::isfinite(capacity) || rate < 0 || capacity < 0)
            throw std::invalid_argument("Token bucket values must be finite and non-negative");
    }
    bool TokenBucket::consume(double tokens) {
        if (!std::isfinite(tokens) || tokens < 0) return false;
        std::lock_guard<std::mutex> lock(mutex);
        const auto current = clock();
        if (current < updated) {
            updated = current;
            return false;
        }
        const double elapsed = std::chrono::duration<double>(current - updated).count();
        const double missing = capacity - available;
        if (rate > 0 && missing > 0) {
            available = elapsed >= missing / rate ? capacity : available + elapsed * rate;
        }
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
            : clock(selectedClock(std::move(clock))), windowIndex(secondWindowIndex(this->clock())) {}
    bool ConsecutiveWindowLimit::recordOverLimit() {
        std::lock_guard<std::mutex> lock(mutex);
        const std::int64_t currentIndex = secondWindowIndex(clock());
        bool previousAdjacentWindowWasOver = false;
        if (currentIndex != windowIndex) {
            previousAdjacentWindowWasOver = currentIndex > windowIndex
                                             && windowIndex != (std::numeric_limits<std::int64_t>::max)()
                                             && currentIndex == windowIndex + 1 && over;
            windowIndex = currentIndex;
            over = false;
        }
        over = true;
        return previousAdjacentWindowWasOver;
    }
    void ConsecutiveWindowLimit::recordWithinLimit() {
        std::lock_guard<std::mutex> lock(mutex);
        const std::int64_t currentIndex = secondWindowIndex(clock());
        if (currentIndex != windowIndex) {
            windowIndex = currentIndex;
            over = false;
        }
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

    ReconnectCredential::~ReconnectCredential() { clear(); }
    ReconnectCredential::ReconnectCredential(const ReconnectCredential &other) : bytes(other.bytes) {}
    ReconnectCredential &ReconnectCredential::operator=(const ReconnectCredential &other) {
        if (this != &other) {
            clear();
            bytes = other.bytes;
        }
        return *this;
    }
    ReconnectCredential::ReconnectCredential(ReconnectCredential &&other) noexcept : bytes(other.bytes) {
        other.clear();
    }
    ReconnectCredential &ReconnectCredential::operator=(ReconnectCredential &&other) noexcept {
        if (this != &other) {
            clear();
            bytes = other.bytes;
            other.clear();
        }
        return *this;
    }
    void ReconnectCredential::clear() noexcept { secureErase(bytes.data(), bytes.size()); }

    ReconnectReservation::ReconnectReservation(std::uint64_t session, ParticipantId participant,
                                               std::uint64_t reservation, Clock clock, RandomFill random)
            : clock(selectedClock(std::move(clock))), random(random ? std::move(random) : RandomFill(secureRandom)),
              expiry(this->clock() + ReconnectCredentialLifetime), session(session), participant(participant),
              reservation(reservation) {
        if (session != 0 && participant != 0 && reservation != 0) generateLocked();
    }
    ReconnectReservation::~ReconnectReservation() { invalidate(); }
    bool ReconnectReservation::valid() {
        std::lock_guard<std::mutex> lock(mutex);
        expireIfDueLocked();
        return value.has_value();
    }
    ReconnectCredential ReconnectReservation::credential() {
        std::lock_guard<std::mutex> lock(mutex);
        expireIfDueLocked();
        if (!value) throw std::logic_error("Reconnect credential is unavailable");
        return ReconnectCredential(*value);
    }
    ReconnectAuthorizationResult ReconnectReservation::authorizeAndConsume(
            const ReconnectCredential &candidate, std::uint64_t expectedSession,
            ParticipantId expectedParticipant, std::uint64_t expectedReservation) {
        std::lock_guard<std::mutex> lock(mutex);
        expireIfDueLocked();
        const ReconnectCredential unavailable{};
        const ReconnectCredential &stored = value ? *value : unavailable;
        const bool credentialMatches = constantTimeEqual(stored, candidate);
        const bool accepted = value.has_value() && !allZero(candidate)
                              && session == expectedSession && participant == expectedParticipant
                              && reservation == expectedReservation && credentialMatches;
        if (accepted) invalidateLocked();
        return {accepted, !accepted, accepted ? std::string_view{} : ReconnectAuthorizationFailureCopy};
    }
    bool ReconnectReservation::consume(const ReconnectCredential &candidate, std::uint64_t expectedSession,
                                       ParticipantId expectedParticipant, std::uint64_t expectedReservation) {
        return authorizeAndConsume(candidate, expectedSession, expectedParticipant, expectedReservation).accepted;
    }
    bool ReconnectReservation::expireIfDue() {
        std::lock_guard<std::mutex> lock(mutex);
        return expireIfDueLocked();
    }
    bool ReconnectReservation::cancel(std::uint64_t expectedSession, ParticipantId expectedParticipant,
                                      std::uint64_t expectedReservation) {
        std::lock_guard<std::mutex> lock(mutex);
        expireIfDueLocked();
        if (!value || session != expectedSession || participant != expectedParticipant
            || reservation != expectedReservation) return false;
        invalidateLocked();
        return true;
    }
    bool ReconnectReservation::participantRemoved(std::uint64_t expectedSession,
                                                  ParticipantId expectedParticipant) {
        std::lock_guard<std::mutex> lock(mutex);
        if (session != expectedSession || participant != expectedParticipant) return false;
        const bool changed = value.has_value();
        invalidateLocked();
        return changed;
    }
    bool ReconnectReservation::sessionEnded(std::uint64_t expectedSession) {
        std::lock_guard<std::mutex> lock(mutex);
        if (session != expectedSession) return false;
        const bool changed = value.has_value();
        invalidateLocked();
        return changed;
    }
    bool ReconnectReservation::replace(std::uint64_t expectedSession, ParticipantId expectedParticipant,
                                       std::uint64_t expectedReservation, std::uint64_t replacementReservation) {
        std::lock_guard<std::mutex> lock(mutex);
        expireIfDueLocked();
        if (!value || replacementReservation == 0 || session != expectedSession
            || participant != expectedParticipant || reservation != expectedReservation) return false;
        ReconnectCredential replaced(std::move(*value));
        value.reset();
        reservation = replacementReservation;
        expiry = clock() + ReconnectCredentialLifetime;
        const bool generated = generateLocked(&replaced);
        replaced.clear();
        return generated;
    }
    void ReconnectReservation::invalidate() {
        std::lock_guard<std::mutex> lock(mutex);
        invalidateLocked();
    }
    bool ReconnectReservation::generateLocked(const ReconnectCredential *disallowed) {
        for (std::size_t attempt = 0; attempt < MaxReconnectCredentialGenerationAttempts; ++attempt) {
            ReconnectCredential candidate;
            const bool generated = random(candidate.bytes.data(), candidate.bytes.size());
            const bool repeated = disallowed && constantTimeEqual(candidate, *disallowed);
            if (generated && !allZero(candidate) && !repeated) {
                value.emplace(std::move(candidate));
                return true;
            }
            candidate.clear();
        }
        value.reset();
        return false;
    }
    bool ReconnectReservation::expireIfDueLocked() {
        if (!value || clock() < expiry) return false;
        invalidateLocked();
        return true;
    }
    void ReconnectReservation::invalidateLocked() {
        if (value) {
            value->clear();
            value.reset();
        }
    }

    bool guestContentMayLoadOrExecute() { return false; }
}
