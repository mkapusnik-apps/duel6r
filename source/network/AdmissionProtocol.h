#ifndef DUEL6_NETWORK_ADMISSIONPROTOCOL_H
#define DUEL6_NETWORK_ADMISSIONPROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CompatibilityManifest.h"

namespace Duel6::Network {
    constexpr std::uint32_t AdmissionProtocolVersion = 1;
    inline constexpr std::string_view NetworkReleaseId = "duel6r-network-r1";
    inline constexpr std::array<std::string_view, 3> RequiredAdmissionCapabilities{{
            "d6r.compatibility-admission.v1",
            "d6r.gameplay-manifest.v1",
            "d6r.session-identity.v1"
    }};

    struct AdmissionRequest {
        std::uint32_t protocolVersion = AdmissionProtocolVersion;
        std::string networkReleaseId{NetworkReleaseId};
        std::vector<std::string> capabilities;
        std::uint8_t localPlayerCount = 1;
        GameplayManifest gameplayManifest;
    };

    enum class AdmissionResultCode : std::uint16_t {
        MalformedRequest = 1,
        NotAuthorized = 2,
        ProtocolIncompatible = 3,
        NetworkReleaseMismatch = 4,
        RequiredCapabilityUnsupported = 5,
        GameplayContentManifestInvalid = 6,
        GameplayContentMismatch = 7,
        MatchAlreadyStarted = 8,
        SessionFull = 9,
        HostPolicyRejected = 10,
        Admitted = 11
    };

    struct AdmissionResult {
        AdmissionResultCode code = AdmissionResultCode::MalformedRequest;
        std::uint64_t participantId = 0;
        std::vector<std::uint64_t> playerIds;

        bool admitted() const { return code == AdmissionResultCode::Admitted; }
    };

    struct AdmissionIdentitySet {
        std::uint64_t participantId = 0;
        std::vector<std::uint64_t> playerIds;
    };

    using AdmissionOfferPayload = AdmissionIdentitySet;
    using AdmissionAcceptance = AdmissionIdentitySet;
    using AdmissionConfirmation = AdmissionIdentitySet;

    inline constexpr std::string_view InvalidHostAdmissionMessageIdentifier =
            "invalid-host-admission-message";
    inline constexpr std::string_view InvalidHostAdmissionMessageCopy =
            "Connection ended before admission completed.";

    enum class ReconnectCompatibilityCode {
        ProtocolIncompatible,
        NetworkReleaseMismatch,
        RequiredCapabilityUnsupported,
        GameplayContentInvalid,
        GameplayContentMismatch
    };

    std::vector<std::uint8_t> serializeAdmissionRequest(const AdmissionRequest &request);
    AdmissionRequest deserializeAdmissionRequest(const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> serializeAdmissionResult(const AdmissionResult &result);
    AdmissionResult deserializeAdmissionResult(const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> serializeAdmissionOffer(const AdmissionOfferPayload &offer);
    AdmissionOfferPayload deserializeAdmissionOffer(const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> serializeAdmissionAcceptance(const AdmissionAcceptance &acceptance);
    AdmissionAcceptance deserializeAdmissionAcceptance(const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> serializeAdmissionConfirmation(const AdmissionConfirmation &confirmation);
    AdmissionConfirmation deserializeAdmissionConfirmation(const std::vector<std::uint8_t> &payload);
    bool validAdmissionIdentitySet(const AdmissionIdentitySet &identities,
                                   std::optional<std::size_t> expectedPlayerCount = std::nullopt);
    bool sameAdmissionIdentitySet(const AdmissionIdentitySet &left, const AdmissionIdentitySet &right);

    std::string_view admissionResultIdentifier(AdmissionResultCode code);
    std::string_view admissionResultUserCopy(AdmissionResultCode code);
    std::string_view reconnectCompatibilityIdentifier(ReconnectCompatibilityCode code);
    std::string_view reconnectCompatibilityUserCopy(ReconnectCompatibilityCode code);
    bool hasRequiredAdmissionCapabilities(const std::vector<std::string> &capabilities);
    AdmissionRequest makeLocalAdmissionRequest(std::uint8_t localPlayers, GameplayManifest manifest);
}

#endif
