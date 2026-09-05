#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "source/network/PlayerInputProtocol.h"
#include "source/network/NetworkTrustPolicy.h"
#include "source/server/AuthoritativePlayerInput.h"
#include "tests/TestHarness.h"

namespace {
using namespace Duel6::Server::Authoritative;
namespace Input = Duel6::Network::Input;

struct InputCall {
    Identity player = 0;
    std::uint32_t actions = 0;
};

std::vector<PlayerDefinition> makeRoster(std::size_t count, bool oneOwner = false) {
    std::vector<PlayerDefinition> result;
    for (std::size_t index = 0; index < count; ++index)
        result.push_back({oneOwner ? Identity{1} : Identity{index + 1}, Identity{101 + index},
                          "Player " + std::to_string(index + 1), static_cast<std::uint8_t>(index)});
    return result;
}

Duel6::Network::GameplayManifest makeManifest() {
    return {{"data/blocks.json", {}}, {"data/config.script", {}}, {"levels/a.json", {}}};
}

MatchConfig makeConfig(std::uint8_t rounds = 1) {
    MatchConfig result;
    result.seed = 1234;
    result.hostParticipantId = 1;
    result.fixedLevel = "levels/a.json";
    result.playableLevels = {"levels/a.json"};
    result.enabledWeapons = {"pistol"};
    result.roundLimit = rounds;
    return result;
}

struct Fixture {
    AuthoritativePlayerInput::TimePoint now = AuthoritativePlayerInput::TimePoint{} + std::chrono::seconds(10);
    std::vector<InputCall> inputCalls;
    std::map<Identity, std::uint32_t> held;
    std::vector<std::string> events;
    MatchRuntimeDependencies dependencies;
    AuthoritativeMatch match;
    AuthoritativePlayerInput input;

    explicit Fixture(const std::vector<PlayerDefinition> &roster, std::uint8_t rounds = 1)
            : dependencies(makeDependencies()), match(dependencies),
              input(1, [this] { return now; }) {
        D6R_REQUIRE_EQ(OutcomeCode::None, match.start(makeConfig(rounds), roster, makeManifest()).code);
        D6R_REQUIRE(input.beginMatch(match, roster));
        // Match startup intentionally initializes each canonical input to zero. Tests below
        // measure only calls caused by the scenario under test.
        inputCalls.clear();
        held.clear();
        events.clear();
    }

    MatchRuntimeDependencies makeDependencies() {
        MatchRuntimeDependencies result;
        result.seedSource = [] { return UINT64_C(1234); };
        result.worldStart = [](RoundStartDecision &) { return true; };
        result.worldTick = [this](Tick, bool) {
            events.push_back("tick");
            return true;
        };
        result.worldInput = [this](Identity player, std::uint32_t actions) {
            inputCalls.push_back({player, actions});
            held[player] = actions;
            events.push_back("input:" + std::to_string(player) + ":" + std::to_string(actions));
            return true;
        };
        result.worldRemove = [](Identity) { return true; };
        result.worldEnd = [] {};
        result.cleanup = [] { return true; };
        return result;
    }
};

struct OutcomeSink {
    std::vector<Input::Outcome> outcomes;
    std::vector<std::string> *events = nullptr;

    Duel6::Network::SendResult send(std::vector<std::uint8_t> payload) {
        const auto frame = Input::deserializeFrame(payload);
        D6R_REQUIRE(frame.has_value() && frame->outcome.has_value());
        outcomes.push_back(*frame->outcome);
        if (events) events->push_back("outcome:" + std::to_string(static_cast<unsigned>(frame->outcome->category)));
        return Duel6::Network::SendResult::Accepted;
    }

    std::size_t count(Input::OutcomeCategory category) const {
        return static_cast<std::size_t>(std::count_if(outcomes.begin(), outcomes.end(), [category](const auto &value) {
            return value.category == category;
        }));
    }
};

struct FrameSink {
    std::vector<Input::Frame> frames;

    Duel6::Network::SendResult send(std::vector<std::uint8_t> payload) {
        const auto frame = Input::deserializeFrame(payload);
        D6R_REQUIRE(frame.has_value());
        frames.push_back(*frame);
        return Duel6::Network::SendResult::Accepted;
    }

    std::size_t count(Input::FrameKind kind) const {
        return static_cast<std::size_t>(std::count_if(frames.begin(), frames.end(), [kind](const auto &value) {
            return value.kind == kind;
        }));
    }
};

Input::Command command(Identity participant, Identity player, std::uint64_t sequence,
                       Tick target, std::uint32_t actions) {
    return {participant, player, sequence, target, actions};
}

AuthoritativeAction matchAction(const AuthoritativeMatch &match, std::uint64_t sequence,
                                Identity participant, Identity player, ActionKind kind,
                                Identity target = 0, std::int32_t amount = 0) {
    return {match.currentTick(), sequence, participant, player, kind, target, 0, amount};
}
}

