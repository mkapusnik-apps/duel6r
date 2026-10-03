#ifndef DUEL6_NETWORK_PUBLICSESSION_H
#define DUEL6_NETWORK_PUBLICSESSION_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Duel6::Network::PublicSession {
    inline constexpr const char *SecurityFailure =
            "Secure connection could not be established. Check the endpoint and try again.";
    inline constexpr const char *ControllerExpired =
            "Session ended because the controller did not reconnect.";
    inline constexpr const char *Maintenance =
            "Session ended for service maintenance. Connect again to start or join a new session.";
    bool validEndpoint(std::string_view value);
    std::string endpointIdentity(std::string_view value);
    bool validInvite(std::string_view value);
    void erase(std::string &value) noexcept;
    struct Secret final {
        std::string value;
        ~Secret();
        Secret() = default;
        Secret(const Secret &) = delete;
        Secret &operator=(const Secret &) = delete;
    };
    std::vector<std::uint8_t> wrapAdmission(const std::vector<std::uint8_t> &request,
                                            std::string_view invitation);
    // Removes and erases the invitation envelope regardless of authorization outcome.
    bool unwrapAdmission(std::vector<std::uint8_t> &request, std::string_view expected);
    std::vector<std::uint8_t> terminalNotice(std::uint64_t sessionId, bool maintenance);
    std::string_view terminalReason(const std::vector<std::uint8_t> &payload, std::uint64_t sessionId);
}
#endif
