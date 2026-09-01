#include "AuthoritativeMatch.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

#include "AuthoritativeMatchValidation.h"
#include "../math/Math.h"

namespace Duel6::Server::Authoritative {
    namespace {
        constexpr std::uint64_t MaximumCounter = UINT64_C(1000000000);

        PlayerStatistics subtractStatistics(const PlayerStatistics &value, const PlayerStatistics &start) {
            PlayerStatistics result;
            result.roundsPlayed = value.roundsPlayed - start.roundsPlayed;
            result.shots = value.shots - start.shots;
            result.hits = value.hits - start.hits;
            result.kills = value.kills - start.kills;
            result.deaths = value.deaths - start.deaths;
            result.assists = value.assists - start.assists;
            result.wins = value.wins - start.wins;
            result.penalties = value.penalties - start.penalties;
            result.survivalTicks = value.survivalTicks - start.survivalTicks;
            result.damage = value.damage - start.damage;
            result.assistedDamage = value.assistedDamage - start.assistedDamage;
            return result;
        }

        bool playerRanking(const PlayerResultRow &left, const PlayerResultRow &right) {
            if (left.statistics.totalPoints() != right.statistics.totalPoints())
                return left.statistics.totalPoints() > right.statistics.totalPoints();
            if (left.statistics.wins != right.statistics.wins)
                return left.statistics.wins > right.statistics.wins;
            if (left.statistics.damage != right.statistics.damage)
                return left.statistics.damage > right.statistics.damage;
            return left.rosterOrder < right.rosterOrder;
        }
    }