D6R_TEST_CASE("NIN complete seven-action commands serialize and owned-player sequences are independent") {
    D6R_REQUIRE_EQ(60u, FixedTickRate);
    Input::PlayerActionState state;
    state.moveLeft = state.moveRight = state.jump = state.crouch = true;
    state.shoot = state.pickOrSwapWeapon = state.showStatus = true;
    D6R_REQUIRE_EQ(Input::AllActions, state.mask());

    Input::OwnedPlayerCommandSource source(20, {201, 202});
    const auto first = source.sample(201, 7, state);
    const auto secondPlayer = source.sample(202, 8, 0);
    const auto gap = source.sample(201, 9, Input::Jump);
    D6R_REQUIRE(first && secondPlayer && gap);
    D6R_REQUIRE_EQ(UINT64_C(1), first->sequence);
    D6R_REQUIRE_EQ(UINT64_C(1), secondPlayer->sequence);
    D6R_REQUIRE_EQ(UINT64_C(2), gap->sequence);
    D6R_REQUIRE_EQ(Input::AllActions, first->actions);
    D6R_REQUIRE_EQ(0u, secondPlayer->actions);
    D6R_REQUIRE(!source.sample(999, 9, Input::Shoot));
    D6R_REQUIRE(!source.sample(201, 9, Input::AllActions | (1u << 15u)));

    const auto encoded = Input::serializeCommand(*first);
    const auto decoded = Input::deserializeFrame(encoded);
    D6R_REQUIRE(decoded && decoded->kind == Input::FrameKind::Command && decoded->command);
    D6R_REQUIRE_EQ(first->participantId, decoded->command->participantId);
    D6R_REQUIRE_EQ(first->playerId, decoded->command->playerId);
    D6R_REQUIRE_EQ(first->sequence, decoded->command->sequence);
    D6R_REQUIRE_EQ(first->targetTick, decoded->command->targetTick);
    D6R_REQUIRE_EQ(first->actions, decoded->command->actions);
    auto truncated = encoded;
    truncated.pop_back();
    D6R_REQUIRE(!Input::deserializeFrame(truncated));
    auto trailing = encoded;
    trailing.push_back(0);
    D6R_REQUIRE(!Input::deserializeFrame(trailing));
}

D6R_TEST_CASE("NIN host and guest command sessions dispatch independent local input and handle outcomes") {
    std::vector<Input::Command> hostDispatches;
    std::vector<Input::Command> guestDispatches;
    Input::ClientCommandSession host(1, {101, 102}, [&](std::vector<std::uint8_t> payload) {
        const auto frame = Input::deserializeFrame(payload);
        D6R_REQUIRE(frame && frame->kind == Input::FrameKind::Command && frame->command);
        hostDispatches.push_back(*frame->command);
        return Duel6::Network::SendResult::Accepted;
    });
    Input::ClientCommandSession guest(2, {201}, [&](std::vector<std::uint8_t> payload) {
        const auto frame = Input::deserializeFrame(payload);
        D6R_REQUIRE(frame && frame->kind == Input::FrameKind::Command && frame->command);
        guestDispatches.push_back(*frame->command);
        return Duel6::Network::SendResult::Accepted;
    });

    const auto hostA = host.submit(101, 8, Input::MoveLeft | Input::Shoot);
    const auto hostB = host.submit(102, 8, Input::Jump | Input::ShowStatus);
    const auto guestA = guest.submit(201, 8, Input::MoveRight | Input::PickOrSwapWeapon);
    D6R_REQUIRE(hostA && hostB && guestA);
    D6R_REQUIRE_EQ(std::size_t{2}, hostDispatches.size());
    D6R_REQUIRE_EQ(std::size_t{1}, guestDispatches.size());
    D6R_REQUIRE_EQ(Identity{1}, hostDispatches[0].participantId);
    D6R_REQUIRE_EQ(Identity{2}, guestDispatches[0].participantId);
    D6R_REQUIRE_EQ(UINT64_C(1), hostDispatches[0].sequence);
    D6R_REQUIRE_EQ(UINT64_C(1), hostDispatches[1].sequence);
    D6R_REQUIRE_EQ(UINT64_C(1), guestDispatches[0].sequence);
    D6R_REQUIRE(!host.submit(201, 8, Input::Crouch));
    D6R_REQUIRE(!guest.submit(101, 8, Input::Crouch));

    D6R_REQUIRE(host.receive(Input::serializeOutcome(
            {Input::OutcomeCategory::Pending, 101, hostA->sequence, 8})));
    D6R_REQUIRE(host.state(101, hostA->sequence) == Input::ClientCommandState::Pending);
    D6R_REQUIRE(host.receive(Input::serializeOutcome(
            {Input::OutcomeCategory::Applied, 101, hostA->sequence, 8})));
    D6R_REQUIRE(host.state(101, hostA->sequence) == Input::ClientCommandState::Applied);
    D6R_REQUIRE(guest.receive(Input::serializeOutcome(
            {Input::OutcomeCategory::Pending, 201, guestA->sequence, 8})));
    D6R_REQUIRE(guest.receive(Input::serializeOutcome(
            {Input::OutcomeCategory::OverLimit, 201, guestA->sequence, 0})));
    D6R_REQUIRE(guest.state(201, guestA->sequence) == Input::ClientCommandState::Rejected);
    D6R_REQUIRE(guest.receive(Input::serializeSessionPolicyViolation()));
    D6R_REQUIRE(guest.policyViolation());
    D6R_REQUIRE(!host.policyViolation());
}

