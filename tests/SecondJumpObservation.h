#ifndef D6R_TEST_SECOND_JUMP_OBSERVATION_H
#define D6R_TEST_SECOND_JUMP_OBSERVATION_H
#include "source/network/StateReplication.h"
#include "source/network/PlayerInputProtocol.h"
#include <array>
#include <optional>

namespace Duel6::Test {
    // A test oracle, not gameplay prediction. Each witness belongs to the same
    // second input edge and immutable actor/round. Delayed receipt confirmation
    // may qualify an already observed witness within the caller's ONE deadline.
    class SecondJumpObservation {
        struct Slot {
            Network::Replication::Identity player;
            Network::Replication::Identity owner;
            std::int64_t firstHeight;
            std::optional<std::uint64_t> witnessTick;
        };
        Network::Replication::Identity session, match, round;
        std::array<Slot, 2> slots;
    public:
        SecondJumpObservation(Network::Replication::Identity session, Network::Replication::Identity match,
                              Network::Replication::Identity round,
                              const Network::Replication::PlayerState &first,
                              const Network::Replication::PlayerState &second)
                : session(session), match(match), round(round),
                  slots{{{first.playerId, first.ownerParticipantId, first.positionY, {}},
                         {second.playerId, second.ownerParticipantId, second.positionY, {}}}} {}
        void observe(std::size_t index, const Network::Replication::CanonicalState &state,
                     const Network::Replication::PlayerState &actor) {
            auto &slot = slots.at(index);
            if (state.sessionId != session || state.matchId != match || !state.round || state.round->roundId != round
                || state.phase != Network::Replication::Phase::ActiveRound || actor.playerId != slot.player
                || actor.ownerParticipantId != slot.owner
                || actor.lifeState != Network::Replication::LifeState::Alive
                || actor.actionMask != Network::Input::Jump || actor.velocityY <= 7 * 65536
                || actor.positionY <= slot.firstHeight) return;
            slot.witnessTick = state.phaseTime;
        }
        bool complete(std::size_t index, bool correctSecondEdgeApplied, std::uint64_t effectiveTick) const {
            const auto &slot = slots.at(index);
            // A later grounded first jump while a key remains held is not the
            // second-edge impulse. Qualify the produced witness, not its late
            // delivery time, within the immediate canonical-update boundary.
            return correctSecondEdgeApplied && slot.witnessTick && effectiveTick <= *slot.witnessTick
                && *slot.witnessTick - effectiveTick <= 4;
        }
    };
}
#endif
