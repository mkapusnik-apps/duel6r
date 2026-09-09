#ifndef DUEL6_NETWORK_HOSTCOMPOSITIONPROTOCOL_H
#define DUEL6_NETWORK_HOSTCOMPOSITIONPROTOCOL_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Duel6::Network::HostComposition {
    constexpr std::uint32_t ProtocolIdentifier = 0x44364843u; // D6HC
    constexpr std::uint16_t ProtocolVersion = 2;
    constexpr std::size_t MaximumDisplayNameBytes = 64;

    enum class Kind : std::uint16_t {
        Setup = 1,
        StartMatch = 2,
        ReturnToLobby = 3,
        AdvanceRound = 4,
        PlayerInput = 5,
        CanonicalSnapshot = 6,
        PlayerInputOutcome = 7,
        UpdateSetup = 8,
        ConfigurationChanged = 9,
        RosterMove = 10
    };

    struct Setup {
        std::vector<std::string> localPlayerNames;
        std::string mode = "Deathmatch";
        std::uint8_t teamCount = 0;
        bool friendlyFire = false;
        std::string levelPlan = "Fixed level";
        std::string fixedLevel;
        std::uint8_t roundLimit = 1;
        bool assistance = true;
        bool quickLiquid = true;
        bool burnableTrees = true;
    };

    struct Message {
        Kind kind = Kind::Setup;
        std::optional<Setup> setup;
        std::vector<std::uint8_t> payload;
        std::uint64_t rosterPlayerId = 0;
        std::int8_t rosterDirection = 0;
    };

    std::vector<std::uint8_t> serializeSetup(const Setup &setup);
    std::vector<std::uint8_t> serializeSetupUpdate(const Setup &setup);
    std::vector<std::uint8_t> serializeAction(Kind kind);
    std::vector<std::uint8_t> serializePayload(Kind kind, const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> serializeRosterMove(std::uint64_t playerId, std::int8_t direction);
    std::optional<Message> deserialize(const std::vector<std::uint8_t> &payload) noexcept;
}

#endif