D6R_TEST_CASE("NIN client command history is bounded and reset starts a clean match lifecycle") {
    std::vector<Input::Command> dispatches;
    Input::ClientCommandSession session(1, {101}, [&](std::vector<std::uint8_t> payload) {
        const auto frame = Input::deserializeFrame(payload);
        D6R_REQUIRE(frame && frame->command);
        dispatches.push_back(*frame->command);
        return Duel6::Network::SendResult::Accepted;
    });

    for (std::size_t index = 0; index < Input::MaxClientCommandHistory; ++index)
        D6R_REQUIRE(session.submit(101, index, Input::MoveLeft));
    D6R_REQUIRE_EQ(Input::MaxClientCommandHistory, dispatches.size());
    D6R_REQUIRE(!session.submit(101, Input::MaxClientCommandHistory, Input::MoveRight));

    for (const auto &submitted: dispatches)
        D6R_REQUIRE(session.receive(Input::serializeOutcome(
                {Input::OutcomeCategory::Pending, 101, submitted.sequence, submitted.targetTick})));
    D6R_REQUIRE(!session.submit(101, Input::MaxClientCommandHistory, Input::MoveRight));
    D6R_REQUIRE(session.receive(Input::serializeOutcome(
            {Input::OutcomeCategory::Applied, 101, dispatches.front().sequence,
             dispatches.front().targetTick})));

    const auto replacement = session.submit(101, Input::MaxClientCommandHistory, Input::MoveRight);
    D6R_REQUIRE(replacement);
    D6R_REQUIRE_EQ(UINT64_C(257), replacement->sequence);
    D6R_REQUIRE(!session.state(101, 1));
    D6R_REQUIRE(session.state(101, 2) == Input::ClientCommandState::Pending);
    D6R_REQUIRE(session.state(101, replacement->sequence) == Input::ClientCommandState::Submitted);

    D6R_REQUIRE(session.receive(Input::serializeSessionPolicyViolation()));
    D6R_REQUIRE(session.policyViolation());
    session.reset();
    D6R_REQUIRE(!session.policyViolation());
    D6R_REQUIRE(!session.lastOutcome());
    D6R_REQUIRE(!session.state(101, replacement->sequence));
    const auto nextMatch = session.submit(101, 0, Input::Jump);
    D6R_REQUIRE(nextMatch);
    D6R_REQUIRE_EQ(UINT64_C(1), nextMatch->sequence);
}

D6R_TEST_CASE("NIN host and guest clients sustain four owned players at 60 Hz through an authoritative round") {
    const std::vector<PlayerDefinition> roster = {
            {1, 101, "Host A", 0}, {1, 102, "Host B", 1},
            {1, 103, "Host C", 2}, {1, 104, "Host D", 3},
            {2, 201, "Guest A", 4}, {2, 202, "Guest B", 5},
            {2, 203, "Guest C", 6}, {2, 204, "Guest D", 7}};
    Fixture fixture(roster);
    std::vector<Input::OutcomeCategory> receiveResults;
    std::unique_ptr<Input::ClientCommandSession> host;
    std::unique_ptr<Input::ClientCommandSession> guest;
    host = std::make_unique<Input::ClientCommandSession>(1, std::vector<Identity>{101, 102, 103, 104},
            [&](std::vector<std::uint8_t> payload) {
                const auto frame = Input::deserializeFrame(payload);
                D6R_REQUIRE(frame && frame->command);
                const auto result = fixture.input.receive(1, *frame->command, false);
                receiveResults.push_back(result.category);
                return Duel6::Network::SendResult::Accepted;
            });
    guest = std::make_unique<Input::ClientCommandSession>(2, std::vector<Identity>{201, 202, 203, 204},
            [&](std::vector<std::uint8_t> payload) {
                const auto frame = Input::deserializeFrame(payload);
                D6R_REQUIRE(frame && frame->command);
                const auto result = fixture.input.receive(2, *frame->command);
                receiveResults.push_back(result.category);
                return Duel6::Network::SendResult::Accepted;
            });
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return host->receive(payload)
            ? Duel6::Network::SendResult::Accepted : Duel6::Network::SendResult::NotConnected; }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest->receive(payload)
            ? Duel6::Network::SendResult::Accepted : Duel6::Network::SendResult::NotConnected; }));

    constexpr std::size_t SustainedTicks = 180;
    const std::vector<Identity> hostPlayers = {101, 102, 103, 104};
    const std::vector<Identity> guestPlayers = {201, 202, 203, 204};
    for (std::size_t tick = 0; tick < SustainedTicks; ++tick) {
        for (std::size_t index = 0; index < hostPlayers.size(); ++index)
            D6R_REQUIRE(host->submit(hostPlayers[index], fixture.match.currentTick(),
                    1u << ((tick + index) % 7u)));
        for (std::size_t index = 0; index < guestPlayers.size(); ++index)
            D6R_REQUIRE(guest->submit(guestPlayers[index], fixture.match.currentTick(),
                    1u << ((tick + index + 3u) % 7u)));
        D6R_REQUIRE(fixture.input.processTick());
        fixture.now += std::chrono::nanoseconds(1000000000 / FixedTickRate);
    }

    D6R_REQUIRE_EQ(SustainedTicks * roster.size(), fixture.inputCalls.size());
    D6R_REQUIRE_EQ(SustainedTicks * roster.size(), receiveResults.size());
    D6R_REQUIRE(std::all_of(receiveResults.begin(), receiveResults.end(), [](const auto category) {
        return category == Input::OutcomeCategory::Pending;
    }));
    for (Identity player: hostPlayers)
        D6R_REQUIRE(host->state(player, SustainedTicks) == Input::ClientCommandState::Applied);
    for (Identity player: guestPlayers)
        D6R_REQUIRE(guest->state(player, SustainedTicks) == Input::ClientCommandState::Applied);
    D6R_REQUIRE(!host->state(101, 1));
    D6R_REQUIRE(!guest->state(201, 1));

    std::uint64_t sequence = SustainedTicks + 1;
    for (Identity target: {Identity{102}, Identity{103}, Identity{104}, Identity{201},
                           Identity{202}, Identity{203}, Identity{204}})
        D6R_REQUIRE_EQ(ActionResult::Accepted, fixture.match.submit(matchAction(
                fixture.match, sequence++, 1, 101, ActionKind::ShotDamage, target, MaximumLife)));
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::RoundEndActive);
    for (std::uint32_t tick = 0; tick < RoundEndTotalTicks; ++tick)
        D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::Completed);
}

