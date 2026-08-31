#include "CanonicalMatchRuntime.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <utility>

#include "../Game.h"
#include "../GameMode.h"
#include "../GameResources.h"
#include "../GameSettings.h"
#include "../Player.h"
#include "../Weapon.h"
#include "../gamemodes/DeathMatch.h"
#include "../gamemodes/Predator.h"
#include "../gamemodes/TeamDeathMatch.h"

namespace Duel6::Server::Authoritative {
    namespace {
        std::uint32_t controllerState(std::uint32_t inputMask) {
            std::uint32_t state = 0;
            if (inputMask & MoveLeft) state |= Player::ButtonLeft;
            if (inputMask & MoveRight) state |= Player::ButtonRight;
            if (inputMask & Jump) state |= Player::ButtonUp;
            if (inputMask & Crouch) state |= Player::ButtonDown;
            if (inputMask & Shoot) state |= Player::ButtonShoot;
            if (inputMask & PickOrSwapWeapon) state |= Player::ButtonPick;
            if (inputMask & ShowStatus) state |= Player::ButtonStatus;
            return state;
        }

        PlayerStatistics statistics(const Person &person) {
            PlayerStatistics result;
            result.shots = static_cast<std::uint64_t>(person.getShots());
            result.hits = static_cast<std::uint64_t>(person.getHits());
            result.kills = static_cast<std::uint64_t>(person.getKills());
            result.deaths = static_cast<std::uint64_t>(person.getDeaths());
            result.assists = static_cast<std::uint64_t>(person.getAssistances());
            result.wins = static_cast<std::uint64_t>(person.getWins());
            result.penalties = static_cast<std::uint64_t>(person.getPenalties());
            result.damage = static_cast<std::uint64_t>(person.getTotalDamage());
            result.assistedDamage = static_cast<std::uint64_t>(person.getAssistedDamage());
            return result;
        }

