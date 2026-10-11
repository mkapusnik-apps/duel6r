#ifndef DUEL6_NETWORK_PUBLICSESSION_H
#define DUEL6_NETWORK_PUBLICSESSION_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Duel6::Network::PublicSession {
    inline constexpr bool supported() {
#if defined(D6R_PUBLIC_DEDICATED) && !defined(D6_MACOS_PLATFORM)
        return true;
#else
        return false;
#endif
    }
    inline constexpr const char *SecurityFailure =
            "Secure connection could not be established. Check the endpoint and try again.";
    inline constexpr const char *ControllerExpired =
            "Session ended because the controller did not reconnect.";
    inline constexpr const char *Maintenance =
            "Session ended for service maintenance. Connect again to start or join a new session.";
    bool validEndpoint(std::string_view value);
    bool validDirectoryFrontend(std::string_view value);
    std::string endpointIdentity(std::string_view value);
    std::vector<std::uint8_t> terminalNotice(std::uint64_t sessionId, bool maintenance);
    std::string_view terminalReason(const std::vector<std::uint8_t> &payload, std::uint64_t sessionId);
}
#endif
