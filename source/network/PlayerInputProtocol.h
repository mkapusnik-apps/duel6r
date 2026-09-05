#ifndef DUEL6_NETWORK_PLAYERINPUTPROTOCOL_H
#define DUEL6_NETWORK_PLAYERINPUTPROTOCOL_H

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "SessionTransport.h"

namespace Duel6::Network::Input {
    using Identity = std::uint64_t;
    using Tick = std::uint64_t;

    constexpr std::uint32_t PlayerInputProtocolIdentifier = 0x4436494e; // D6IN
    constexpr std::uint16_t PlayerInputProtocolVersion = 1;
    constexpr std::uint32_t MoveLeft = 1u << 0u;
    constexpr std::uint32_t MoveRight = 1u << 1u;
    constexpr std::uint32_t Jump = 1u << 2u;
    constexpr std::uint32_t Crouch = 1u << 3u;
    constexpr std::uint32_t Shoot = 1u << 4u;
    constexpr std::uint32_t PickOrSwapWeapon = 1u << 5u;
    constexpr std::uint32_t ShowStatus = 1u << 6u;
    constexpr std::uint32_t AllActions = MoveLeft | MoveRight | Jump | Crouch | Shoot
                                                 | PickOrSwapWeapon | ShowStatus;

    struct PlayerActionState {
        bool moveLeft = false;
        bool moveRight = false;
        bool jump = false;
        bool crouch = false;
        bool shoot = false;
        bool pickOrSwapWeapon = false;
        bool showStatus = false;

        std::uint32_t mask() const noexcept;
    };

    enum class FrameKind : std::uint16_t { Command = 1, Outcome = 2, AppliedAcknowledgment = 3 };
    enum class OutcomeCategory : std::uint8_t {
        Pending,
        Applied,
        Unauthorized,
        Stale,
        Duplicate,
        TooFuture,
        Invalid,
        Unavailable,
        Superseded,
        OverLimit
    };

    struct Command {
        Identity participantId = 0;
        Identity playerId = 0;
        std::uint64_t sequence = 0;
        Tick targetTick = 0;
        std::uint32_t actions = 0;
    };

    struct Outcome {
        OutcomeCategory category = OutcomeCategory::Invalid;
        Identity playerId = 0;
        std::uint64_t sequence = 0;
        Tick effectiveTick = 0;
    };

    struct Frame {
        FrameKind kind = FrameKind::Command;
        std::optional<Command> command;
        std::optional<Outcome> outcome;
    };

    bool isPlayerInputFrame(const std::vector<std::uint8_t> &payload) noexcept;
    std::vector<std::uint8_t> serializeCommand(const Command &command);
    std::vector<std::uint8_t> serializeOutcome(const Outcome &outcome);
    std::optional<Frame> deserializeFrame(const std::vector<std::uint8_t> &payload) noexcept;

    // Converts complete seven-action local control states into independently sequenced commands
    // for the participant's owned players. Local Play does not instantiate this type.
    class OwnedPlayerCommandSource final {
    public:
        OwnedPlayerCommandSource(Identity participantId, std::vector<Identity> ownedPlayerIds);
        std::optional<Command> sample(Identity playerId, Tick targetTick, std::uint32_t actions);
        std::optional<Command> sample(Identity playerId, Tick targetTick, const PlayerActionState &actions);
        void reset() noexcept;
    private:
        Identity participantId;
        std::map<Identity, std::uint64_t> sequences;
    };
}

#endif