D6R_TEST_CASE("NIN protocol rejects malformed wrong-kind and invalid identity sequence or action commands") {
    const auto valid = Input::serializeCommand(command(2, 201, 1, 7, Input::MoveLeft));
    for (std::size_t size = 0; size < valid.size(); ++size) {
        const std::vector<std::uint8_t> truncated(valid.begin(), valid.begin() + size);
        D6R_REQUIRE(!Input::deserializeFrame(truncated));
    }
    auto wrongKind = valid;
    wrongKind[6] = 99;
    wrongKind[7] = 0;
    D6R_REQUIRE(Input::isPlayerInputFrame(wrongKind));
    D6R_REQUIRE(!Input::deserializeFrame(wrongKind));

    for (const auto invalid: {
            command(0, 201, 1, 7, Input::MoveLeft),
            command(2, 0, 1, 7, Input::MoveLeft),
            command(2, 201, 0, 7, Input::MoveLeft),
            command(2, 201, 1, 7, Input::AllActions | (1u << 7u))}) {
        bool rejected = false;
        try { (void) Input::serializeCommand(invalid); }
        catch (const std::invalid_argument &) { rejected = true; }
        D6R_REQUIRE(rejected);
    }
    const auto zeroActions = Input::serializeCommand(command(2, 201, 1, 7, 0));
    const auto decoded = Input::deserializeFrame(zeroActions);
    D6R_REQUIRE(decoded && decoded->command && decoded->command->actions == 0);
}

D6R_TEST_CASE("NIN host guest and multi-player ownership remain isolated") {
    const std::vector<PlayerDefinition> roster = {
            {1, 101, "Host A", 0}, {1, 102, "Host B", 1},
            {2, 201, "Guest A", 2}, {2, 202, "Guest B", 3}};
    Fixture fixture(roster);
    OutcomeSink host, guest, other;
    bool guestClosed = false;
    bool otherClosed = false;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return host.send(std::move(payload)); }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest.send(std::move(payload)); },
                                      [&] { guestClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(3, [&](auto payload) { return other.send(std::move(payload)); },
                                      [&] { otherClosed = true; }));

    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 1, 0, Input::MoveLeft), false).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 102, 1, 0, Input::Jump), false).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 201, 1, 0, Input::MoveRight)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 202, 1, 0, Input::Crouch)).category
                == Input::OutcomeCategory::Pending);
    const auto denied = fixture.input.receive(3, command(3, 201, 1, 0, Input::Shoot));
    D6R_REQUIRE(denied.category == Input::OutcomeCategory::Unauthorized && denied.closeConnection);
    D6R_REQUIRE(otherClosed);
    D6R_REQUIRE(!guestClosed);
    D6R_REQUIRE(fixture.inputCalls.empty());

    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(std::size_t{4}, fixture.inputCalls.size());
    D6R_REQUIRE_EQ(Input::MoveLeft, fixture.held[101]);
    D6R_REQUIRE_EQ(Input::Jump, fixture.held[102]);
    D6R_REQUIRE_EQ(Input::MoveRight, fixture.held[201]);
    D6R_REQUIRE_EQ(Input::Crouch, fixture.held[202]);
    D6R_REQUIRE_EQ(std::size_t{2}, host.count(Input::OutcomeCategory::Applied));
    D6R_REQUIRE_EQ(std::size_t{2}, guest.count(Input::OutcomeCategory::Applied));

    std::uint64_t sequence = 2; // Player 101 already applied network-input sequence 1.
    for (Identity target: {Identity{201}, Identity{202}, Identity{102}})
        D6R_REQUIRE_EQ(ActionResult::Accepted, fixture.match.submit(matchAction(
                fixture.match, sequence++, 1, 101, ActionKind::ShotDamage, target, MaximumLife)));
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::RoundEndActive);
    for (std::uint32_t tick = 0; tick < RoundEndTotalTicks; ++tick) D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::Completed);
    D6R_REQUIRE_EQ(std::size_t{4}, fixture.inputCalls.size());
}