    std::int64_t PlayerStatistics::totalPoints() const {
        const std::uint64_t positive = kills + wins + assists;
        if (positive > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::numeric_limits<std::int64_t>::max();
        const std::int64_t signedPositive = static_cast<std::int64_t>(positive);
        if (penalties > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::numeric_limits<std::int64_t>::min();
        return signedPositive - static_cast<std::int64_t>(penalties);
    }

    const char *modeName(Mode mode) {
        switch (mode) {
            case Mode::Deathmatch: return "Deathmatch";
            case Mode::Predator: return "Predator";
            case Mode::TeamDeathmatch: return "Team deathmatch";
        }
        return "Unknown";
    }

    const char *levelPlanName(LevelPlan plan) {
        switch (plan) {
            case LevelPlan::Fixed: return "Fixed level";
            case LevelPlan::ShuffleAll: return "Shuffle all levels";
            case LevelPlan::Random: return "Random level";
        }
        return "Unknown";
    }

    const char *teamName(Team team) {
        switch (team) {
            case Team::None: return "";
            case Team::Alpha: return "Alpha";
            case Team::Bravo: return "Bravo";
            case Team::Charlie: return "Charlie";
            case Team::Delta: return "Delta";
        }
        return "";
    }

    const char *phaseName(MatchPhase phase) {
        switch (phase) {
            case MatchPhase::Lobby: return "lobby";
            case MatchPhase::ActiveRound: return "active-round";
            case MatchPhase::RoundEndActive: return "round-end-active";
            case MatchPhase::RoundEndFrozen: return "round-end-frozen";
            case MatchPhase::Completed: return "completed";
            case MatchPhase::Failed: return "failed";
            case MatchPhase::Ended: return "ended";
        }
        return "failed";
    }

    TerminalOutcome terminalOutcome(OutcomeCode code) {
        switch (code) {
            case OutcomeCode::Completed:
                return {code, 0, "authoritative-match-completed", "Authoritative match completed."};
            case OutcomeCode::InterruptedNoWinner:
                return {code, 0, "authoritative-match-interrupted-no-winner",
                        "Authoritative match ended without a winner."};
            case OutcomeCode::EndedIntentionally:
                return {code, 0, "authoritative-match-ended-intentionally",
                        "Authoritative match ended by the host."};
            case OutcomeCode::SettingsInvalid:
                return {code, 2, "authoritative-match-settings-invalid",
                        "Match settings are invalid. Correct the settings and try again."};
            case OutcomeCode::ContentUnavailable:
                return {code, 2, "authoritative-match-content-unavailable",
                        "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application."};
            case OutcomeCode::RuntimeFailed:
                return {code, 3, "authoritative-match-runtime-failed",
                        "The authoritative match stopped unexpectedly."};
            case OutcomeCode::ShutdownFailed:
                return {code, 4, "authoritative-match-shutdown-failed",
                        "Authoritative match cleanup did not complete."};
            case OutcomeCode::None: break;
        }
        return {OutcomeCode::None, 3, {}, {}};
    }

    AuthoritativeMatch::AuthoritativeMatch(MatchRuntimeDependencies dependencies)
            : dependencies(std::move(dependencies)) {
        if (!this->dependencies.worldStart) this->dependencies.worldStart = [](const auto &) { return true; };
        if (!this->dependencies.worldTick) this->dependencies.worldTick = [](Tick, bool) { return true; };
        if (!this->dependencies.worldEnd) this->dependencies.worldEnd = [] {};
        if (!this->dependencies.cleanup) this->dependencies.cleanup = [] { return true; };
        if (!this->dependencies.actionSource) this->dependencies.actionSource = [](Tick) {
            return std::vector<AuthoritativeAction>{};
        };
        if (!this->dependencies.clock) this->dependencies.clock = [this] { return tick; };
    }

    AuthoritativeMatch::~AuthoritativeMatch() {
        if (!released && !cleanupAttempted) {
            try { shutdown(); } catch (...) {}
        }
        else endWorld();
    }

    TerminalOutcome AuthoritativeMatch::start(MatchConfig requested, std::vector<PlayerDefinition> roster,
                                               const Network::GameplayManifest &manifest) {
        if (contentStartBlocked) return terminalOutcome(OutcomeCode::ContentUnavailable);
        if (currentPhase != MatchPhase::Lobby || terminal.code != OutcomeCode::None)
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        if (requested.seed == 0 && dependencies.seedSource) {
            try { requested.seed = dependencies.seedSource(); } catch (...) { requested.seed = 0; }
        }
        const ValidationResult settings = validateMatchConfig(requested, roster);
        if (!settings.valid) {
            currentPhase = MatchPhase::Lobby;
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        }
        const ValidationResult content = validateFrozenContent(requested, manifest);
        if (!content.valid) {
            contentStartBlocked = true;
            currentPhase = MatchPhase::Lobby;
            return terminalOutcome(OutcomeCode::ContentUnavailable);
        }

        config = std::move(requested);
        random = std::make_unique<DeterministicRandom>(config.seed);
        std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
            return left.rosterOrder < right.rosterOrder;
        });
        players.clear();
        players.reserve(roster.size());
        for (auto &definition: roster) {
            PlayerState state;
            state.definition = std::move(definition);
            if (config.mode == Mode::TeamDeathmatch)
                state.team = static_cast<Team>(state.definition.rosterOrder % config.teamCount + 1);
            players.push_back(std::move(state));
        }
        shuffledLevels = config.playableLevels;
        if (config.levelPlan == LevelPlan::ShuffleAll) random->shuffle(shuffledLevels, "level-plan-shuffle");
        released = false;
        cleanupAttempted = false;
        result.reset();
        completedRounds.clear();
        completedPlayerStatistics.clear();
        latestStateDigest = 0;
        latestCanonicalSnapshot.reset();
        checkpoints.clear();
        terminal = terminalOutcome(OutcomeCode::None);
        if (!startRound()) failRuntime();
        return terminal;
    }

    AuthoritativeMatch::PlayerState *AuthoritativeMatch::findPlayer(Identity id) {
        auto found = std::find_if(players.begin(), players.end(), [id](const auto &entry) {
            return entry.definition.playerId == id;
        });
        return found == players.end() ? nullptr : &*found;
    }

    const AuthoritativeMatch::PlayerState *AuthoritativeMatch::findPlayer(Identity id) const {
        auto found = std::find_if(players.begin(), players.end(), [id](const auto &entry) {
            return entry.definition.playerId == id;
        });
        return found == players.end() ? nullptr : &*found;
    }

    bool AuthoritativeMatch::isHost(Identity participant) const {
        return participant != 0 && participant == config.hostParticipantId;
    }

    bool AuthoritativeMatch::checkedAdd(std::uint64_t &target, std::uint64_t amount) {
        if (amount > MaximumCounter || target > MaximumCounter - amount) return false;
        target += amount;
        return true;
    }

