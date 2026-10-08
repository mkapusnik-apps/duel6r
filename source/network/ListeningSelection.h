#ifndef DUEL6_NETWORK_LISTENINGSELECTION_H
#define DUEL6_NETWORK_LISTENINGSELECTION_H

#include "NetworkTrustPolicy.h"
#include <algorithm>

namespace Duel6::Network {
    // Editable journey state, separate from the immutable coverage of a started service.
    struct ListeningSelection {
        bool all = true;
        bool initialized = false;
        std::vector<std::string> selected;
        std::optional<std::vector<std::string>> available;

        void setAll(bool value) {
            if (!value && !initialized) {
                selected = available.value_or(std::vector<std::string>{});
                initialized = true;
            }
            all = value;
        }
        bool contains(const std::string &address) const {
            return std::find(selected.begin(), selected.end(), address) != selected.end();
        }
        bool eligible(const std::string &address) const {
            return available && std::find(available->begin(), available->end(), address) != available->end();
        }
        void toggle(const std::string &address) {
            if (all) return;
            const auto found = std::find(selected.begin(), selected.end(), address);
            if (found != selected.end()) selected.erase(found);
            else if (eligible(address)) selected.push_back(address);
        }
        std::vector<std::string> rows() const {
            auto result = available.value_or(std::vector<std::string>{});
            if (!all) for (const auto &address: selected)
                if (std::find(result.begin(), result.end(), address) == result.end()) result.push_back(address);
            return result;
        }
        std::string validation() const {
            if (all) return {};
            if (selected.empty()) return "Select at least one listening interface.";
            if (!available) return "Listening interfaces could not be verified. Use Listen on all or correct the selection.";
            for (const auto &address: selected) if (!eligible(address))
                return "Selected listening interface is no longer available. Choose another interface.";
            return {};
        }
    };

    // Pure decision shared by publication and tests. Numeric IPv4 order, public before private.
    inline std::optional<std::string> publicationAddress(
            const std::vector<std::string> &coverage,
            const std::optional<std::vector<std::string>> &available) {
        if (!available || coverage.empty()) return {};
        const bool wildcard = coverage.size() == 1 && coverage.front() == "0.0.0.0";
        std::optional<std::string> best;
        std::array<std::uint8_t, 4> bestBytes{};
        bool bestPublic = false;
        for (const auto &address: *available) {
            if (!wildcard && std::find(coverage.begin(), coverage.end(), address) == coverage.end()) continue;
            std::array<std::uint8_t, 4> bytes{};
            const auto scope = Trust::classifyIpv4Literal(address, &bytes);
            const bool isPublic = scope == Trust::EndpointScope::PublicUnicast;
            if (!isPublic && scope != Trust::EndpointScope::PrivateLan) continue;
            if (!best || (isPublic && !bestPublic) || (isPublic == bestPublic && bytes < bestBytes)) {
                best = address; bestBytes = bytes; bestPublic = isPublic;
            }
        }
        return best;
    }
}
#endif