D6R_TEST_CASE("NIN unauthorized input sends exact policy outcome and closes only its offender") {
    Fixture fixture(makeRoster(3));
    FrameSink owner, offender, peer;
    bool ownerClosed = false;
    bool offenderClosed = false;
    bool peerClosed = false;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return owner.send(std::move(payload)); },
                                      [&] { ownerClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return offender.send(std::move(payload)); },
                                      [&] { offenderClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(3, [&](auto payload) { return peer.send(std::move(payload)); },
                                      [&] { peerClosed = true; }));
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 1, 0, Input::MoveLeft)).category
                == Input::OutcomeCategory::Pending);

    const auto denied = fixture.input.receive(2, command(2, 101, 1, 0, Input::Shoot));
    D6R_REQUIRE(denied.category == Input::OutcomeCategory::Unauthorized && denied.closeConnection);
    D6R_REQUIRE(offenderClosed);
    D6R_REQUIRE(!ownerClosed && !peerClosed);
    D6R_REQUIRE_EQ(std::size_t{1}, offender.count(Input::FrameKind::SessionPolicyViolation));
    D6R_REQUIRE_EQ(std::string("session-policy-violation"), Duel6::Network::Trust::outcomeCode(
            Duel6::Network::Trust::AdmissionOutcome::SessionPolicyViolation));
    D6R_REQUIRE_EQ(std::string("Connection ended."), Duel6::Network::Trust::outcomeUserCopy(
            Duel6::Network::Trust::AdmissionOutcome::SessionPolicyViolation));
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(Input::MoveLeft, fixture.held[101]);
    D6R_REQUIRE_EQ(std::size_t{1}, owner.count(Input::FrameKind::AppliedAcknowledgment));
}

D6R_TEST_CASE("NIN target tick boundaries effective ticks and post-processing acknowledgments are exact") {
    Fixture fixture(makeRoster(4));
    for (int tick = 0; tick < 3; ++tick) D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(Tick{3}, fixture.match.currentTick());
    OutcomeSink sinks[4];
    for (std::size_t index = 0; index < 4; ++index) {
        sinks[index].events = &fixture.events;
        const Identity participant = index + 1;
        D6R_REQUIRE(fixture.input.restore(participant, [&, index](auto payload) {
            return sinks[index].send(std::move(payload));
        }));
    }
    fixture.events.clear();
    for (std::size_t index = 0; index < 4; ++index) {
        const Tick target = 1 + index; // N-2, N-1, N, N+1
        const auto result = fixture.input.receive(index + 1, command(index + 1, 101 + index, 1,
                target, Input::MoveLeft << index));
        D6R_REQUIRE(result.category == Input::OutcomeCategory::Pending);
        D6R_REQUIRE_EQ(target <= 3 ? Tick{3} : Tick{4}, sinks[index].outcomes.back().effectiveTick);
        D6R_REQUIRE_EQ(std::size_t{0}, sinks[index].count(Input::OutcomeCategory::Applied));
    }
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 2, 0, Input::Shoot)).category
                == Input::OutcomeCategory::Stale);
    D6R_REQUIRE(fixture.input.receive(4, command(4, 104, 2, 5, Input::Shoot)).category
                == Input::OutcomeCategory::TooFuture);
    D6R_REQUIRE(fixture.inputCalls.empty());

    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(std::size_t{3}, fixture.inputCalls.size());
    D6R_REQUIRE_EQ(std::size_t{1}, sinks[0].count(Input::OutcomeCategory::Applied));
    D6R_REQUIRE_EQ(std::size_t{0}, sinks[3].count(Input::OutcomeCategory::Applied));
    const auto tickEvent = std::find(fixture.events.begin(), fixture.events.end(), "tick");
    const auto ackEvent = std::find(fixture.events.begin(), fixture.events.end(),
                                    "outcome:" + std::to_string(static_cast<unsigned>(Input::OutcomeCategory::Applied)));
    D6R_REQUIRE(tickEvent != fixture.events.end() && ackEvent != fixture.events.end() && tickEvent < ackEvent);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(std::size_t{4}, fixture.inputCalls.size());
    D6R_REQUIRE_EQ(std::size_t{1}, sinks[3].count(Input::OutcomeCategory::Applied));
    D6R_REQUIRE_EQ(Tick{4}, sinks[3].outcomes.back().effectiveTick);
}

