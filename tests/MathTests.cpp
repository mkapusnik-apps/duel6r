#include "source/math/Math.h"
#include "source/math/Matrix.h"
#include "source/math/Vector.h"
#include "source/SysEvent.h"
#include "tests/TestHarness.h"

D6R_TEST_CASE("Vector operations stay numerically stable") {
    Duel6::Vector left(3.0f, 4.0f, 0.0f);
    Duel6::Vector right(2.0f, -1.0f, 5.0f);

    D6R_REQUIRE_NEAR(5.0, left.length(), 1e-6);
    D6R_REQUIRE_NEAR(2.0, left.dot(right), 1e-6);

    Duel6::Vector cross = left.cross(right);
    D6R_REQUIRE_NEAR(20.0, cross.x, 1e-6);
    D6R_REQUIRE_NEAR(-15.0, cross.y, 1e-6);
    D6R_REQUIRE_NEAR(-11.0, cross.z, 1e-6);
}

D6R_TEST_CASE("Matrix transforms apply in expected order") {
    Duel6::Matrix transform = Duel6::Matrix::translate(5.0f, -1.0f, 2.0f) * Duel6::Matrix::scale(2.0f, 3.0f, 4.0f);
    Duel6::Vector result = transform * Duel6::Vector(1.0f, 2.0f, 3.0f);

    D6R_REQUIRE_NEAR(7.0, result.x, 1e-5);
    D6R_REQUIRE_NEAR(5.0, result.y, 1e-5);
    D6R_REQUIRE_NEAR(14.0, result.z, 1e-5);
}

D6R_TEST_CASE("Matrix subtraction assignment subtracts each component") {
    Duel6::Matrix value = Duel6::Matrix::scale(4.0f, 5.0f, 6.0f);
    value -= Duel6::Matrix::scale(1.0f, 2.0f, 3.0f);

    D6R_REQUIRE_NEAR(3.0, value(0, 0), 1e-6);
    D6R_REQUIRE_NEAR(3.0, value(1, 1), 1e-6);
    D6R_REQUIRE_NEAR(3.0, value(2, 2), 1e-6);
    D6R_REQUIRE_NEAR(0.0, value(3, 3), 1e-6);
}

D6R_TEST_CASE("Pointer coordinates map back into a scaled and centered menu") {
    const Duel6::MouseButtonEvent physical(967, 360, Duel6::SysEvent::MouseButton::LEFT,
                                            Duel6::SysEvent::ButtonState::PRESSED, true);
    const Duel6::MouseButtonEvent logical = physical.inverseTransform(1.35f, 386, 67);

    D6R_REQUIRE_EQ(430, logical.getX());
    D6R_REQUIRE_EQ(217, logical.getY());
    D6R_REQUIRE(logical.getButton() == Duel6::SysEvent::MouseButton::LEFT);
    D6R_REQUIRE(logical.isPressed());
    D6R_REQUIRE(logical.isDoubleClick());
}

D6R_TEST_CASE("Scaled pointer motion preserves logical drag distance and button state") {
    const Duel6::MouseMotionEvent physical(1183, 841, 27, -54,
                                            static_cast<Duel6::Uint32>(Duel6::SysEvent::MouseButton::LEFT));
    const Duel6::MouseMotionEvent logical = physical.inverseTransform(1.35f, 386, 67);

    D6R_REQUIRE_EQ(590, logical.getX());
    D6R_REQUIRE_EQ(573, logical.getY());
    D6R_REQUIRE_EQ(20, logical.getXDiff());
    D6R_REQUIRE_EQ(-40, logical.getYDiff());
    D6R_REQUIRE(logical.isPressed(Duel6::SysEvent::MouseButton::LEFT));
}

D6R_TEST_CASE("Wheel mapping changes position but not scroll amount") {
    const Duel6::MouseWheelEvent physical(202, 257, -2, 3);
    const Duel6::MouseWheelEvent logical = physical.inverseTransform(9.0f / 7.0f, 93, 0);

    D6R_REQUIRE_EQ(84, logical.getX());
    D6R_REQUIRE_EQ(199, logical.getY());
    D6R_REQUIRE_EQ(-2, logical.getAmountX());
    D6R_REQUIRE_EQ(3, logical.getAmountY());
}
