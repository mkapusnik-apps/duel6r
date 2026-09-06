#include <string>

#include "source/AnimationLooping.h"
#include "source/Sprite.h"
#include "source/aseprite/animation.h"
#include "tests/TestHarness.h"

#ifndef D6R_TEST_SOURCE_DIR
#error D6R_TEST_SOURCE_DIR must identify the repository root
#endif

D6R_TEST_CASE("Local Play frozen Pick animation completes only at its exact 66-tick boundary") {
    const std::string path = std::string(D6R_TEST_SOURCE_DIR) + "/resources/textures/man/man.ase";
    const animation::Animation source = animation::Animation::loadAseImage(path);
    const auto lookup = source.animationLookup.find("Pick");
    D6R_REQUIRE(lookup != source.animationLookup.end());
    D6R_REQUIRE(lookup->second < source.animations.size());
    const auto &pick = source.animations.at(lookup->second);
    D6R_REQUIRE_EQ(static_cast<std::size_t>(24), pick.size());
    D6R_REQUIRE_EQ(-1, pick.at(22));
    D6R_REQUIRE_EQ(0, pick.at(23));
    for (std::size_t index = 1; index < 22; index += 2)
        D6R_REQUIRE_EQ(100, pick.at(index));

    Duel6::Sprite sprite(pick.data(), 0);
    sprite.setLooping(Duel6::AnimationLooping::OnceAndStop);
    for (int tick = 0; tick < 65; ++tick) {
        sprite.update(1.0f / 60.0f);
        D6R_REQUIRE(!sprite.isFinished());
    }
    sprite.update(1.0f / 60.0f);
    D6R_REQUIRE(sprite.isFinished());
}