D6R_TEST_CASE("NIN supersession sequence rejection retention and zero-state release are deterministic") {
    Fixture fixture(makeRoster(2));
    OutcomeSink sink;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return sink.send(std::move(payload)); }));
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 1, 0, Input::MoveLeft)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 3, 0, Input::Jump)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE_EQ(std::size_t{1}, sink.count(Input::OutcomeCategory::Superseded));
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 2, 0, Input::Shoot)).category
                == Input::OutcomeCategory::Duplicate);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 3, 0, Input::Shoot)).category
                == Input::OutcomeCategory::Duplicate);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(std::size_t{1}, fixture.inputCalls.size());
    D6R_REQUIRE_EQ(Input::Jump, fixture.held[101]);
    D6R_REQUIRE_EQ(std::size_t{1}, sink.count(Input::OutcomeCategory::Applied));
    D6R_REQUIRE_EQ(UINT64_C(3), sink.outcomes.back().sequence);

    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(std::size_t{1}, fixture.inputCalls.size());
    D6R_REQUIRE_EQ(Input::Jump, fixture.held[101]);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 4, fixture.match.currentTick(), 0)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(0u, fixture.held[101]);

    auto invalid = command(1, 101, 5, fixture.match.currentTick(), Input::AllActions | (1u << 20u));
    D6R_REQUIRE(fixture.input.receive(1, invalid).category == Input::OutcomeCategory::Invalid);
    D6R_REQUIRE_EQ(0u, fixture.held[101]);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 5, fixture.match.currentTick(), Input::Shoot)).category
                == Input::OutcomeCategory::Pending);
}

D6R_TEST_CASE("NIN maximum sequence cannot wrap and lower input does not replace it") {
    Fixture fixture(makeRoster(2));
    OutcomeSink sink;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return sink.send(std::move(payload)); }));
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, std::numeric_limits<std::uint64_t>::max(),
            0, Input::ShowStatus)).category == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 1, 0, Input::Shoot)).category
                == Input::OutcomeCategory::Duplicate);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(Input::ShowStatus, fixture.held[101]);
    D6R_REQUIRE_EQ(std::size_t{1}, sink.count(Input::OutcomeCategory::Applied));
}

D6R_TEST_CASE("NIN dead and frozen players are unavailable without canonical input mutation") {
    Fixture dead(makeRoster(2));
    OutcomeSink deadSink;
    D6R_REQUIRE(dead.input.restore(1, [&](auto payload) { return deadSink.send(std::move(payload)); }));
    D6R_REQUIRE_EQ(ActionResult::Accepted, dead.match.submit(matchAction(
            dead.match, 1, 2, 102, ActionKind::ShotDamage, 101, MaximumLife)));
    D6R_REQUIRE(dead.input.receive(1, command(1, 101, 1, dead.match.currentTick(), Input::Shoot)).category
                == Input::OutcomeCategory::Unavailable);
    D6R_REQUIRE(dead.inputCalls.empty());

    Fixture frozen(makeRoster(2));
    OutcomeSink frozenSink;
    D6R_REQUIRE(frozen.input.restore(1, [&](auto payload) { return frozenSink.send(std::move(payload)); }));
    D6R_REQUIRE_EQ(ActionResult::Accepted, frozen.match.submit(matchAction(
            frozen.match, 1, 1, 101, ActionKind::ShotDamage, 102, MaximumLife)));
    D6R_REQUIRE(frozen.input.receive(1, command(1, 101, 2, frozen.match.currentTick(), Input::MoveRight)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(frozen.input.processTick());
    D6R_REQUIRE_EQ(Input::MoveRight, frozen.held[101]);
    for (std::uint32_t tick = 1; tick < RoundEndActiveTicks; ++tick) D6R_REQUIRE(frozen.input.processTick());
    D6R_REQUIRE(frozen.match.phase() == MatchPhase::RoundEndFrozen);
    D6R_REQUIRE(frozen.input.receive(1, command(1, 101, 3, frozen.match.currentTick(), Input::MoveLeft)).category
                == Input::OutcomeCategory::Unavailable);
    D6R_REQUIRE_EQ(std::size_t{1}, frozen.inputCalls.size());
}

D6R_TEST_CASE("NIN disconnect and revocation clear held and pending input and block authority") {
    Fixture fixture(makeRoster(2));
    OutcomeSink owner, other;
    bool ownerClosed = false;
    bool otherClosed = false;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return owner.send(std::move(payload)); },
                                      [&] { ownerClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return other.send(std::move(payload)); },
                                      [&] { otherClosed = true; }));
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 1, 0, Input::MoveRight)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(Input::MoveRight, fixture.held[102]);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 2, 2, Input::Shoot)).category
                == Input::OutcomeCategory::Pending);
    fixture.input.disconnect(2);
    D6R_REQUIRE_EQ(0u, fixture.held[102]);
    const std::size_t callsAfterClear = fixture.inputCalls.size();
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(callsAfterClear, fixture.inputCalls.size());

    const auto reserved = fixture.input.receive(2, command(2, 102, 3, fixture.match.currentTick(), Input::Jump));
    D6R_REQUIRE_EQ(Input::OutcomeCategory::Unavailable, reserved.category);
    D6R_REQUIRE_EQ(0u, fixture.held[102]);
}

