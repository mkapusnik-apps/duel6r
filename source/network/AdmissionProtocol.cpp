#include "AdmissionProtocol.h"

#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace Duel6::Network {
    namespace {
        constexpr std::array<std::uint8_t, 4> RequestMagic{{'D', '6', 'R', 'A'}};
        constexpr std::array<std::uint8_t, 4> ResultMagic{{'D', '6', 'R', 'S'}};
        constexpr std::array<std::uint8_t, 4> AcceptanceMagic{{'D', '6', 'R', 'K'}};

        class Writer {
        public:
            void byte(std::uint8_t value) { bytes.push_back(value); }
            void uint16(std::uint16_t value) {
                byte(static_cast<std::uint8_t>(value >> 8u));
                byte(static_cast<std::uint8_t>(value));
            }
            void uint32(std::uint32_t value) {
                for (int shift = 24; shift >= 0; shift -= 8) byte(static_cast<std::uint8_t>(value >> shift));
            }
            void uint64(std::uint64_t value) {
                for (int shift = 56; shift >= 0; shift -= 8) byte(static_cast<std::uint8_t>(value >> shift));
            }
            void raw(const std::uint8_t *value, std::size_t size) {
                if (size > Trust::MaxAdmissionPayloadBytes - bytes.size())
                    throw std::length_error("Admission payload exceeds its bound");
                bytes.insert(bytes.end(), value, value + size);
            }
            template<std::size_t Size>
            void raw(const std::array<std::uint8_t, Size> &value) { raw(value.data(), value.size()); }
            void text(const std::string &value, std::size_t maximum) {
                if (value.size() > maximum || value.size() > std::numeric_limits<std::uint16_t>::max())
                    throw std::length_error("Admission string exceeds its bound");
                uint16(static_cast<std::uint16_t>(value.size()));
                raw(reinterpret_cast<const std::uint8_t *>(value.data()), value.size());
            }
            std::vector<std::uint8_t> finish() {
                if (bytes.empty() || bytes.size() > Trust::MaxAdmissionPayloadBytes)
                    throw std::length_error("Admission payload exceeds its bound");
                return std::move(bytes);
            }
        private:
            std::vector<std::uint8_t> bytes;
        };

        class Reader {
        public:
            explicit Reader(const std::vector<std::uint8_t> &bytes) : bytes(bytes) {
                if (bytes.empty() || bytes.size() > Trust::MaxAdmissionPayloadBytes)
                    throw std::length_error("Admission payload exceeds its bound");
            }
            std::uint8_t byte() {
                require(1);
                return bytes[offset++];
            }
            std::uint16_t uint16() {
                const std::uint16_t high = byte();
                return static_cast<std::uint16_t>((high << 8u) | byte());
            }
            std::uint32_t uint32() {
                std::uint32_t value = 0;
                for (int index = 0; index < 4; ++index) value = (value << 8u) | byte();
                return value;
            }
            std::uint64_t uint64() {
                std::uint64_t value = 0;
                for (int index = 0; index < 8; ++index) value = (value << 8u) | byte();
                return value;
            }
            std::string text(std::size_t maximum) {
                const std::size_t size = uint16();
                if (size > maximum) throw std::length_error("Admission string exceeds its bound");
                require(size);
                const auto *start = reinterpret_cast<const char *>(bytes.data() + offset);
                std::string result(start, size);
                offset += size;
                return result;
            }
            template<std::size_t Size>
            std::array<std::uint8_t, Size> fixed() {
                require(Size);
                std::array<std::uint8_t, Size> result{};
                std::copy_n(bytes.data() + offset, Size, result.data());
                offset += Size;
                return result;
            }
            void expectFinished() const {
                if (offset != bytes.size()) throw std::invalid_argument("Admission payload has trailing data");
            }
        private:
            void require(std::size_t size) const {
                if (size > bytes.size() - offset) throw std::invalid_argument("Admission payload is truncated");
            }
            const std::vector<std::uint8_t> &bytes;
            std::size_t offset = 0;
        };

        template<std::size_t Size>
        void expectMagic(Reader &reader, const std::array<std::uint8_t, Size> &magic) {
            if (reader.fixed<Size>() != magic) throw std::invalid_argument("Admission payload identifier is invalid");
        }

        bool validCapability(const std::string &capability) {
            return !capability.empty() && Trust::validAsciiReason(capability);
        }

        bool validResultCode(std::uint16_t code) {
            return code >= static_cast<std::uint16_t>(AdmissionResultCode::MalformedRequest)
                   && code <= static_cast<std::uint16_t>(AdmissionResultCode::Admitted);
        }
    }

    std::vector<std::uint8_t> serializeAdmissionRequest(const AdmissionRequest &request) {
        if (request.networkReleaseId.empty() || !Trust::validGeneralString(request.networkReleaseId)
            || request.localPlayerCount == 0 || request.localPlayerCount > Trust::MaxParticipants
            || !Trust::validCollectionSize(request.capabilities.size())
            || !validCanonicalManifest(request.gameplayManifest))
            throw std::invalid_argument("Admission request is invalid");
        Writer writer;
        writer.raw(RequestMagic);
        writer.uint32(request.protocolVersion);
        writer.text(request.networkReleaseId, Trust::MaxStringBytes);
        writer.uint16(static_cast<std::uint16_t>(request.capabilities.size()));
        for (const std::string &capability: request.capabilities) {
            if (!validCapability(capability)) throw std::invalid_argument("Admission capability is invalid");
            writer.text(capability, Trust::MaxReasonBytes);
        }
        if (std::set<std::string>(request.capabilities.begin(), request.capabilities.end()).size()
            != request.capabilities.size())
            throw std::invalid_argument("Admission capabilities must be unique");
        writer.byte(request.localPlayerCount);
        writer.uint16(static_cast<std::uint16_t>(request.gameplayManifest.size()));
        for (const GameplayManifestEntry &entry: request.gameplayManifest) {
            writer.text(entry.logicalPath, Trust::MaxLogicalPathBytes);
            writer.raw(entry.contentIdentity);
        }
        return writer.finish();
    }

    AdmissionRequest deserializeAdmissionRequest(const std::vector<std::uint8_t> &payload) {
        Reader reader(payload);
        expectMagic(reader, RequestMagic);
        AdmissionRequest request;
        request.protocolVersion = reader.uint32();
        request.networkReleaseId = reader.text(Trust::MaxStringBytes);
        if (request.networkReleaseId.empty()) throw std::invalid_argument("Admission release ID is missing");
        const std::size_t capabilityCount = reader.uint16();
        if (!Trust::validCollectionSize(capabilityCount)) throw std::length_error("Too many admission capabilities");
        request.capabilities.reserve(capabilityCount);
        for (std::size_t index = 0; index < capabilityCount; ++index) {
            std::string capability = reader.text(Trust::MaxReasonBytes);
            if (!validCapability(capability)) throw std::invalid_argument("Admission capability is invalid");
            request.capabilities.push_back(std::move(capability));
        }
        if (std::set<std::string>(request.capabilities.begin(), request.capabilities.end()).size()
            != request.capabilities.size())
            throw std::invalid_argument("Admission capabilities must be unique");
        request.localPlayerCount = reader.byte();
        if (request.localPlayerCount == 0 || request.localPlayerCount > Trust::MaxParticipants)
            throw std::invalid_argument("Admission local player count is invalid");
        const std::size_t manifestCount = reader.uint16();
        if (!Trust::validManifestEntryCount(manifestCount)) throw std::length_error("Admission manifest is too large");
        request.gameplayManifest.reserve(manifestCount);
        for (std::size_t index = 0; index < manifestCount; ++index) {
            GameplayManifestEntry entry;
            entry.logicalPath = reader.text(Trust::MaxLogicalPathBytes);
            entry.contentIdentity = reader.fixed<ContentIdentityBytes>();
            request.gameplayManifest.push_back(std::move(entry));
        }
        reader.expectFinished();
        return request;
    }

    std::vector<std::uint8_t> serializeAdmissionResult(const AdmissionResult &result) {
        if (!validResultCode(static_cast<std::uint16_t>(result.code)))
            throw std::invalid_argument("Admission result code is invalid");
        const bool admitted = result.code == AdmissionResultCode::Admitted;
        if (admitted != (result.participantId != 0) || admitted != !result.playerIds.empty()
            || result.playerIds.size() > Trust::MaxParticipants
            || std::find(result.playerIds.begin(), result.playerIds.end(), result.participantId) != result.playerIds.end()
            || std::set<std::uint64_t>(result.playerIds.begin(), result.playerIds.end()).size() != result.playerIds.size())
            throw std::invalid_argument("Admission result identities are invalid");
        Writer writer;
        writer.raw(ResultMagic);
        writer.uint16(static_cast<std::uint16_t>(result.code));
        writer.uint64(result.participantId);
        writer.byte(static_cast<std::uint8_t>(result.playerIds.size()));
        for (std::uint64_t playerId: result.playerIds) {
            if (playerId == 0) throw std::invalid_argument("Admission player identity is invalid");
            writer.uint64(playerId);
        }
        return writer.finish();
    }

    AdmissionResult deserializeAdmissionResult(const std::vector<std::uint8_t> &payload) {
        Reader reader(payload);
        expectMagic(reader, ResultMagic);
        const std::uint16_t code = reader.uint16();
        if (!validResultCode(code)) throw std::invalid_argument("Admission result code is invalid");
        AdmissionResult result;
        result.code = static_cast<AdmissionResultCode>(code);
        result.participantId = reader.uint64();
        const std::size_t playerCount = reader.byte();
        if (playerCount > Trust::MaxParticipants) throw std::length_error("Admission result has too many players");
        result.playerIds.reserve(playerCount);
        for (std::size_t index = 0; index < playerCount; ++index) result.playerIds.push_back(reader.uint64());
        reader.expectFinished();
        const bool admitted = result.code == AdmissionResultCode::Admitted;
        if (admitted != (result.participantId != 0) || admitted != !result.playerIds.empty()
            || std::any_of(result.playerIds.begin(), result.playerIds.end(), [](std::uint64_t id) { return id == 0; })
            || std::find(result.playerIds.begin(), result.playerIds.end(), result.participantId) != result.playerIds.end()
            || std::set<std::uint64_t>(result.playerIds.begin(), result.playerIds.end()).size() != result.playerIds.size())
            throw std::invalid_argument("Admission result identities are invalid");
        return result;
    }

    std::vector<std::uint8_t> serializeAdmissionAcceptance(const AdmissionAcceptance &acceptance) {
        if (acceptance.participantId == 0)
            throw std::invalid_argument("Admission acceptance participant identity is invalid");
        Writer writer;
        writer.raw(AcceptanceMagic);
        writer.uint64(acceptance.participantId);
        return writer.finish();
    }

    AdmissionAcceptance deserializeAdmissionAcceptance(const std::vector<std::uint8_t> &payload) {
        Reader reader(payload);
        expectMagic(reader, AcceptanceMagic);
        AdmissionAcceptance acceptance{reader.uint64()};
        reader.expectFinished();
        if (acceptance.participantId == 0)
            throw std::invalid_argument("Admission acceptance participant identity is invalid");
        return acceptance;
    }

    std::string_view admissionResultIdentifier(AdmissionResultCode code) {
        switch (code) {
            case AdmissionResultCode::MalformedRequest: return "malformed-request";
            case AdmissionResultCode::NotAuthorized: return "not-authorized";
            case AdmissionResultCode::ProtocolIncompatible: return "protocol-incompatible";
            case AdmissionResultCode::NetworkReleaseMismatch: return "network-release-mismatch";
            case AdmissionResultCode::RequiredCapabilityUnsupported: return "required-capability-unsupported";
            case AdmissionResultCode::GameplayContentManifestInvalid: return "gameplay-content-manifest-invalid";
            case AdmissionResultCode::GameplayContentMismatch: return "gameplay-content-mismatch";
            case AdmissionResultCode::MatchAlreadyStarted: return "match-already-started";
            case AdmissionResultCode::SessionFull: return "session-full";
            case AdmissionResultCode::HostPolicyRejected: return "host-policy-rejected";
            case AdmissionResultCode::Admitted: return "admitted";
        }
        return "malformed-request";
    }

    std::string_view admissionResultUserCopy(AdmissionResultCode code) {
        switch (code) {
            case AdmissionResultCode::MalformedRequest: return "Connection request rejected.";
            case AdmissionResultCode::NotAuthorized: return "Connection not authorized.";
            case AdmissionResultCode::ProtocolIncompatible:
            case AdmissionResultCode::NetworkReleaseMismatch:
            case AdmissionResultCode::RequiredCapabilityUnsupported:
                return "Network release mismatch. Use the same supported game release as the host.";
            case AdmissionResultCode::GameplayContentManifestInvalid:
                return "Gameplay content manifest is invalid. Use the host's exact supported gameplay content.";
            case AdmissionResultCode::GameplayContentMismatch:
                return "Gameplay content mismatch. Use the host's exact supported gameplay content.";
            case AdmissionResultCode::MatchAlreadyStarted:
                return "Match already started. Join-in-progress is not supported.";
            case AdmissionResultCode::SessionFull: return "Session is full.";
            case AdmissionResultCode::HostPolicyRejected: return "Host rejected the connection.";
            case AdmissionResultCode::Admitted: return {};
        }
        return "Connection request rejected.";
    }

    std::string_view reconnectCompatibilityIdentifier(ReconnectCompatibilityCode code) {
        switch (code) {
            case ReconnectCompatibilityCode::ProtocolIncompatible: return "reconnect-protocol-incompatible";
            case ReconnectCompatibilityCode::NetworkReleaseMismatch: return "reconnect-network-release-mismatch";
            case ReconnectCompatibilityCode::RequiredCapabilityUnsupported:
                return "reconnect-required-capability-unsupported";
            case ReconnectCompatibilityCode::GameplayContentInvalid: return "reconnect-gameplay-content-invalid";
            case ReconnectCompatibilityCode::GameplayContentMismatch: return "reconnect-gameplay-content-mismatch";
        }
        return "reconnect-protocol-incompatible";
    }

    std::string_view reconnectCompatibilityUserCopy(ReconnectCompatibilityCode code) {
        switch (code) {
            case ReconnectCompatibilityCode::ProtocolIncompatible:
            case ReconnectCompatibilityCode::NetworkReleaseMismatch:
            case ReconnectCompatibilityCode::RequiredCapabilityUnsupported:
                return "Network release mismatch. This session cannot be restored.";
            case ReconnectCompatibilityCode::GameplayContentInvalid:
            case ReconnectCompatibilityCode::GameplayContentMismatch:
                return "Gameplay content mismatch. This session cannot be restored.";
        }
        return "Network release mismatch. This session cannot be restored.";
    }

    bool hasRequiredAdmissionCapabilities(const std::vector<std::string> &capabilities) {
        for (std::string_view required: RequiredAdmissionCapabilities) {
            if (std::find(capabilities.begin(), capabilities.end(), required) == capabilities.end()) return false;
        }
        return true;
    }

    AdmissionRequest makeLocalAdmissionRequest(std::uint8_t localPlayers, GameplayManifest manifest) {
        AdmissionRequest request;
        request.localPlayerCount = localPlayers;
        request.gameplayManifest = std::move(manifest);
        request.capabilities.reserve(RequiredAdmissionCapabilities.size());
        for (std::string_view capability: RequiredAdmissionCapabilities) request.capabilities.emplace_back(capability);
        return request;
    }
}
