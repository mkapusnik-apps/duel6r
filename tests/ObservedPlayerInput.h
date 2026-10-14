#ifndef D6R_TEST_OBSERVED_PLAYER_INPUT_H
#define D6R_TEST_OBSERVED_PLAYER_INPUT_H
#include <optional>
#include <cstdint>
namespace Duel6::Test::ObservedInput {
    void begin();
    void end();
    void secondEdge();
    void releaseFirstEdge();
    std::optional<std::uint64_t> appliedSecondJump(std::uint64_t player, std::uint64_t participant);
}
#endif
