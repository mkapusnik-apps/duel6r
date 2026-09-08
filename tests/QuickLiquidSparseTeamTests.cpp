#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "source/Game.h"
#include "source/GameResources.h"
#include "source/GameSettings.h"
#include "source/Weapon.h"
#include "source/gamemodes/TeamDeathMatch.h"
#include "source/server/DeterministicRandom.h"
#include "tests/TestHarness.h"

namespace {
    using Position = std::pair<Duel6::Int32, Duel6::Int32>;

    D6R_TEST_CASE("ENV-QL-004 sparse Team starts wrap safely and retain preferred-first placement") {
        Duel6::GameResources resources;
        resources.loadHeadless(std::string(D6R_TEST_SOURCE_DIR) + "/resources");

        Duel6::GameSettings settings;
        settings.setQuickLiquid(true);
        settings.enableWeapon(Duel6::Weapon::values().front(), true);

        Duel6::Server::Authoritative::DeterministicRandom random(UINT64_C(0x81));
        Duel6::Game game(resources, settings, random);
        Duel6::TeamDeathMatch mode(4, false);
        const std::vector<std::string> names = {
                "Player 1", "Player 2", "Player 3", "Player 4", "Player 5", "Player 6"};
        const std::vector<Duel6::Size> rosterSlots = {0, 1, 2, 3, 4, 5};
        game.startHeadlessRound(names,
                std::string(D6R_TEST_SOURCE_DIR) + "/tests/fixtures/quick-liquid-sparse-team.json",
                rosterSlots, false, mode);

        const std::set<Position> expectedPositions = {{2, 4}, {5, 4}, {3, 2}};
        std::vector<Position> actual;
        for (const auto &player: game.getPlayers()) {
            const Position position = {
                    static_cast<Duel6::Int32>(std::floor(player.getPosition().x)),
                    static_cast<Duel6::Int32>(std::floor(player.getPosition().y))};
            D6R_REQUIRE(expectedPositions.count(position) == 1);
            actual.push_back(position);
        }

        D6R_REQUIRE_EQ(names.size(), actual.size());
        for (Duel6::Size cycle = 0; cycle < 2; ++cycle) {
            const auto offset = cycle * expectedPositions.size();
            D6R_REQUIRE_EQ(4, actual[offset].second);
            D6R_REQUIRE_EQ(4, actual[offset + 1].second);
            D6R_REQUIRE_EQ(2, actual[offset + 2].second);
            D6R_REQUIRE_EQ(expectedPositions,
                    std::set<Position>(actual.begin() + offset, actual.begin() + offset + expectedPositions.size()));
        }

        game.endHeadlessRound();
    }
}