    bool AuthoritativeMatch::startRound() {
        if (!random || completedRoundCount >= config.roundLimit) return false;
        Math::RandomScope authoritativeScope(random.get());
        endWorld();
        currentRoundDecision = {};
        currentRoundDecision.roundNumber = static_cast<std::uint8_t>(completedRoundCount + 1);
        if (config.levelPlan == LevelPlan::Fixed) currentRoundDecision.level = config.fixedLevel;
        else if (config.levelPlan == LevelPlan::ShuffleAll)
            currentRoundDecision.level = shuffledLevels[completedRoundCount % shuffledLevels.size()];
        else currentRoundDecision.level = config.playableLevels[
                random->bounded(config.playableLevels.size(), "round-level")];
        currentRoundDecision.mirrored = random->bounded(2, "round-orientation") == 0;
        currentRoundDecision.rosterOrder.clear();
        currentRoundDecision.startingAmmo.clear();
        currentRoundDecision.startingWeaponIndices.clear();
        currentRoundDecision.startingPositionOrder.clear();
        std::vector<std::uint32_t> positions;
        for (std::size_t index = 0; index < players.size(); ++index)
            if (!players[index].departed) positions.push_back(static_cast<std::uint32_t>(index));
        random->shuffle(positions, "round-position-order");
        std::vector<Identity> active;
        for (auto &player: players) {
            player.alive = !player.departed;
            player.life = MaximumLife;
            player.attackers.clear();
            player.lastInputTick = static_cast<Tick>(-1);
            player.roundStart = player.total;
            if (!player.departed) {
                if (!checkedAdd(player.total.roundsPlayed, 1)) return false;
                active.push_back(player.definition.playerId);
                currentRoundDecision.rosterOrder.push_back(player.definition.playerId);
            }
        }
        currentRoundDecision.startingPositionOrder = std::move(positions);
        predatorPlayer = 0;
        if (config.mode == Mode::Predator) {
            predatorPlayer = active[random->bounded(active.size(), "predator-selection")];
            currentRoundDecision.predatorPlayerId = predatorPlayer;
        }
        for (const auto &player: players) {
            if (player.departed) continue;
            currentRoundDecision.startingWeaponIndices.push_back(
                    static_cast<std::uint32_t>(random->bounded(config.enabledWeapons.size(), "starting-weapon")));
            const std::uint64_t ammoSpan = static_cast<std::uint64_t>(config.startingAmmoMaximum)
                                           - config.startingAmmoMinimum + 1u;
            std::uint32_t ammunition = config.startingAmmoMinimum
                                       + static_cast<std::uint32_t>(random->bounded(ammoSpan, "starting-ammo"));
            if (config.mode == Mode::Predator && player.definition.playerId != predatorPlayer)
                ammunition += 10;
            currentRoundDecision.startingAmmo.push_back(ammunition);
        }
        currentRoundNoWinner = false;
        currentRoundWinners.clear();
        currentRoundWinningTeam = Team::None;
        roundEndTicks = 0;
        try { worldActive = dependencies.worldStart(currentRoundDecision); }
        catch (...) { worldActive = false; }
        if (!worldActive) return false;
        if (config.mode == Mode::Predator) predatorPlayer = currentRoundDecision.predatorPlayerId;
        currentPhase = MatchPhase::ActiveRound;
        if (dependencies.worldSnapshot && !synchronizeCanonicalWorld()) {
            endWorld();
            return false;
        }
        return true;
    }

