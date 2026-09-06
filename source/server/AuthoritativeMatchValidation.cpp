#include "AuthoritativeMatchValidation.h"

#include <algorithm>
#include <set>
#include <string_view>

#include "../network/NetworkTrustPolicy.h"
#include "FrozenGameplayConfig.h"

namespace Duel6::Server::Authoritative {
    namespace {
        ValidationResult invalid(const char *diagnostic) { return {false, diagnostic}; }

        bool levelPath(std::string_view path) {
            return path.size() > 12 && path.compare(0, 7, "levels/") == 0
                   && path.compare(path.size() - 5, 5, ".json") == 0
                   && Network::Trust::validLogicalPath(path);
        }
    }

    ValidationResult validateMatchConfig(const MatchConfig &config,
                                         const std::vector<PlayerDefinition> &roster) {
        if (config.seed == 0) return invalid("seed-zero");
        if (config.roundLimit < 1 || config.roundLimit > 99) return invalid("round-limit");
        if (roster.size() < 2 || roster.size() > MaxPlayers) return invalid("roster-cardinality");
        if (config.hostParticipantId == 0) return invalid("host-identity");
        if (config.optionalScriptsEnabled) return invalid("optional-scripts");

        switch (config.mode) {
            case Mode::Deathmatch:
            case Mode::Predator:
            case Mode::TeamDeathmatch: break;
            default: return invalid("mode");
        }
        switch (config.levelPlan) {
            case LevelPlan::Fixed:
            case LevelPlan::ShuffleAll:
            case LevelPlan::Random: break;
            default: return invalid("level-plan");
        }

        if (config.mode == Mode::TeamDeathmatch) {
            if (config.teamCount < 2 || config.teamCount > 4) return invalid("team-count");
        } else if (config.teamCount != 0 || config.friendlyFire) {
            return invalid("non-team-settings");
        }

        if (config.playableLevels.size() > MaxLevels) return invalid("level-count");
        if (config.enabledWeapons.size() > 256) return invalid("weapon-count");
        if (config.startingAmmoMinimum > config.startingAmmoMaximum
            || config.startingAmmoMaximum > 100000u) return invalid("starting-ammo-range");
        std::set<std::string> levels;
        for (const auto &level: config.playableLevels) {
            if (!levelPath(level) || !levels.insert(level).second) return invalid("level-path");
        }
        std::set<std::string> weapons;
        const auto &knownWeapons = canonicalWeaponKeys();
        for (const auto &weapon: config.enabledWeapons) {
            if (std::find(knownWeapons.begin(), knownWeapons.end(), weapon) == knownWeapons.end()
                || !weapons.insert(weapon).second) return invalid("weapon-name");
        }
        if (!config.fixedStartingWeapon.empty() && !weapons.count(config.fixedStartingWeapon))
            return invalid("fixed-starting-weapon");
        if (config.levelPlan != LevelPlan::Fixed && !config.fixedLevel.empty()) {
            return invalid("unexpected-fixed-level");
        }

        std::set<Identity> participants;
        std::set<Identity> players;
        std::set<std::uint8_t> positions;
        bool hostFound = false;
        for (const auto &player: roster) {
            if (player.participantId == 0 || player.playerId == 0
                || player.participantId == player.playerId) return invalid("zero-or-overlapping-identity");
            if (!Network::Trust::validParticipantName(player.displayName)
                || player.displayName.size() > MaxDisplayNameBytes) return invalid("display-name");
            participants.insert(player.participantId);
            if (!players.insert(player.playerId).second || !positions.insert(player.rosterOrder).second)
                return invalid("duplicate-player-or-order");
            if (player.rosterOrder >= roster.size()) return invalid("roster-order-range");
            hostFound = hostFound || player.participantId == config.hostParticipantId;
        }
        if (!hostFound || participants.size() < 2 || participants.size() > MaxParticipants)
            return invalid("participant-cardinality");
        for (Identity participant: participants) {
            if (players.count(participant)) return invalid("identity-domain-overlap");
        }
        for (std::size_t index = 0; index < roster.size(); ++index) {
            if (!positions.count(static_cast<std::uint8_t>(index))) return invalid("roster-order-gap");
        }
        return {true, {}};
    }

    ValidationResult validateFrozenContent(const MatchConfig &config,
                                           const Network::GameplayManifest &manifest) {
        if (config.playableLevels.empty()) return invalid("levels-unavailable");
        if (config.enabledWeapons.empty()) return invalid("weapons-unavailable");
        if (config.levelPlan == LevelPlan::Fixed
            && (config.fixedLevel.empty()
                || std::find(config.playableLevels.begin(), config.playableLevels.end(), config.fixedLevel)
                   == config.playableLevels.end())) return invalid("fixed-level-unavailable");
        if (!Network::validCanonicalManifest(manifest)) return invalid("manifest-invalid");
        if (config.optionalScriptsEnabled) return invalid("optional-scripts");
        std::set<std::string> paths;
        std::set<std::string> frozenPlayableLevels;
        for (const auto &entry: manifest) {
            if (entry.logicalPath.compare(0, 8, "scripts/") == 0
                || entry.logicalPath.compare(0, 9, "profiles/") == 0)
                return invalid("script-content");
            paths.insert(entry.logicalPath);
            if (levelPath(entry.logicalPath)) frozenPlayableLevels.insert(entry.logicalPath);
        }
        if (!paths.count("data/blocks.json") || !paths.count("data/config.script"))
            return invalid("gameplay-data-missing");
        const std::set<std::string> configuredPlayableLevels(config.playableLevels.begin(),
                                                              config.playableLevels.end());
        if (configuredPlayableLevels != frozenPlayableLevels) return invalid("playable-level-set");
        return {true, {}};
    }
}