D6R_TEST_CASE("NIN reconnect starts released and permanent revocation closes only a later offender") {
    Fixture fixture(makeRoster(2));
    OutcomeSink owner, other;
    bool ownerClosed = false;
    bool otherClosed = false;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return owner.send(std::move(payload)); },
                                      [&] { ownerClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return other.send(std::move(payload)); },
                                      [&] { otherClosed = true; }));
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 1, 0, Input::MoveRight)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    fixture.input.disconnect(2);
    D6R_REQUIRE_EQ(0u, fixture.held[102]);
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return other.send(std::move(payload)); },
                                      [&] { otherClosed = true; }));
    D6R_REQUIRE_EQ(0u, fixture.held[102]);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 2, fixture.match.currentTick(), Input::Jump)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(Input::Jump, fixture.held[102]);

    fixture.input.revokePlayer(101);
    const auto revoked = fixture.input.receive(1, command(1, 101, 1, fixture.match.currentTick(), Input::Shoot));
    D6R_REQUIRE(revoked.category == Input::OutcomeCategory::Unauthorized && revoked.closeConnection);
    D6R_REQUIRE(ownerClosed);
    D6R_REQUIRE(!otherClosed);
}

D6R_TEST_CASE("NIN round start clears retained shoot state") {
    Fixture fixture(makeRoster(3), 2);
    OutcomeSink sink;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return sink.send(std::move(payload)); }));
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 1, 0, Input::Shoot)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(UINT64_C(1), fixture.match.playerStatistics().at(101).shots);

    D6R_REQUIRE_EQ(ActionResult::Accepted, fixture.match.submit(matchAction(
            fixture.match, 1, 2, 102, ActionKind::ShotDamage, 103, MaximumLife)));
    D6R_REQUIRE_EQ(ActionResult::Accepted, fixture.match.submit(matchAction(
            fixture.match, 2, 2, 102, ActionKind::EnvironmentalDamage, 102, MaximumLife)));
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::RoundEndActive);
    for (std::uint32_t tick = 0; tick < RoundEndTotalTicks; ++tick) D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE(fixture.match.phase() == MatchPhase::ActiveRound);
    D6R_REQUIRE(fixture.input.receive(1, command(1, 101, 2, fixture.match.currentTick(), Input::Shoot)).category
                == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.processTick());
    D6R_REQUIRE_EQ(UINT64_C(2), fixture.match.playerStatistics().at(101).shots);
}

D6R_TEST_CASE("NIN per-player rate rejection preserves sequence and consecutive windows close only remote offender") {
    Fixture fixture(makeRoster(2));
    OutcomeSink host, guest;
    bool hostClosed = false;
    bool guestClosed = false;
    D6R_REQUIRE(fixture.input.restore(1, [&](auto payload) { return host.send(std::move(payload)); },
                                      [&] { hostClosed = true; }));
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest.send(std::move(payload)); },
                                      [&] { guestClosed = true; }));
    for (std::uint64_t sequence = 1; sequence <= 120; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 1, Input::MoveLeft)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 121, 1, Input::MoveRight)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!guestClosed && !hostClosed);

    fixture.now += std::chrono::seconds(2); // skipped window resets consecutive-over-limit tracking
    for (std::uint64_t sequence = 121; sequence <= 240; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 1, Input::MoveRight)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 241, 1, Input::Jump)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!guestClosed);

    fixture.now += std::chrono::seconds(1);
    for (std::uint64_t sequence = 241; sequence <= 360; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 1, Input::Jump)).category
                    == Input::OutcomeCategory::Pending);
    const auto consecutive = fixture.input.receive(2, command(2, 102, 361, 1, Input::Crouch));
    D6R_REQUIRE(consecutive.category == Input::OutcomeCategory::OverLimit && consecutive.closeConnection);
    D6R_REQUIRE(guestClosed);
    D6R_REQUIRE(!hostClosed);
}

D6R_TEST_CASE("NIN mixed invalid and valid submissions share the per-player quota without advancing sequence") {
    Fixture fixture(makeRoster(2));
    OutcomeSink guest;
    bool guestClosed = false;
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest.send(std::move(payload)); },
                                      [&] { guestClosed = true; }));
    for (std::uint64_t attempt = 0; attempt < 60; ++attempt)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 0, 0, Input::MoveLeft)).category
                    == Input::OutcomeCategory::Invalid);
    for (std::uint64_t sequence = 1; sequence <= 60; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::MoveRight)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 61, 0, Input::Jump)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!guestClosed);

    fixture.now += std::chrono::seconds(1);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 61, 0, Input::Jump)).category
                == Input::OutcomeCategory::Pending);
}

