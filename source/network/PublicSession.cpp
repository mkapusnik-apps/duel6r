#include "PublicSession.h"
#include "NetworkTrustPolicy.h"
#include <algorithm>

namespace Duel6::Network::PublicSession {
    bool validInvite(std::string_view value) {
        return !value.empty() && value.size() <= 256
               && std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
    }
    void erase(std::string &value) noexcept {
        Trust::secureEraseMemory(value.data(), value.size());
        value.clear();
    }
    Secret::~Secret() { erase(value); }
    std::vector<std::uint8_t> terminalNotice(std::uint64_t sessionId, bool maintenance) {
        std::vector<std::uint8_t> result{'D', '6', 'P', 'E', static_cast<std::uint8_t>(maintenance ? 1 : 0)};
        for (unsigned i = 0; i < 8; ++i) result.push_back(static_cast<std::uint8_t>(sessionId >> (i * 8)));
        return result;
    }
    std::string_view terminalReason(const std::vector<std::uint8_t> &payload, std::uint64_t sessionId) {
        if (payload.size() != 13 || payload[0] != 'D' || payload[1] != '6' || payload[2] != 'P'
            || payload[3] != 'E' || payload[4] > 1 || sessionId == 0) return {};
        std::uint64_t identity = 0;
        for (unsigned i = 0; i < 8; ++i) identity |= std::uint64_t(payload[i + 5]) << (i * 8);
        return identity == sessionId ? (payload[4] ? Maintenance : ControllerExpired) : std::string_view{};
    }
    bool validEndpoint(std::string_view value) {
        if (value.empty() || value.size() > 253) return false;
        const auto alnum = [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };
        bool numeric = true;
        std::size_t start = 0, labels = 0;
        while (start < value.size()) {
            auto end = value.find('.', start);
            if (end == std::string_view::npos) end = value.size();
            const auto label = value.substr(start, end - start);
            if (label.empty() || label.size() > 63 || !alnum(label.front()) || !alnum(label.back())) return false;
            for (unsigned char c: label) {
                if (!alnum(c) && c != '-') return false;
                if (c < '0' || c > '9') numeric = false;
            }
            ++labels;
            start = end + 1;
        }
        if (value.back() == '.') return false;
        if (!numeric) return true;
        if (labels != 4) return false;
        unsigned octet = 0;
        for (char c: value) {
            if (c == '.') octet = 0;
            else { octet = octet * 10 + c - '0'; if (octet > 255) return false; }
        }
        return true;
    }
    std::string endpointIdentity(std::string_view value) {
        if (!validEndpoint(value) || !std::all_of(value.begin(), value.end(), [](char c) {
                return (c >= '0' && c <= '9') || c == '.';
            })) return std::string(value);
        // IPv4 octets are decimal under NET-JOIN-PUB-010, not libc's legacy octal syntax.
        std::string result;
        unsigned octet = 0;
        for (std::size_t i = 0; i <= value.size(); ++i) {
            if (i == value.size() || value[i] == '.') {
                if (!result.empty()) result += '.';
                result += std::to_string(octet); octet = 0;
            } else octet = octet * 10 + value[i] - '0';
        }
        return result;
    }
    std::vector<std::uint8_t> wrapAdmission(const std::vector<std::uint8_t> &request,
                                            std::string_view invitation) {
        if (!validInvite(invitation) || request.size() + invitation.size() + 6 > Trust::MaxAdmissionPayloadBytes)
            return {};
        std::vector<std::uint8_t> result{'D', '6', 'P', 'I',
                static_cast<std::uint8_t>(invitation.size() >> 8), static_cast<std::uint8_t>(invitation.size())};
        result.insert(result.end(), invitation.begin(), invitation.end());
        result.insert(result.end(), request.begin(), request.end());
        return result;
    }
    bool unwrapAdmission(std::vector<std::uint8_t> &request, std::string_view expected) {
        bool authorized = false;
        std::vector<std::uint8_t> payload;
        if (request.size() >= 6 && request[0] == 'D' && request[1] == '6'
            && request[2] == 'P' && request[3] == 'I') {
            const std::size_t size = (static_cast<std::size_t>(request[4]) << 8) | request[5];
            if (size <= 256 && request.size() > size + 6) {
                unsigned difference = static_cast<unsigned>(size ^ expected.size());
                for (std::size_t i = 0; i < expected.size(); ++i)
                    difference |= static_cast<unsigned>(static_cast<unsigned char>(expected[i])
                                  ^ (i < size ? request[6 + i] : 0));
                authorized = validInvite(expected) && difference == 0;
                if (authorized) payload.assign(request.begin() + 6 + size, request.end());
            }
        }
        Trust::secureEraseMemory(request.data(), request.size());
        request = std::move(payload);
        return authorized;
    }
}