    ActionResult AuthoritativeMatch::submit(const AuthoritativeAction &action) {
        std::unique_ptr<Math::RandomScope> authoritativeScope;
        try {
            if (random) authoritativeScope = std::make_unique<Math::RandomScope>(random.get());
        } catch (...) {
            failRuntime();
            return ActionResult::RuntimeFailed;
        }
        if (currentPhase == MatchPhase::Failed || currentPhase == MatchPhase::Completed
            || currentPhase == MatchPhase::Ended || terminal.code != OutcomeCode::None)
            return ActionResult::RejectedPhase;
        if (action.tick != tick || action.sequence == 0 || action.sequence <= lastSequence)
            return ActionResult::RejectedOrder;
        if (action.tick != actionTick) { actionTick = action.tick; actionsThisTick = 0; }
        if (++actionsThisTick > MaxActionsPerTick || ++totalActions > MaxActions) {
            failRuntime();
            return ActionResult::RejectedLimit;
        }
        if (action.kind == ActionKind::AdvanceRound || action.kind == ActionKind::EndSession
            || action.kind == ActionKind::RemovePlayer || action.kind == ActionKind::RuntimeFailure) {
            if (!isHost(action.participantId) || action.playerId != 0)
                return ActionResult::RejectedAuthority;
            if (action.amount != 0 || action.inputMask != 0
                || (action.kind != ActionKind::RemovePlayer && action.targetPlayerId != 0))
                return ActionResult::RejectedValue;
            if (action.kind == ActionKind::EndSession) {
                lastSequence = action.sequence;
                endWorld();
                result.reset();
                currentPhase = MatchPhase::Ended;
                terminal = terminalOutcome(OutcomeCode::EndedIntentionally);
                return ActionResult::Accepted;
            }
            if (action.kind == ActionKind::RuntimeFailure) {
                lastSequence = action.sequence;
                failRuntime();
                return ActionResult::RuntimeFailed;
            }
            if (action.kind == ActionKind::AdvanceRound) {
                if ((currentPhase != MatchPhase::RoundEndActive && currentPhase != MatchPhase::RoundEndFrozen)
                    || completedRoundCount + 1 >= config.roundLimit) return ActionResult::RejectedPhase;
                finishRound();
                if (terminal.code == OutcomeCode::None && !startRound()) failRuntime();
                lastSequence = action.sequence;
                return terminal.code == OutcomeCode::RuntimeFailed
                       ? ActionResult::RuntimeFailed : ActionResult::Accepted;
            }
            PlayerState *removed = findPlayer(action.targetPlayerId);
            if (!removed || removed->departed || action.targetPlayerId == 0)
                return ActionResult::RejectedValue;
            if (dependencies.worldRemove) {
                try {
                    if (!dependencies.worldRemove(action.targetPlayerId)) return ActionResult::RejectedValue;
                } catch (...) {
                    failRuntime();
                    return ActionResult::RuntimeFailed;
                }
            }
            removed->departed = true;
            removed->alive = false;
            interruptIfRosterTooSmall();
            if (terminal.code == OutcomeCode::None && currentPhase == MatchPhase::ActiveRound)
                evaluateRoundOutcome();
            lastSequence = action.sequence;
            return ActionResult::Accepted;
        }

        if (currentPhase != MatchPhase::ActiveRound && currentPhase != MatchPhase::RoundEndActive)
            return ActionResult::RejectedPhase;
        PlayerState *source = findPlayer(action.playerId);
        if (!source || source->departed || source->definition.participantId != action.participantId)
            return ActionResult::RejectedAuthority;
        bool applied = false;
        if (action.kind == ActionKind::PlayerInput) applied = applyPlayerInput(*source, action);
        else if (action.kind == ActionKind::ShotDamage) {
            if (dependencies.worldSnapshot) return ActionResult::RejectedAuthority;
            if (action.inputMask != 0 || action.targetPlayerId == 0) return ActionResult::RejectedValue;
            PlayerState *target = findPlayer(action.targetPlayerId);
            applied = target && applyShotDamage(*source, *target, action.amount);
        } else if (action.kind == ActionKind::EnvironmentalDamage) {
            if (dependencies.worldSnapshot) return ActionResult::RejectedAuthority;
            if (action.inputMask != 0 || action.targetPlayerId != action.playerId)
                return ActionResult::RejectedValue;
            applied = applyEnvironmentalDamage(*source, action.amount);
        }
        if (!applied) return terminal.code == OutcomeCode::RuntimeFailed
                             ? ActionResult::RuntimeFailed : ActionResult::RejectedValue;
        evaluateRoundOutcome();
        lastSequence = action.sequence;
        return ActionResult::Accepted;
    }

    bool AuthoritativeMatch::applyPlayerInput(PlayerState &player, const AuthoritativeAction &action) {
        if (!player.alive || action.inputMask == 0 || (action.inputMask & ~AllPlayerInputs) != 0
            || action.amount != 0 || action.targetPlayerId != 0 || player.lastInputTick == tick) return false;
        player.lastInputTick = tick;
        if (dependencies.worldInput) {
            try {
                if (!dependencies.worldInput(player.definition.playerId, action.inputMask)) return false;
            } catch (...) {
                failRuntime();
                return false;
            }
        }
        if (!dependencies.worldSnapshot && (action.inputMask & Shoot) != 0 && !checkedAdd(player.total.shots, 1)) {
            failRuntime();
            return false;
        }
        return true;
    }

