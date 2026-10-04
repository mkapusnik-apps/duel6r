#ifndef DUEL6_TEST_HOST_END_BOUNDARY_CONTROL_H
#define DUEL6_TEST_HOST_END_BOUNDARY_CONTROL_H

#include <atomic>
#include <memory>

namespace Duel6::Test {
    // Owned by one isolated harness process, never installed in shipped targets.
    struct HostEndBoundaryControl {
        std::atomic<bool> armed{false};
        std::atomic<unsigned> notices{0}, rejectedProbes{0}, sealedNotices{0};
        std::atomic<bool> eligibleReceipt{false};
    };
    void installHostEndBoundaryControl(std::shared_ptr<HostEndBoundaryControl> control);
}

#endif
