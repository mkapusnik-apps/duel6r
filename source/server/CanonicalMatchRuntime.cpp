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

        void digestText(std::uint64_t &digest, const std::string &value) {
            digestValue(digest, value.size());
            for (const unsigned char character: value) {
                digest ^= character;
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
            authoritativeRound = decision.roundNumber;
            previousLife.clear();
            previousStatistics.clear();
            previousEntities.clear();
            previousWaterLevel = 0;
            previousSuddenDeath = false;
            previousRoundOver = false;
            if (!initialized) {
                resources = std::make_unique<GameResources>();
                resources->loadHeadless(resourcesPath);
                initialized = true;
            }
            settings = std::make_unique<GameSettings>();
            settings->setMaxRounds(1).setQuickLiquid(config.quickLiquid)
                    .setBurnableTrees(config.burnableTrees).setGlobalAssistances(config.assistance)
                    .setAmmoRange({static_cast<Int32>(config.startingAmmoMinimum),
                                   static_cast<Int32>(config.startingAmmoMaximum)});
            for (const Weapon &weapon: Weapon::values()) {
                if (std::find(config.enabledWeapons.begin(), config.enabledWeapons.end(), weaponKey(weapon.getName()))
                    != config.enabledWeapons.end()) settings->enableWeapon(weapon, true);
            }
            if (settings->getEnabledWeapons().empty()) return false;

            if (config.mode == Mode::Deathmatch) mode = std::make_unique<DeathMatch>();
            else if (config.mode == Mode::Predator) mode = std::make_unique<Predator>();
            else mode = std::make_unique<TeamDeathMatch>(config.teamCount, config.friendlyFire);

            std::vector<std::string> names;
            std::vector<Size> rosterSlots;
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
            rosterSlots.reserve(activeRoster.size());
            for (const auto &player: activeRoster) {
                names.push_back(player.displayName);
                rosterSlots.push_back(player.rosterOrder);
            }
            game = std::make_unique<Game>(*resources, *settings);
            game->startHeadlessRound(names, resourcesPath + "/" + decision.level, rosterSlots,
                                     decision.mirrored, *mode);

            const auto &players = game->getPlayers();
            if (!config.fixedStartingWeapon.empty()) {
                const auto selected = std::find_if(Weapon::values().begin(), Weapon::values().end(), [&](const Weapon &weapon) {
                    return weaponKey(weapon.getName()) == config.fixedStartingWeapon;
                });
                if (selected == Weapon::values().end()) return false;
                for (Player &player: game->getPlayers()) player.setHeadlessLoadout(*selected, player.getAmmo());
            }
            Level::StartingPositionList positions;
            game->getRound().getWorld().getLevel().findStartingPositions(positions);
            if (config.compactSpawnLayout && players.size() > 1) {
                std::sort(positions.begin(), positions.end(), [](const auto &left, const auto &right) {
                    return left.second < right.second
                           || (left.second == right.second && left.first < right.first);
                });
                Level::StartingPositionList best;
                Int32 bestSpan = std::numeric_limits<Int32>::max();
                for (auto candidate = positions.begin(); candidate != positions.end(); ++candidate) {
                    Level::StartingPositionList selected{*candidate};
                    for (auto next = candidate + 1; next != positions.end() && selected.size() < players.size(); ++next) {
                        if (next->second != candidate->second) break;
                        if (next->first - selected.back().first >= 2) selected.push_back(*next);
                    }
                    if (selected.size() != players.size()) continue;
                    const Int32 span = selected.back().first - selected.front().first;
                    if (span < bestSpan) { best = std::move(selected); bestSpan = span; }
                }
                if (!best.empty()) {
                    for (std::size_t index = 0; index < players.size(); ++index)
                        game->getPlayers()[index].setHeadlessPosition(best[index].first, best[index].second);
                }
            }
            decision.startingWeaponIndices.clear();
            decision.startingAmmo.clear();
            decision.startingPositionOrder.clear();
            spawnIdentities.clear();
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
                spawnIdentities[activeRoster[&player - players.data()].playerId]
                        = decision.startingPositionOrder.back();
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

    bool CanonicalMatchRuntime::tickWorld(Tick tick, bool simulate) {
        if (!game) return false;
        if (simulate) game->update(1.0f / static_cast<Float32>(FixedTickRate));
        worldTick = tick + (simulate ? 1u : 0u);
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

    void CanonicalMatchRuntime::appendEvent(std::string kind, std::uint64_t entityId, Identity playerId,
                                             Identity targetPlayerId, std::int64_t value) {
        if (kind.empty() || kind.size() > 64 || nextEventSequence == 0) return;
        if (eventTrace.size() >= MaxCanonicalEvents) eventTrace.erase(eventTrace.begin());
        eventTrace.push_back({worldTick, nextEventSequence++, std::move(kind), entityId,
                              playerId, targetPlayerId, value});
    }

    CanonicalWorldSnapshot CanonicalMatchRuntime::snapshot() {
        CanonicalWorldSnapshot result;
        if (!game) return result;
        const auto &players = game->getPlayers();
        const World &world = game->getRound().getWorld();
        const std::uint64_t roundIdentity = static_cast<std::uint64_t>(authoritativeRound) << 48u;
        const auto entityIdentity = [roundIdentity](std::uint64_t category, std::uint64_t local) {
            return roundIdentity | (category << 40u) | local;
        };
        std::uint64_t digest = UINT64_C(14695981039346656037);
        digestValue(digest, static_cast<std::uint64_t>(game->getCurrentRound()));
        digestValue(digest, static_cast<std::uint64_t>(world.getLevel().getWaterLevel()));
        digestValue(digest, worldTick);
        result.worldTick = worldTick;
        result.waterLevel = world.getLevel().getWaterLevel();
        result.waterRaising = world.getLevel().isRaisingWater();
        result.suddenDeath = game->getRound().isSuddenDeath();
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
            player.rosterSlot = definition.rosterOrder;
            player.team = config.mode == Mode::TeamDeathmatch
                          ? static_cast<Team>(definition.rosterOrder % config.teamCount + 1) : Team::None;
            player.spawnIdentity = spawnIdentities[definition.playerId];
            player.alive = players[index].isAlive();
            player.life = static_cast<std::int32_t>(std::max(players[index].getLife(), 0.0f));
            player.weapon = weaponKey(players[index].getWeapon().getName());
            player.ammo = std::max(players[index].getAmmo(), 0);
            player.underWater = players[index].isUnderWater();
            player.drowning = players[index].getAir() <= 0.0f;
            player.crouching = players[index].isKneeling();
            player.hasWeapon = players[index].hasGun();
            if (players[index].getBonus()) player.timedBonus = players[index].getBonus()->getName();
            player.statistics = statistics(players[index].getPerson());
            result.players.push_back(player);
            std::int64_t positionX = 0, positionY = 0, velocityX = 0, velocityY = 0;
            std::int64_t reload = 0, charge = 0, air = 0, bonusRemaining = 0;
            if (!fixedValue(players[index].getPosition().x, positionX)
                || !fixedValue(players[index].getPosition().y, positionY)
                || !fixedValue(players[index].getVelocity().x, velocityX)
                || !fixedValue(players[index].getVelocity().y, velocityY)
                || !fixedValue(players[index].getReloadTime(), reload)
                || !fixedValue(players[index].getAir(), air)
                || !fixedValue(players[index].getBonusRemainingTime(), bonusRemaining)) return {};
            if (players[index].getWeapon().isChargeable()
                && !fixedValue(players[index].getChargeLevel(), charge)) return {};
            result.players.back().positionX = positionX;
            result.players.back().positionY = positionY;
            result.players.back().velocityX = velocityX;
            result.players.back().velocityY = velocityY;
            result.players.back().reload = reload;
            result.players.back().charge = charge;
            result.players.back().air = air;
            result.players.back().bonusRemaining = bonusRemaining;
            digestValue(digest, definition.playerId);
            digestValue(digest, definition.rosterOrder);
            digestValue(digest, result.players.back().spawnIdentity);
            digestValue(digest, static_cast<std::uint64_t>(positionX));
            digestValue(digest, static_cast<std::uint64_t>(positionY));
            digestValue(digest, static_cast<std::uint64_t>(velocityX));
            digestValue(digest, static_cast<std::uint64_t>(velocityY));
            digestValue(digest, static_cast<std::uint64_t>(result.players.back().ammo));
            digestValue(digest, static_cast<std::uint64_t>(std::max(players[index].getLife(), 0.0f) * 256.0f));
            digestValue(digest, static_cast<std::uint64_t>(reload));
            digestValue(digest, static_cast<std::uint64_t>(air));
            digestText(digest, result.players.back().weapon);
            digestText(digest, result.players.back().timedBonus);
            const auto previous = previousLife.find(definition.playerId);
            if (previous == previousLife.end()) appendEvent("player-spawned", 0, definition.playerId, 0,
                                                            result.players.back().spawnIdentity);
            else if (previous->second != result.players.back().life) {
                appendEvent(result.players.back().alive ? "player-life-changed" : "player-died", 0,
                            definition.playerId, 0, result.players.back().life - previous->second);
            }
            previousLife[definition.playerId] = result.players.back().life;
            const auto previousStats = previousStatistics.find(definition.playerId);
            if (previousStats != previousStatistics.end()) {
                const PlayerStatistics &before = previousStats->second;
                const PlayerStatistics &after = result.players.back().statistics;
                if (after.shots > before.shots)
                    appendEvent("shot-fired", 0, definition.playerId, 0,
                                static_cast<std::int64_t>(after.shots - before.shots));
                if (after.hits > before.hits)
                    appendEvent("shot-hit", 0, definition.playerId, 0,
                                static_cast<std::int64_t>(after.hits - before.hits));
                if (after.kills > before.kills)
                    appendEvent("player-killed", 0, definition.playerId, 0,
                                static_cast<std::int64_t>(after.kills - before.kills));
            }
            previousStatistics[definition.playerId] = result.players.back().statistics;
        }
        std::set<std::uint64_t> currentEntities;
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
            CanonicalEntitySnapshot entity;
            entity.stableId = entityIdentity(1, shot.getStableId());
            entity.kind = "projectile";
            entity.type = weaponKey(shot.getWeapon().getName());
            entity.positionX = centreX; entity.positionY = centreY;
            entity.velocityX = velocityX; entity.velocityY = velocityY;
            const auto owner = std::find_if(players.begin(), players.end(), [&](const Player &player) {
                return &player == &shot.getPlayer();
            });
            if (owner != players.end()) {
                const std::size_t ownerIndex = static_cast<std::size_t>(std::distance(players.begin(), owner));
                entity.ownerPlayerId = activeRoster[ownerIndex].playerId;
            }
            result.projectiles.push_back(entity);
            currentEntities.insert(entity.stableId);
            digestValue(digest, entity.stableId);
            digestValue(digest, entity.ownerPlayerId);
            digestText(digest, entity.type);
            ++shotCount;
            return shotCount <= MaxCanonicalEntities;
        });
        if (!validShots || shotCount > MaxCanonicalEntities) return {};

        for (const Bonus &bonus: world.getBonusList().getBonuses()) {
            CanonicalEntitySnapshot entity;
            entity.stableId = entityIdentity(2, bonus.getStableId()); entity.kind = "bonus";
            entity.type = bonus.getType()->getName(); entity.primaryValue = bonus.getDuration();
            if (!fixedValue(bonus.getPosition().x, entity.positionX)
                || !fixedValue(bonus.getPosition().y, entity.positionY)) return {};
            result.pickups.push_back(entity); currentEntities.insert(entity.stableId);
        }
        for (const LyingWeapon &weapon: world.getBonusList().getWeapons()) {
            CanonicalEntitySnapshot entity;
            entity.stableId = entityIdentity(3, weapon.getStableId()); entity.kind = "weapon-pickup";
            entity.type = weaponKey(weapon.getWeapon().getName()); entity.primaryValue = weapon.getBullets();
            if (!fixedValue(weapon.getPosition().x, entity.positionX)
                || !fixedValue(weapon.getPosition().y, entity.positionY)
                || !fixedValue(weapon.remainingReloadTime, entity.secondaryValue)) return {};
            result.pickups.push_back(entity); currentEntities.insert(entity.stableId);
        }
        std::uint64_t localId = 1;
        for (const Elevator &elevator: world.getElevatorList().values()) {
            CanonicalEntitySnapshot entity;
            entity.stableId = entityIdentity(4, localId++); entity.kind = "elevator"; entity.type = "elevator";
            if (!fixedValue(elevator.getPosition().x, entity.positionX)
                || !fixedValue(elevator.getPosition().y, entity.positionY)
                || !fixedValue(elevator.getVelocity().x, entity.velocityX)
                || !fixedValue(elevator.getVelocity().y, entity.velocityY)) return {};
            result.elevators.push_back(entity); currentEntities.insert(entity.stableId);
        }
        localId = 1;
        for (const Fire &fire: world.getFireList().values()) {
            CanonicalEntitySnapshot entity;
            entity.stableId = entityIdentity(5, localId++); entity.kind = "tree";
            entity.type = std::to_string(fire.getType().getId()); entity.active = !fire.isBurned();
            if (!fixedValue(fire.getPosition().x, entity.positionX)
                || !fixedValue(fire.getPosition().y, entity.positionY)) return {};
            result.trees.push_back(entity); currentEntities.insert(entity.stableId);
        }
        CanonicalEntitySnapshot water;
        water.stableId = entityIdentity(6, 1); water.kind = "hazard"; water.type = "water";
        water.primaryValue = result.waterLevel; water.active = result.waterRaising;
        result.hazards.push_back(water); currentEntities.insert(water.stableId);

        for (const auto entity: currentEntities)
            if (!previousEntities.count(entity)) appendEvent("entity-spawned", entity);
        for (const auto entity: previousEntities)
            if (!currentEntities.count(entity)) appendEvent("entity-removed", entity);
        previousEntities = std::move(currentEntities);
        if (previousWaterLevel != result.waterLevel) appendEvent("water-level-changed", water.stableId, 0, 0,
                                                                 result.waterLevel);
        if (!previousSuddenDeath && result.suddenDeath) appendEvent("sudden-death-started");
        result.roundOver = game->getRound().hasWinner();
        if (!previousRoundOver && result.roundOver) appendEvent("round-ended");
        previousWaterLevel = result.waterLevel;
        previousSuddenDeath = result.suddenDeath;
        previousRoundOver = result.roundOver;
        result.events = eventTrace;
        for (const auto &collection: {&result.projectiles, &result.pickups, &result.elevators,
                                     &result.hazards, &result.trees}) {
            for (const auto &entity: *collection) {
                digestValue(digest, entity.stableId); digestText(digest, entity.kind); digestText(digest, entity.type);
                digestValue(digest, static_cast<std::uint64_t>(entity.positionX));
                digestValue(digest, static_cast<std::uint64_t>(entity.positionY));
                digestValue(digest, static_cast<std::uint64_t>(entity.velocityX));
                digestValue(digest, static_cast<std::uint64_t>(entity.velocityY));
                digestValue(digest, static_cast<std::uint64_t>(entity.primaryValue));
                digestValue(digest, entity.active ? 1u : 0u);
            }
        }
        const std::size_t entityCount = players.size() + result.projectiles.size() + result.pickups.size()
                                        + result.elevators.size() + result.hazards.size() + result.trees.size();
        if (entityCount > MaxCanonicalEntities || result.events.size() > MaxCanonicalEvents) return {};
        result.valid = true;
        result.stateDigest = digest == 0 ? 1 : digest;
        result.dynamicEntityCount = entityCount;
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