    bool AuthoritativeMatch::applyShotDamage(PlayerState &source, PlayerState &target, std::int32_t amount) {
        if (!source.alive || !target.alive || amount <= 0 || amount > MaximumLife) return false;
        if (config.mode == Mode::TeamDeathmatch && !config.friendlyFire && &source != &target
            && source.team == target.team)
            return true;
        std::int32_t applied = amount;
        if (config.mode == Mode::Predator && target.definition.playerId == predatorPlayer)
            applied = amount * 30 / 100;
        applied = std::min(applied, target.life);
        if (applied < 0) return false;
        if (&source != &target) {
            if (!checkedAdd(source.total.hits, 1)
                || !checkedAdd(source.total.damage, static_cast<std::uint64_t>(applied))) {
                failRuntime();
                return false;
            }
            auto &record = target.attackers[source.definition.playerId];
            if (!checkedAdd(record.damage, static_cast<std::uint64_t>(applied))) {
                failRuntime();
                return false;
            }
        }
        target.life -= applied;
        return target.life > 0 || applyDeath(target, &source, false);
    }

    bool AuthoritativeMatch::applyEnvironmentalDamage(PlayerState &target, std::int32_t amount) {
        if (!target.alive || amount <= 0 || amount > MaximumLife) return false;
        target.life -= std::min(amount, target.life);
        return target.life > 0 || applyDeath(target, nullptr, true);
    }

    bool AuthoritativeMatch::applyDeath(PlayerState &target, PlayerState *killer, bool environmental) {
        if (!target.alive) return false;
        target.alive = false;
        if (!checkedAdd(target.total.deaths, 1)) { failRuntime(); return false; }
        const bool suicide = !environmental && killer == &target;
        if (environmental || suicide) {
            if (!checkedAdd(target.total.penalties, 1)) { failRuntime(); return false; }
        } else if (killer) {
            if (config.mode == Mode::TeamDeathmatch && killer->team == target.team) {
                if (!checkedAdd(killer->total.penalties, 1)) { failRuntime(); return false; }
            } else if (!checkedAdd(killer->total.kills, 1)) { failRuntime(); return false; }
        }
        return environmental || awardAssists(target, killer, suicide);
    }

    bool AuthoritativeMatch::awardAssists(PlayerState &target, PlayerState *killer, bool suicide) {
        for (const auto &entry: target.attackers) {
            PlayerState *assistant = findPlayer(entry.first);
            if (!assistant || assistant == killer || entry.second.damage <= 40) continue;
            bool qualifies = config.mode != Mode::TeamDeathmatch;
            if (config.mode == Mode::TeamDeathmatch) {
                if (suicide) qualifies = assistant->team != target.team;
                else if (killer && killer->team == target.team) qualifies = assistant->team != killer->team;
                else if (killer) qualifies = assistant->team == killer->team || config.assistance;
            }
            if (qualifies && (!checkedAdd(assistant->total.assists, 1)
                              || !checkedAdd(assistant->total.assistedDamage, entry.second.damage))) {
                failRuntime();
                return false;
            }
        }
        return true;
    }

    void AuthoritativeMatch::evaluateRoundOutcome() {
        if (currentPhase != MatchPhase::ActiveRound) return;
        std::vector<PlayerState *> alive;
        for (auto &player: players) if (player.alive && !player.departed) alive.push_back(&player);
        if (alive.empty()) { establishRoundOutcome({}, Team::None, true); return; }
        if (config.mode == Mode::Deathmatch && alive.size() == 1) {
            establishRoundOutcome({alive.front()->definition.playerId}, Team::None, false);
        } else if (config.mode == Mode::Predator) {
            const bool predatorAlive = std::any_of(alive.begin(), alive.end(), [this](const auto *player) {
                return player->definition.playerId == predatorPlayer;
            });
            if (!predatorAlive) {
                std::vector<Identity> winners;
                for (const auto *player: alive) winners.push_back(player->definition.playerId);
                establishRoundOutcome(std::move(winners), Team::None, false);
            } else if (alive.size() == 1) {
                establishRoundOutcome({predatorPlayer}, Team::None, false);
            }
        } else if (config.mode == Mode::TeamDeathmatch) {
            const Team team = alive.front()->team;
            if (std::all_of(alive.begin(), alive.end(), [team](const auto *player) { return player->team == team; })) {
                std::vector<Identity> winners;
                for (const auto *player: alive) winners.push_back(player->definition.playerId);
                establishRoundOutcome(std::move(winners), team, false);
            }
        }
    }