        std::string weaponKey(std::string value) {
            for (char &character: value) {
                if (character == ' ') character = '-';
                else character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

        void digestValue(std::uint64_t &digest, std::uint64_t value) {
            for (unsigned shift = 0; shift < 64; shift += 8) {
                digest ^= (value >> shift) & UINT64_C(0xff);
                digest *= UINT64_C(1099511628211);
            }
        }

        bool fixedValue(Float32 value, std::int64_t &fixed) {
            if (!std::isfinite(value)) return false;
            const Float64 scaled = static_cast<Float64>(value) * 65536.0;
            if (scaled < static_cast<Float64>(std::numeric_limits<std::int64_t>::min())
                || scaled > static_cast<Float64>(std::numeric_limits<std::int64_t>::max())) return false;
            fixed = static_cast<std::int64_t>(std::llround(scaled));
            return true;
        }
    }

    CanonicalMatchRuntime::CanonicalMatchRuntime(MatchConfig config, std::vector<PlayerDefinition> roster,
                                                 std::string resourcesPath)
            : config(std::move(config)), roster(std::move(roster)), resourcesPath(std::move(resourcesPath)) {}

    CanonicalMatchRuntime::~CanonicalMatchRuntime() { endWorld(); }

    MatchRuntimeDependencies CanonicalMatchRuntime::createDependencies(MatchConfig config,
            std::vector<PlayerDefinition> roster, std::string resourcesPath) {
        auto runtime = std::make_shared<CanonicalMatchRuntime>(
                std::move(config), std::move(roster), std::move(resourcesPath));
        return runtime->dependencies();
    }

    MatchRuntimeDependencies CanonicalMatchRuntime::dependencies() {
        const auto self = shared_from_this();
        MatchRuntimeDependencies result;
        result.worldStart = [self](RoundStartDecision &decision) { return self->startWorld(decision); };
        result.worldTick = [self](Tick tick, bool simulate) { return self->tickWorld(tick, simulate); };
        result.worldInput = [self](Identity playerId, std::uint32_t mask) {
            return self->setPlayerInput(playerId, mask);
        };
        result.worldRemove = [self](Identity playerId) { return self->removePlayer(playerId); };
        result.worldSnapshot = [self] { return self->snapshot(); };
        result.worldEnd = [self] { self->endWorld(); };
        result.cleanup = [self] { return self->cleanup(); };
        return result;
    }

    bool CanonicalMatchRuntime::startWorld(RoundStartDecision &decision) {
        endWorld();
        try {
            if (!initialized) {
                resources = std::make_unique<GameResources>();
                resources->loadHeadless(resourcesPath);
                initialized = true;
            }
            settings = std::make_unique<GameSettings>();
            settings->setMaxRounds(1).setQuickLiquid(config.quickLiquid)
                    .setBurnableTrees(config.burnableTrees).setGlobalAssistances(config.assistance);
            for (const Weapon &weapon: Weapon::values()) {
                if (std::find(config.enabledWeapons.begin(), config.enabledWeapons.end(), weaponKey(weapon.getName()))
                    != config.enabledWeapons.end()) settings->enableWeapon(weapon, true);
            }
            if (settings->getEnabledWeapons().empty()) return false;

            if (config.mode == Mode::Deathmatch) mode = std::make_unique<DeathMatch>();
            else if (config.mode == Mode::Predator) mode = std::make_unique<Predator>();
            else mode = std::make_unique<TeamDeathMatch>(config.teamCount, config.friendlyFire);

            std::vector<std::string> names;
            activeRoster.clear();
            activeRoster.reserve(decision.rosterOrder.size());
            for (Identity id: decision.rosterOrder) {
                const auto player = std::find_if(roster.begin(), roster.end(), [id](const auto &entry) {
                    return entry.playerId == id;
                });
                if (player == roster.end()) return false;
                activeRoster.push_back(*player);
            }
            names.reserve(activeRoster.size());
            for (const auto &player: activeRoster) names.push_back(player.displayName);
            game = std::make_unique<Game>(*resources, *settings);
            game->startHeadlessRound(names, resourcesPath + "/" + decision.level, decision.mirrored, *mode);

            const auto &players = game->getPlayers();
            Level::StartingPositionList positions;
            game->getRound().getWorld().getLevel().findStartingPositions(positions);
            decision.startingWeaponIndices.clear();
            decision.startingAmmo.clear();
            decision.startingPositionOrder.clear();
            for (const Player &player: players) {
                const auto found = std::find_if(Weapon::values().begin(), Weapon::values().end(),
                        [&player](const Weapon &weapon) { return weapon == player.getWeapon(); });
                decision.startingWeaponIndices.push_back(static_cast<std::uint32_t>(
                        std::distance(Weapon::values().begin(), found)));
                decision.startingAmmo.push_back(static_cast<std::uint32_t>(std::max(player.getAmmo(), 0)));
                const auto position = std::find(positions.begin(), positions.end(), Level::StartingPosition(
                        static_cast<Int32>(std::floor(player.getPosition().x)),
                        static_cast<Int32>(std::floor(player.getPosition().y))));
                if (position == positions.end()) return false;
                decision.startingPositionOrder.push_back(static_cast<std::uint32_t>(
                        std::distance(positions.begin(), position)));
            }
            if (auto *predator = dynamic_cast<Predator *>(mode.get())) {
                const Player *selected = predator->getPredator();
                for (std::size_t index = 0; index < players.size(); ++index)
                    if (&players[index] == selected) decision.predatorPlayerId = activeRoster[index].playerId;
            }
            return true;
        } catch (...) {
            endWorld();
            return false;
        }
    }

    bool CanonicalMatchRuntime::tickWorld(Tick, bool simulate) {
        if (!game) return false;
        if (simulate) game->update(1.0f / static_cast<Float32>(FixedTickRate));
        return true;
    }

    bool CanonicalMatchRuntime::setPlayerInput(Identity playerId, std::uint32_t inputMask) {
        if (!game) return false;
        const auto definition = std::find_if(activeRoster.begin(), activeRoster.end(), [playerId](const auto &entry) {
            return entry.playerId == playerId;
        });
        if (definition == activeRoster.end()) return false;
        const std::size_t index = static_cast<std::size_t>(std::distance(activeRoster.begin(), definition));
        if (index >= game->getPlayers().size()) return false;
        game->getPlayers()[index].setControllerState(controllerState(inputMask));
        return true;
    }

    bool CanonicalMatchRuntime::removePlayer(Identity playerId) {
        if (!game) return false;
        const auto definition = std::find_if(activeRoster.begin(), activeRoster.end(), [playerId](const auto &entry) {
            return entry.playerId == playerId;
        });
        if (definition == activeRoster.end()) return false;
        const std::size_t index = static_cast<std::size_t>(std::distance(activeRoster.begin(), definition));
        if (index >= game->getPlayers().size()) return false;
        Player &player = game->getPlayers()[index];
        player.setControllerState(0);
        if (player.isAlive()) player.die();
        return true;
    }

    CanonicalWorldSnapshot CanonicalMatchRuntime::snapshot() const {
        CanonicalWorldSnapshot result;
        if (!game) return result;
        const auto &players = game->getPlayers();
        const World &world = game->getRound().getWorld();
        std::uint64_t digest = UINT64_C(14695981039346656037);
        digestValue(digest, static_cast<std::uint64_t>(game->getCurrentRound()));
        digestValue(digest, static_cast<std::uint64_t>(world.getLevel().getWaterLevel()));
        result.players.reserve(roster.size());
        for (const auto &definition: roster) {
            const auto active = std::find_if(activeRoster.begin(), activeRoster.end(), [&](const auto &entry) {
                return entry.playerId == definition.playerId;
            });
            if (active == activeRoster.end()) {
                CanonicalPlayerSnapshot player;
                player.playerId = definition.playerId;
                result.players.push_back(player);
                digestValue(digest, definition.playerId);
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(std::distance(activeRoster.begin(), active));
            CanonicalPlayerSnapshot player;
            player.playerId = definition.playerId;
            player.alive = players[index].isAlive();
            player.life = static_cast<std::int32_t>(std::max(players[index].getLife(), 0.0f));
            player.statistics = statistics(players[index].getPerson());
            result.players.push_back(player);
            std::int64_t positionX = 0, positionY = 0, velocityX = 0, velocityY = 0;
            if (!fixedValue(players[index].getPosition().x, positionX)
                || !fixedValue(players[index].getPosition().y, positionY)
                || !fixedValue(players[index].getVelocity().x, velocityX)
                || !fixedValue(players[index].getVelocity().y, velocityY)) return {};
            result.players.back().positionX = positionX;
            result.players.back().positionY = positionY;
            digestValue(digest, definition.playerId);
            digestValue(digest, static_cast<std::uint64_t>(positionX));
            digestValue(digest, static_cast<std::uint64_t>(positionY));
            digestValue(digest, static_cast<std::uint64_t>(velocityX));
            digestValue(digest, static_cast<std::uint64_t>(velocityY));
            digestValue(digest, static_cast<std::uint64_t>(std::max(players[index].getAmmo(), 0)));
            digestValue(digest, static_cast<std::uint64_t>(std::max(players[index].getLife(), 0.0f) * 256.0f));
        }
        std::size_t shotCount = 0;
        bool validShots = true;
        world.getShotList().forEach([&](const Shot &shot) {
            std::int64_t centreX = 0, centreY = 0, velocityX = 0, velocityY = 0;
            if (!fixedValue(shot.getCentre().x, centreX) || !fixedValue(shot.getCentre().y, centreY)
                || !fixedValue(shot.getVelocity().x, velocityX) || !fixedValue(shot.getVelocity().y, velocityY)) {
                validShots = false;
                return false;
            }
            digestValue(digest, static_cast<std::uint64_t>(centreX));
            digestValue(digest, static_cast<std::uint64_t>(centreY));
            digestValue(digest, static_cast<std::uint64_t>(velocityX));
            digestValue(digest, static_cast<std::uint64_t>(velocityY));
            ++shotCount;
            return shotCount <= 100000u;
        });
        if (!validShots || shotCount > 100000u) return {};
        result.roundOver = game->getRound().hasWinner();
        result.valid = true;
        result.stateDigest = digest == 0 ? 1 : digest;
        result.dynamicEntityCount = players.size() + shotCount;
        return result;
    }

    void CanonicalMatchRuntime::endWorld() {
        if (game) game->endHeadlessRound();
        game.reset();
        mode.reset();
        settings.reset();
    }

    bool CanonicalMatchRuntime::cleanup() {
        endWorld();
        resources.reset();
        return true;
    }
}
