#include "tests/TestHarness.h"
#include "source/input/PlayerControls.h"

using namespace Duel6;

D6R_TEST_CASE("control managers keep simultaneous inputs and preset ownership independent") {
    Console console(Console::ExpandFlag);
    Input first(console), second(console);
    PlayerControlsManager firstControls(first), secondControls(second);
    D6R_REQUIRE_EQ(Size(6), firstControls.getSize());
    D6R_REQUIRE_EQ(Size(6), secondControls.getSize());
    D6R_REQUIRE(&firstControls.get(0) != &secondControls.get(0));
    first.setPressed(SDLK_LEFT, true);
    second.setPressed(SDLK_q, true);
    D6R_REQUIRE(firstControls.get(0).getLeft().isPressed());
    D6R_REQUIRE(!secondControls.get(0).getLeft().isPressed());
    D6R_REQUIRE(!firstControls.get(1).getShoot().isPressed());
    D6R_REQUIRE(secondControls.get(1).getShoot().isPressed());
    firstControls.detectJoypads();
    secondControls.detectJoypads();
    D6R_REQUIRE_EQ(Size(6), firstControls.getNumAvailable());
    D6R_REQUIRE_EQ(Size(6), secondControls.getNumAvailable());
}

D6R_TEST_CASE("sequential control managers never sample a destroyed Input") {
    Console console(Console::ExpandFlag);
    for (unsigned lifetime = 0; lifetime < 8; ++lifetime) {
        // Heap lifetime deliberately exposes old references to ASan, rather
        // than allowing a later stack Input to reuse the same address.
        auto input = std::make_unique<Input>(console);
        PlayerControlsManager controls(*input);
        D6R_REQUIRE_EQ(Size(6), controls.getSize());
        D6R_REQUIRE(!controls.get(0).getLeft().isPressed());
        input->setPressed(SDLK_LEFT, true);
        D6R_REQUIRE(controls.get(0).getLeft().isPressed());
        input->setPressed(SDLK_LEFT, false);
        D6R_REQUIRE(!controls.get(0).getLeft().isPressed());
    }
}