    void AuthoritativeMatch::establishRoundOutcome(std::vector<Identity> winners, Team winningTeam,
                                                    bool noWinner) {
        if (currentPhase != MatchPhase::ActiveRound) return;
        if (!dependencies.worldSnapshot) {
            for (Identity id: winners) {
                PlayerState *winner = findPlayer(id);
                if (!winner || !checkedAdd(winner->total.wins, 1)) { failRuntime(); return; }
            }
        }
        currentRoundWinners = std::move(winners);
        currentRoundWinningTeam = winningTeam;
        currentRoundNoWinner = noWinner;
        roundEndTicks = 0;
        currentPhase = MatchPhase::RoundEndActive;
    }

    bool AuthoritativeMatch::advanceOneTick() {
        if (terminal.code != OutcomeCode::None) return false;
        if (tick >= MaxMatchTicks) { failRuntime(); return false; }
        bool normalUpdate = currentPhase == MatchPhase::ActiveRound
                            || currentPhase == MatchPhase::RoundEndActive;
        std::unique_ptr<Math::RandomScope> authoritativeScope;
        try {
            if (random) authoritativeScope = std::make_unique<Math::RandomScope>(random.get());
        } catch (...) { failRuntime(); return false; }
        if (normalUpdate) {
            try {
                if (!dependencies.worldTick(tick, true)) { failRuntime(); return false; }
            } catch (...) { failRuntime(); return false; }
            if (dependencies.worldSnapshot && !synchronizeCanonicalWorld()) {
                failRuntime();
                return false;
            }
            for (auto &player: players) {
                if (player.alive && !player.departed && !checkedAdd(player.total.survivalTicks, 1)) {
                    failRuntime();
                    return false;
                }
            }
        }
        ++tick;
        actionsThisTick = 0;
        actionTick = tick;
        if (currentPhase == MatchPhase::RoundEndActive || currentPhase == MatchPhase::RoundEndFrozen) {
            ++roundEndTicks;
            if (roundEndTicks >= RoundEndActiveTicks && currentPhase == MatchPhase::RoundEndActive)
                currentPhase = MatchPhase::RoundEndFrozen;
            if (roundEndTicks >= RoundEndTotalTicks) {
                finishRound();
                if (terminal.code == OutcomeCode::None && completedRoundCount < config.roundLimit && !startRound())
                    failRuntime();
            }
        }
        return terminal.code == OutcomeCode::None;
    }

    bool AuthoritativeMatch::synchronizeCanonicalWorld() {
        CanonicalWorldSnapshot snapshot;
        try { snapshot = dependencies.worldSnapshot(); }
        catch (...) { return false; }
        if (!snapshot.valid || snapshot.stateDigest == 0 || snapshot.players.size() != players.size()
            || snapshot.dynamicEntityCount > 100000u) return false;
        const bool roundEnded = snapshot.roundOver
                                && (!latestCanonicalSnapshot || !latestCanonicalSnapshot->roundOver);
        latestStateDigest = snapshot.stateDigest;
        latestCanonicalSnapshot = snapshot;
        if ((snapshot.worldTick % FixedTickRate == 0 || roundEnded)
            && (checkpoints.empty() || checkpoints.back().tick != snapshot.worldTick)) {
            if (checkpoints.size() >= MaxCanonicalCheckpoints) checkpoints.erase(checkpoints.begin());
            checkpoints.push_back({snapshot.worldTick, snapshot.stateDigest});
        }
        for (const auto &canonical: snapshot.players) {
            PlayerState *player = findPlayer(canonical.playerId);
            if (!player || canonical.life < 0 || canonical.life > MaximumLife) return false;
            player->alive = canonical.alive && !player->departed;
            player->life = canonical.life;
            const auto apply = [&player](std::uint64_t &target, std::uint64_t start, std::uint64_t value) {
                if (value > MaximumCounter || start > MaximumCounter - value) return false;
                target = start + value;
                return true;
            };
            const PlayerStatistics &value = canonical.statistics;
            if (!apply(player->total.shots, player->roundStart.shots, value.shots)
                || !apply(player->total.hits, player->roundStart.hits, value.hits)
                || !apply(player->total.kills, player->roundStart.kills, value.kills)
                || !apply(player->total.deaths, player->roundStart.deaths, value.deaths)
                || !apply(player->total.assists, player->roundStart.assists, value.assists)
                || !apply(player->total.wins, player->roundStart.wins, value.wins)
                || !apply(player->total.penalties, player->roundStart.penalties, value.penalties)
                || !apply(player->total.damage, player->roundStart.damage, value.damage)
                || !apply(player->total.assistedDamage, player->roundStart.assistedDamage, value.assistedDamage))
                return false;
        }
        if (snapshot.roundOver) evaluateRoundOutcome();
        return true;
    }

