#include "AuthoritativeMatchCli.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#include <unistd.h>
#endif

#include "AuthoritativeMatch.h"
#include "AuthoritativeMatchSerialization.h"
#include "../network/CompatibilityManifest.h"

namespace Duel6::Server::Authoritative {
    namespace {
        struct CliOptions {
            MatchConfig config;
            std::vector<PlayerDefinition> roster;
            std::string resources = "resources";
            std::string scenario = "complete";
            bool actionsFromInput = false;
        };

        bool startsWith(const std::string &value, const char *prefix) {
            return value.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
        }

        std::string after(const std::string &value, const char *prefix) {
            return value.substr(std::char_traits<char>::length(prefix));
        }

        std::uint64_t unsignedValue(const std::string &value) {
            if (value.empty() || value.size() > 20 || value.front() == '+' || value.front() == '-')
                throw std::invalid_argument("invalid unsigned value");
            std::uint64_t result = 0;
            for (const char character: value) {
                if (character < '0' || character > '9') throw std::invalid_argument("invalid unsigned value");
                const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
                if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u)
                    throw std::invalid_argument("invalid unsigned value");
                result = result * 10u + digit;
            }
            return result;
        }

        bool booleanValue(const std::string &value) {
            if (value == "on") return true;
            if (value == "off") return false;
            throw std::invalid_argument("invalid Boolean value");
        }

