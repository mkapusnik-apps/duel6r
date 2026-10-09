#include "TestHarness.h"
#include "source/platform/DarwinProcess.h"

using namespace Duel6::Platform::Darwin;

D6R_TEST_CASE("Guardian IPC retains origin exit across delayed consumption and rejects stale future and backward times") {
    const auto frame = guardianFrame(GuardianEvent::LeaderExited, 42, 999);
    const auto delayed = decodeGuardianFrame(frame, 100, 500, 2000);
    D6R_REQUIRE(delayed);
    D6R_REQUIRE_EQ(std::uint64_t{999}, delayed->origin);
    D6R_REQUIRE(delayed->origin < 1000); // Original startup deadline; consumption is later.
    D6R_REQUIRE(!decodeGuardianFrame(frame, 1000, 0, 2000));
    D6R_REQUIRE(!decodeGuardianFrame(frame, 100, 1000, 2000));
    D6R_REQUIRE(!decodeGuardianFrame(frame, 100, 0, 998));
    D6R_REQUIRE(!decodeGuardianFrame(guardianFrame(GuardianEvent::LeaderExited, 0, 999), 100, 0, 2000));
    const auto atDeadline = decodeGuardianFrame(guardianFrame(GuardianEvent::LeaderExited, 42, 1000), 100, 0, 2000);
    D6R_REQUIRE(atDeadline);
    D6R_REQUIRE_EQ(std::uint64_t{1000}, atDeadline->origin);
}

D6R_TEST_CASE("Darwin cleanup requires exited anchored leader and two complete descendant-zero observations") {
    CleanupState state;
    D6R_REQUIRE(state.maySignal());
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    state.observeExit();
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    D6R_REQUIRE(!state.observeGroup(GroupInspection::Unknown));
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    D6R_REQUIRE(!state.observeGroup(GroupInspection::Descendants));
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    D6R_REQUIRE(state.observeGroup(GroupInspection::LeaderOnly));
    state.loseAnchor();
    D6R_REQUIRE(!state.maySignal());
    D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
}

D6R_TEST_CASE("Darwin ownership loss cannot be reversed by exit or successful inspection") {
    CleanupState state;
    state.loseAnchor();
    state.observeExit();
    for (unsigned attempt = 0; attempt < 10; ++attempt) {
        D6R_REQUIRE(!state.maySignal());
        D6R_REQUIRE(!state.observeGroup(GroupInspection::LeaderOnly));
    }
}