D6R_TEST_CASE("NIN consecutive quota windows work across epoch and skipped windows reset them") {
    Fixture fixture(makeRoster(2));
    OutcomeSink guest;
    bool guestClosed = false;
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest.send(std::move(payload)); },
                                      [&] { guestClosed = true; }));
    fixture.now = AuthoritativePlayerInput::TimePoint{} - std::chrono::seconds(1);
    for (std::uint64_t sequence = 1; sequence <= 120; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::MoveLeft)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 121, 0, Input::MoveLeft)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!guestClosed);

    fixture.now = AuthoritativePlayerInput::TimePoint{};
    for (std::uint64_t sequence = 121; sequence <= 240; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::MoveRight)).category
                    == Input::OutcomeCategory::Pending);
    const auto epochConsecutive = fixture.input.receive(2, command(2, 102, 241, 0, Input::MoveRight));
    D6R_REQUIRE(epochConsecutive.category == Input::OutcomeCategory::OverLimit
                && epochConsecutive.closeConnection);
    D6R_REQUIRE(guestClosed);

    Fixture skipped(makeRoster(2));
    OutcomeSink skippedSink;
    bool skippedClosed = false;
    D6R_REQUIRE(skipped.input.restore(2, [&](auto payload) { return skippedSink.send(std::move(payload)); },
                                      [&] { skippedClosed = true; }));
    for (std::uint64_t sequence = 1; sequence <= 120; ++sequence)
        D6R_REQUIRE(skipped.input.receive(2, command(2, 102, sequence, 0, Input::Jump)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(skipped.input.receive(2, command(2, 102, 121, 0, Input::Jump)).category
                == Input::OutcomeCategory::OverLimit);
    skipped.now += std::chrono::seconds(2);
    for (std::uint64_t sequence = 121; sequence <= 240; ++sequence)
        D6R_REQUIRE(skipped.input.receive(2, command(2, 102, sequence, 0, Input::Crouch)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(skipped.input.receive(2, command(2, 102, 241, 0, Input::Crouch)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!skippedClosed);
}

D6R_TEST_CASE("NIN backward quota clock resets before a new consecutive pair") {
    Fixture fixture(makeRoster(2));
    OutcomeSink guest;
    bool guestClosed = false;
    D6R_REQUIRE(fixture.input.restore(2, [&](auto payload) { return guest.send(std::move(payload)); },
                                      [&] { guestClosed = true; }));
    for (std::uint64_t sequence = 1; sequence <= 120; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::MoveLeft)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 121, 0, Input::MoveLeft)).category
                == Input::OutcomeCategory::OverLimit);

    fixture.now -= std::chrono::seconds(1);
    for (std::uint64_t sequence = 121; sequence <= 240; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::MoveRight)).category
                    == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(2, command(2, 102, 241, 0, Input::MoveRight)).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE(!guestClosed);

    fixture.now += std::chrono::seconds(1);
    for (std::uint64_t sequence = 241; sequence <= 360; ++sequence)
        D6R_REQUIRE(fixture.input.receive(2, command(2, 102, sequence, 0, Input::Jump)).category
                    == Input::OutcomeCategory::Pending);
    const auto consecutive = fixture.input.receive(2, command(2, 102, 361, 0, Input::Jump));
    D6R_REQUIRE(consecutive.category == Input::OutcomeCategory::OverLimit && consecutive.closeConnection);
    D6R_REQUIRE(guestClosed);
}

D6R_TEST_CASE("NIN host-wide input ceiling accepts at most 1800 commands per window") {
    const auto roster = makeRoster(15);
    Fixture fixture(roster);
    OutcomeSink sink;
    for (Identity participant = 1; participant <= 15; ++participant)
        D6R_REQUIRE(fixture.input.restore(participant, [&](auto payload) { return sink.send(std::move(payload)); }));
    for (const auto &player: roster)
        for (std::uint64_t sequence = 1; sequence <= 120; ++sequence)
            D6R_REQUIRE(fixture.input.receive(player.participantId,
                    command(player.participantId, player.playerId, sequence, 1, Input::MoveLeft), false).category
                        == Input::OutcomeCategory::Pending);
    D6R_REQUIRE(fixture.input.receive(1, command(1, roster.front().playerId, 121, 1, Input::MoveRight), false).category
                == Input::OutcomeCategory::OverLimit);
    D6R_REQUIRE_EQ(std::size_t{1800}, sink.count(Input::OutcomeCategory::Pending));
    D6R_REQUIRE_EQ(std::size_t{1}, sink.count(Input::OutcomeCategory::OverLimit));
    D6R_REQUIRE(fixture.inputCalls.empty());
}