    bool AuthoritativeMatch::recordRound() {
        if (currentPhase != MatchPhase::RoundEndActive && currentPhase != MatchPhase::RoundEndFrozen) {
            failRuntime();
            return false;
        }
        endWorld();
        RoundResult round;
        round.roundNumber = static_cast<std::uint8_t>(completedRoundCount + 1);
        round.level = currentRoundDecision.level;
        round.mirrored = currentRoundDecision.mirrored;
        round.winnerPlayerIds = currentRoundWinners;
        round.winningTeam = currentRoundWinningTeam;
        round.noWinner = currentRoundNoWinner;
        round.rosterOrder = currentRoundDecision.rosterOrder;
        completedRounds.push_back(std::move(round));
        std::vector<PlayerStatistics> playerRound;
        playerRound.reserve(players.size());
        for (const auto &player: players)
            playerRound.push_back(subtractStatistics(player.total, player.roundStart));
        completedPlayerStatistics.push_back(std::move(playerRound));
        ++completedRoundCount;
        return true;
    }

    void AuthoritativeMatch::finishRound() {
        if (!recordRound()) return;
        if (completedRoundCount >= config.roundLimit) {
            if (!publishResult(ResultState::Completed)) { failRuntime(); return; }
            currentPhase = MatchPhase::Completed;
            terminal = terminalOutcome(OutcomeCode::Completed);
        }
    }

    void AuthoritativeMatch::interruptIfRosterTooSmall() {
        const std::size_t remaining = static_cast<std::size_t>(std::count_if(players.begin(), players.end(),
                [](const auto &player) { return !player.departed; }));
        if (remaining >= 2 || terminal.code != OutcomeCode::None) return;
        if (currentPhase == MatchPhase::RoundEndActive || currentPhase == MatchPhase::RoundEndFrozen) {
            if (!recordRound()) return;
        } else {
            endWorld();
            for (auto &player: players) player.total = player.roundStart;
        }
        if (!publishResult(ResultState::Interrupted)) { failRuntime(); return; }
        currentPhase = MatchPhase::Completed;
        terminal = terminalOutcome(OutcomeCode::InterruptedNoWinner);
    }

    bool AuthoritativeMatch::publishResult(ResultState state) {
        SessionResult candidate;
        candidate.state = state;
        candidate.config = config;
        candidate.completedRounds = completedRoundCount;
        candidate.rounds = completedRounds;
        candidate.finalNoWinner = state == ResultState::Interrupted;
        if (state == ResultState::Completed && !completedRounds.empty()) {
            const RoundResult &finalRound = completedRounds.back();
            candidate.finalWinnerPlayerIds = finalRound.winnerPlayerIds;
            candidate.finalWinningTeam = finalRound.winningTeam;
            candidate.finalNoWinner = finalRound.noWinner;
        }
        candidate.players.reserve(players.size());
        for (std::size_t playerIndex = 0; playerIndex < players.size(); ++playerIndex) {
            const auto &player = players[playerIndex];
            PlayerResultRow row;
            row.playerId = player.definition.playerId;
            row.participantId = player.definition.participantId;
            row.displayName = player.definition.displayName;
            row.team = player.team;
            row.departed = player.departed;
            row.rosterOrder = player.definition.rosterOrder;
            row.statistics = player.total;
            row.rounds.reserve(completedRounds.size());
            for (const auto &round: completedPlayerStatistics) {
                if (playerIndex >= round.size()) return false;
                row.rounds.push_back(round[playerIndex]);
            }
            candidate.players.push_back(std::move(row));
        }
        std::sort(candidate.players.begin(), candidate.players.end(), playerRanking);
        if (config.mode == Mode::TeamDeathmatch) {
            for (std::uint8_t value = 1; value <= config.teamCount; ++value) {
                TeamResultRow team;
                team.team = static_cast<Team>(value);
                for (const auto &player: candidate.players) if (player.team == team.team) {
                    team.totalPoints += player.statistics.totalPoints();
                    team.rankedPlayerIds.push_back(player.playerId);
                }
                candidate.teams.push_back(std::move(team));
            }
            std::stable_sort(candidate.teams.begin(), candidate.teams.end(), [](const auto &left, const auto &right) {
                if (left.totalPoints != right.totalPoints) return left.totalPoints > right.totalPoints;
                return static_cast<std::uint8_t>(left.team) < static_cast<std::uint8_t>(right.team);
            });
        }
        result = std::move(candidate);
        return true;
    }

