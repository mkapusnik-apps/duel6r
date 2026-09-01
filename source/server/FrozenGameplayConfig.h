#ifndef DUEL6_SERVER_FROZENGAMEPLAYCONFIG_H
#define DUEL6_SERVER_FROZENGAMEPLAYCONFIG_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Duel6::Server::Authoritative {
    struct FrozenGameplayConfig {
        std::vector<std::string> enabledWeapons;
        std::uint32_t startingAmmoMinimum = 15;
        std::uint32_t startingAmmoMaximum = 15;
    };

    const std::vector<std::string> &canonicalWeaponKeys();
    bool parseFrozenGameplayConfig(std::string_view source, FrozenGameplayConfig &result) noexcept;
    bool loadFrozenGameplayConfig(const std::string &resourcesPath, FrozenGameplayConfig &result) noexcept;
}

#endif