        bool secureSeed(std::uint64_t &seed) {
            for (int attempt = 0; attempt < 4; ++attempt) {
#ifdef D6R_TRANSPORT_WINDOWS
                if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&seed), sizeof(seed),
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
#else
                std::size_t offset = 0;
                auto *bytes = reinterpret_cast<unsigned char *>(&seed);
                while (offset < sizeof(seed)) {
                    const ssize_t count = getrandom(bytes + offset, sizeof(seed) - offset, 0);
                    if (count > 0) offset += static_cast<std::size_t>(count);
                    else if (count < 0 && errno == EINTR) continue;
                    else return false;
                }
#endif
                if (seed != 0) return true;
            }
            return false;
        }

        CliOptions parse(int argc, char **argv) {
            CliOptions options;
            options.config.enabledWeapons = {"pistol", "machine-gun", "shotgun", "bazooka", "laser",
                                              "plasma", "bow", "sling", "spray", "uzi", "lightning",
                                              "triton", "double-laser", "slime", "stopper-gun", "kiss-of-death"};
            options.config.roundLimit = 1;
            options.config.hostParticipantId = 1;
            for (int index = 1; index < argc; ++index) {
                const std::string argument = argv[index] ? argv[index] : "";
                if (argument == "--authoritative-match") continue;
                if (startsWith(argument, "--resources=")) options.resources = after(argument, "--resources=");
                else if (startsWith(argument, "--match-mode=")) {
                    const std::string value = after(argument, "--match-mode=");
                    if (value == "deathmatch") options.config.mode = Mode::Deathmatch;
                    else if (value == "predator") options.config.mode = Mode::Predator;
                    else if (value == "team-deathmatch") options.config.mode = Mode::TeamDeathmatch;
                    else throw std::invalid_argument("invalid mode");
                } else if (startsWith(argument, "--teams=")) {
                    const auto value = unsignedValue(after(argument, "--teams="));
                    if (value > 255) throw std::invalid_argument("invalid teams");
                    options.config.teamCount = static_cast<std::uint8_t>(value);
                } else if (startsWith(argument, "--friendly-fire="))
                    options.config.friendlyFire = booleanValue(after(argument, "--friendly-fire="));
                else if (startsWith(argument, "--level-plan=")) {
                    const std::string value = after(argument, "--level-plan=");
                    if (value == "fixed") options.config.levelPlan = LevelPlan::Fixed;
                    else if (value == "shuffle") options.config.levelPlan = LevelPlan::ShuffleAll;
                    else if (value == "random") options.config.levelPlan = LevelPlan::Random;
                    else throw std::invalid_argument("invalid level plan");
                } else if (startsWith(argument, "--level="))
                    options.config.playableLevels.push_back(after(argument, "--level="));
                else if (startsWith(argument, "--fixed-level="))
                    options.config.fixedLevel = after(argument, "--fixed-level=");
                else if (startsWith(argument, "--rounds=")) {
                    const auto value = unsignedValue(after(argument, "--rounds="));
                    if (value > 255) throw std::invalid_argument("invalid rounds");
                    options.config.roundLimit = static_cast<std::uint8_t>(value);
                } else if (startsWith(argument, "--assistance="))
                    options.config.assistance = booleanValue(after(argument, "--assistance="));
                else if (startsWith(argument, "--quick-liquid="))
                    options.config.quickLiquid = booleanValue(after(argument, "--quick-liquid="));
                else if (startsWith(argument, "--burnable-trees="))
                    options.config.burnableTrees = booleanValue(after(argument, "--burnable-trees="));
                else if (startsWith(argument, "--seed=")) options.config.seed = unsignedValue(after(argument, "--seed="));
                else if (startsWith(argument, "--host-participant="))
                    options.config.hostParticipantId = unsignedValue(after(argument, "--host-participant="));
                else if (startsWith(argument, "--player=")) {
                    std::string value = after(argument, "--player=");
                    const std::size_t first = value.find(',');
                    const std::size_t second = first == std::string::npos ? first : value.find(',', first + 1);
                    if (first == std::string::npos || second == std::string::npos)
                        throw std::invalid_argument("invalid player");
                    PlayerDefinition player;
                    player.participantId = unsignedValue(value.substr(0, first));
                    player.playerId = unsignedValue(value.substr(first + 1, second - first - 1));
                    player.displayName = value.substr(second + 1);
                    if (options.roster.size() >= MaxPlayers) throw std::invalid_argument("too many players");
                    player.rosterOrder = static_cast<std::uint8_t>(options.roster.size());
                    options.roster.push_back(std::move(player));
                } else if (startsWith(argument, "--scenario=")) options.scenario = after(argument, "--scenario=");
                else if (argument == "--actions-stdin") options.actionsFromInput = true;
                else throw std::invalid_argument("unsupported authoritative match argument");
            }
            if (options.roster.empty()) {
                options.roster = {{1, 101, "Host player", 0}, {2, 201, "Guest player", 1}};
            }
            return options;
        }

        void printOutcome(std::ostream &output, const TerminalOutcome &outcome,
                          const std::optional<SessionResult> &result) {
            output << outcome.identifier << '\n' << outcome.copy << '\n';
            if (result) {
                const auto serialized = serializeSessionResult(*result);
                if (serialized) output << "session-result=" << *serialized << '\n';
            }
            output.flush();
        }

        std::vector<AuthoritativeAction> readActions(std::istream &input) {
            std::vector<AuthoritativeAction> actions;
            std::string line;
            while (std::getline(input, line)) {
                if (line.empty()) continue;
                if (line.size() > 512 || actions.size() >= MaxActions) throw std::invalid_argument("action limit");
                std::istringstream parser(line);
                std::string kind;
                AuthoritativeAction action;
                if (!(parser >> action.tick >> action.sequence >> action.participantId >> action.playerId
                      >> kind >> action.targetPlayerId >> action.amount >> action.inputMask))
                    throw std::invalid_argument("invalid action");
                std::string trailing;
                if (parser >> trailing) throw std::invalid_argument("invalid action");
                if (kind == "input") action.kind = ActionKind::PlayerInput;
                else if (kind == "shot") action.kind = ActionKind::ShotDamage;
                else if (kind == "environment") action.kind = ActionKind::EnvironmentalDamage;
                else if (kind == "remove") action.kind = ActionKind::RemovePlayer;
                else if (kind == "advance") action.kind = ActionKind::AdvanceRound;
                else if (kind == "end") action.kind = ActionKind::EndSession;
                else if (kind == "fail") action.kind = ActionKind::RuntimeFailure;
                else throw std::invalid_argument("invalid action kind");
                actions.push_back(action);
            }
            if (!std::is_sorted(actions.begin(), actions.end(), [](const auto &left, const auto &right) {
                return left.tick < right.tick || (left.tick == right.tick && left.sequence < right.sequence);
            })) throw std::invalid_argument("unordered actions");
            return actions;
        }

        bool submitChecked(AuthoritativeMatch &match, const AuthoritativeAction &action) {
            return match.submit(action) == ActionResult::Accepted;
        }

        void driveCompleteScenario(AuthoritativeMatch &match, const MatchConfig &config,
                                   const std::vector<PlayerDefinition> &roster,
                                   const std::function<bool()> &stopped) {
            std::uint64_t sequence = 1;
            while (match.outcome().code == OutcomeCode::None) {
                if (stopped && stopped()) {
                    submitChecked(match, {match.currentTick(), sequence++, config.hostParticipantId, 0,
                                          ActionKind::EndSession, 0, 0, 0});
                    break;
                }
                if (match.phase() == MatchPhase::ActiveRound) {
                    Identity shooterId = roster.front().playerId;
                    Identity shooterParticipant = roster.front().participantId;
                    const Identity predator = match.roundDecision().predatorPlayerId;
                    if (config.mode == Mode::Predator && shooterId == predator && roster.size() > 1) {
                        shooterId = roster[1].playerId;
                        shooterParticipant = roster[1].participantId;
                    }
                    Team shooterTeam = config.mode == Mode::TeamDeathmatch
                                       ? static_cast<Team>(roster[shooterId == roster.front().playerId ? 0 : 1].rosterOrder
                                                           % config.teamCount + 1) : Team::None;
                    for (const auto &target: roster) {
                        if (target.playerId == shooterId) continue;
                        if (config.mode == Mode::Predator && target.playerId != predator) continue;
                        if (config.mode == Mode::TeamDeathmatch) {
                            const Team targetTeam = static_cast<Team>(target.rosterOrder % config.teamCount + 1);
                            if (targetTeam == shooterTeam) continue;
                        }
                        const int hits = config.mode == Mode::Predator && target.playerId == predator ? 4 : 1;
                        for (int hit = 0; hit < hits; ++hit) {
                            if (!submitChecked(match, {match.currentTick(), sequence++, shooterParticipant, shooterId,
                                                       ActionKind::ShotDamage, target.playerId, 0, MaximumLife}))
                                return;
                        }
                    }
                }
                if (match.outcome().code == OutcomeCode::None) match.advanceOneTick();
            }
        }
    }

    bool authoritativeMatchRequested(int argc, char **argv) {
        for (int index = 1; index < argc; ++index)
            if (argv[index] && std::string(argv[index]) == "--authoritative-match") return true;
        return false;
    }

    int runAuthoritativeMatchCli(int argc, char **argv, std::istream &input, std::ostream &output,
                                 AuthoritativeMatchCliDependencies cliDependencies) {
        try {
            CliOptions options = parse(argc, argv);
            const Network::ManifestBuildResult built = Network::CompatibilityManifestBuilder(options.resources, {}).build();
            if (!built.valid()) {
                output << "host-gameplay-content-manifest-invalid\n"
                       << "Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.\n";
                return 2;
            }
            if (options.config.playableLevels.empty()) {
                for (const auto &entry: built.manifest)
                    if (startsWith(entry.logicalPath, "levels/")
                        && entry.logicalPath.size() > 5
                        && entry.logicalPath.compare(entry.logicalPath.size() - 5, 5, ".json") == 0)
                        options.config.playableLevels.push_back(entry.logicalPath);
            }
            if (options.config.levelPlan == LevelPlan::Fixed && options.config.fixedLevel.empty()
                && !options.config.playableLevels.empty())
                options.config.fixedLevel = options.config.playableLevels.front();
            if (options.scenario == "invalid") options.config.roundLimit = 0;
            if (options.config.seed == 0 && !secureSeed(options.config.seed)) {
                const auto failed = terminalOutcome(OutcomeCode::RuntimeFailed);
                printOutcome(output, failed, std::nullopt);
                return failed.exitStatus;
            }

            MatchRuntimeDependencies runtime;
            if (options.scenario == "cleanup-failure") runtime.cleanup = [] { return false; };
            AuthoritativeMatch match(std::move(runtime));
            Network::GameplayManifest manifest = built.manifest;
            if (options.scenario == "content-unavailable" && !manifest.empty()) manifest.pop_back();
            TerminalOutcome startup = match.start(options.config, options.roster, manifest);
            if (startup.code != OutcomeCode::None) {
                printOutcome(output, startup, std::nullopt);
                return startup.exitStatus;
            }
            if (cliDependencies.reportReady && !cliDependencies.reportReady()) {
                submitChecked(match, {match.currentTick(), 1, options.config.hostParticipantId, 0,
                                      ActionKind::RuntimeFailure, 0, 0, 0});
            } else if (options.actionsFromInput) {
                const auto actions = readActions(input);
                std::size_t next = 0;
                while (match.outcome().code == OutcomeCode::None && next < actions.size()) {
                    while (match.currentTick() < actions[next].tick && match.outcome().code == OutcomeCode::None)
                        match.advanceOneTick();
                    while (next < actions.size() && actions[next].tick == match.currentTick()) {
                        if (!submitChecked(match, actions[next++])) break;
                    }
                }
                while (match.outcome().code == OutcomeCode::None) match.advanceOneTick();
            } else if (options.scenario == "interrupted") {
                submitChecked(match, {0, 1, options.config.hostParticipantId, 0,
                                      ActionKind::RemovePlayer, options.roster.back().playerId, 0, 0});
            } else if (options.scenario == "runtime-failure") {
                submitChecked(match, {0, 1, options.config.hostParticipantId, 0,
                                      ActionKind::RuntimeFailure, 0, 0, 0});
            } else if (options.scenario == "complete" || options.scenario == "cleanup-failure") {
                driveCompleteScenario(match, options.config, options.roster, cliDependencies.stopRequested);
            } else {
                const auto invalid = terminalOutcome(OutcomeCode::SettingsInvalid);
                printOutcome(output, invalid, std::nullopt);
                return invalid.exitStatus;
            }
            TerminalOutcome final = match.shutdown();
            if (final.code == OutcomeCode::None) final = terminalOutcome(OutcomeCode::RuntimeFailed);
            printOutcome(output, final, match.publishedResult());
            return final.exitStatus;
        } catch (...) {
            const auto invalid = terminalOutcome(OutcomeCode::SettingsInvalid);
            printOutcome(output, invalid, std::nullopt);
            return invalid.exitStatus;
        }
    }
}