    TerminalOutcome AuthoritativeMatch::runUntilTerminal(Tick maximumTicks) {
        if (maximumTicks > MaxMatchTicks) maximumTicks = MaxMatchTicks;
        while (terminal.code == OutcomeCode::None && tick < maximumTicks) {
            std::vector<AuthoritativeAction> actions;
            try { actions = dependencies.actionSource(dependencies.clock()); }
            catch (...) { failRuntime(); break; }
            if (actions.size() > MaxActionsPerTick) { failRuntime(); break; }
            for (const auto &action: actions) {
                const ActionResult accepted = submit(action);
                if (accepted == ActionResult::RuntimeFailed || terminal.code != OutcomeCode::None) break;
            }
            if (terminal.code == OutcomeCode::None) advanceOneTick();
        }
        if (terminal.code == OutcomeCode::None && tick >= maximumTicks) failRuntime();
        return terminal;
    }

    void AuthoritativeMatch::failRuntime() {
        if (terminal.code == OutcomeCode::EndedIntentionally) return;
        endWorld();
        result.reset();
        currentPhase = MatchPhase::Failed;
        terminal = terminalOutcome(OutcomeCode::RuntimeFailed);
    }

    void AuthoritativeMatch::endWorld() noexcept {
        if (!worldActive) return;
        try {
            Math::RandomScope authoritativeScope(random.get());
            dependencies.worldEnd();
        } catch (...) {}
        worldActive = false;
    }

    TerminalOutcome AuthoritativeMatch::shutdown() {
        if (cleanupAttempted) return terminal;
        std::unique_ptr<Math::RandomScope> authoritativeScope;
        try {
            if (random) authoritativeScope = std::make_unique<Math::RandomScope>(random.get());
        } catch (...) { failRuntime(); }
        endWorld();
        cleanupAttempted = true;
        bool cleaned = false;
        try { cleaned = dependencies.cleanup(); } catch (...) {}
        released = cleaned;
        if (!cleaned) {
            result.reset();
            currentPhase = MatchPhase::Failed;
            terminal = terminalOutcome(OutcomeCode::ShutdownFailed);
        }
        return terminal;
    }

    MatchPhase AuthoritativeMatch::phase() const noexcept { return currentPhase; }
    Tick AuthoritativeMatch::currentTick() const noexcept { return tick; }
    const MatchConfig &AuthoritativeMatch::frozenConfig() const noexcept { return config; }
    const std::optional<SessionResult> &AuthoritativeMatch::publishedResult() const noexcept { return result; }
    const TerminalOutcome &AuthoritativeMatch::outcome() const noexcept { return terminal; }
    const RoundStartDecision &AuthoritativeMatch::roundDecision() const noexcept { return currentRoundDecision; }
    bool AuthoritativeMatch::resourcesReleased() const noexcept { return released; }
    std::uint64_t AuthoritativeMatch::currentStateDigest() const noexcept { return latestStateDigest; }
    const CanonicalWorldSnapshot *AuthoritativeMatch::canonicalWorldSnapshot() const noexcept {
        return latestCanonicalSnapshot ? &*latestCanonicalSnapshot : nullptr;
    }

    const std::vector<CanonicalStateCheckpoint> &AuthoritativeMatch::stateCheckpoints() const noexcept {
        return checkpoints;
    }
    std::uint64_t AuthoritativeMatch::randomDecisionCount() const noexcept {
        return random ? random->decisionCount() : 0;
    }
    std::uint64_t AuthoritativeMatch::randomDecisionDigest() const noexcept {
        return random ? random->decisionDigest() : 0;
    }
    const std::vector<DeterministicRandom::Decision> &AuthoritativeMatch::randomDecisionTrace() const noexcept {
        static const std::vector<DeterministicRandom::Decision> empty;
        return random ? random->decisionTrace() : empty;
    }
}
