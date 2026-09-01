#include "FrozenGameplayConfig.h"

#include <array>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace Duel6::Server::Authoritative {
    namespace {
        constexpr std::size_t MaximumConfigBytes = 64u * 1024u;
        constexpr std::size_t MaximumLineBytes = 512u;
        constexpr std::uint32_t MaximumStartingAmmo = 100000u;

        bool unsignedValue(const std::string &value, std::uint32_t &result) {
            if (value.empty() || value.size() > 10) return false;
            const char *begin = value.data();
            const char *end = begin + value.size();
            const auto parsed = std::from_chars(begin, end, result);
            return parsed.ec == std::errc{} && parsed.ptr == end;
        }

        bool noTrailing(std::istringstream &line) {
            std::string trailing;
            return !(line >> trailing);
        }
    }

    const std::vector<std::string> &canonicalWeaponKeys() {
        static const std::vector<std::string> keys = {
                "pistol", "bazooka", "lightning", "shotgun", "plasma", "laser", "machine-gun",
                "triton", "uzi", "bow", "slime", "double-laser", "kiss-of-death", "spray", "sling",
                "stopper-gun", "shit-thrower"};
        return keys;
    }

    bool parseFrozenGameplayConfig(std::string_view source, FrozenGameplayConfig &result) noexcept {
        try {
            if (source.size() > MaximumConfigBytes) return false;
            const auto &keys = canonicalWeaponKeys();
            std::vector<bool> enabled(keys.size(), true);
            std::unordered_set<std::uint32_t> configuredWeapons;
            bool ammoConfigured = false;
            std::uint32_t ammoMinimum = 15, ammoMaximum = 15;
            std::istringstream input{std::string(source)};
            std::string raw;
            while (std::getline(input, raw)) {
                if (raw.size() > MaximumLineBytes) return false;
                const std::size_t comment = raw.find("//");
                if (comment != std::string::npos) raw.resize(comment);
                std::istringstream line(raw);
                std::string command;
                if (!(line >> command)) continue;
                if (command == "gun") {
                    std::string indexValue, state;
                    std::uint32_t index = 0;
                    if (!(line >> indexValue >> state) || !noTrailing(line) || !unsignedValue(indexValue, index)
                        || index >= enabled.size() || !configuredWeapons.insert(index).second
                        || (state != "true" && state != "false")) return false;
                    enabled[index] = state == "true";
                } else if (command == "start_ammo_range") {
                    std::string minimumValue, maximumValue;
                    if (ammoConfigured || !(line >> minimumValue >> maximumValue) || !noTrailing(line)
                        || !unsignedValue(minimumValue, ammoMinimum) || !unsignedValue(maximumValue, ammoMaximum)
                        || ammoMinimum > ammoMaximum || ammoMaximum > MaximumStartingAmmo) return false;
                    ammoConfigured = true;
                } else if (command == "volume") {
                    std::string value;
                    std::uint32_t volume = 0;
                    if (!(line >> value) || !noTrailing(line) || !unsignedValue(value, volume) || volume > 128)
                        return false;
                } else if (command == "music") {
                    std::string state;
                    if (!(line >> state) || !noTrailing(line) || (state != "on" && state != "off")) return false;
                } else {
                    return false;
                }
            }
            FrozenGameplayConfig parsed;
            parsed.startingAmmoMinimum = ammoMinimum;
            parsed.startingAmmoMaximum = ammoMaximum;
            for (std::size_t index = 0; index < keys.size(); ++index)
                if (enabled[index]) parsed.enabledWeapons.push_back(keys[index]);
            if (parsed.enabledWeapons.empty()) return false;
            result = std::move(parsed);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool loadFrozenGameplayConfig(const std::string &resourcesPath, FrozenGameplayConfig &result) noexcept {
        try {
            std::ifstream input(resourcesPath + "/data/config.script", std::ios::binary);
            if (!input) return false;
            std::string source;
            std::array<char, 4096> buffer{};
            while (input) {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                source.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
                if (source.size() > MaximumConfigBytes) return false;
            }
            return input.eof() && parseFrozenGameplayConfig(source, result);
        } catch (...) {
            return false;
        }
    }
}
