#ifndef DUEL6_RUNTIME_OBSERVATIONS_H
#define DUEL6_RUNTIME_OBSERVATIONS_H

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Duel6::Server {
    enum class ObservedTerminal : unsigned {
        None, AdmissionEnded, AdmissionSealed, NetworkSampleFailed, ReplicationFailed,
        TransportTerminal, InputRejected, HostCancelled, HostFailed
    };
    // Optional fixed-size observation storage. No callback, payload capture,
    // clock replacement, allocation or output occurs on the runtime path.
    // These values are never read to make a gameplay/transport decision.
    struct RuntimeObservations {
        std::atomic<std::uint64_t> offers{0}, grants{0}, confirmations{0}, snapshots{0}, responses{0}, probes{0}, admissions{0};
        std::atomic<bool> calibrated{false}, probeOutstanding{false}, initialReady{false};
        std::atomic<std::uint64_t> lastProbeSequence{0}, lastResponseSequence{0};
        std::atomic<std::uint64_t> maxResponseRttUs{0}, responsesOverBudget{0};
        std::atomic<std::int64_t> lastProbeUs{-1}, lastResponseUs{-1}, calibrationBudgetMs{0};
        std::atomic<int> lastReplicationResult{-1}, transportState{-1}, transportFailure{-1};
        std::atomic<std::int64_t> closeUs{-1}, deadlineUs{-1};
        std::atomic<unsigned> terminal{0};
        std::atomic<std::uint64_t> hostLoops{0}, hostTicks{0}, waitCalls{0}, requestedWaitUs{0}, actualWaitUs{0};
        std::atomic<std::uint64_t> maxWaitUs{0}, maxLoopWorkUs{0}, maxTickDebtUs{0}, lastTickDebtUs{0};
        std::atomic<std::int64_t> matchStartUs{-1}, firstOutcomeUs{-1}, hostStopUs{-1};
        std::atomic<std::int64_t> lastMatchStartUs{-1};
        std::atomic<std::uint64_t> matchStarts{0};
        std::atomic<std::uint64_t> lastTick{0}, firstOutcomeTick{0};
        std::atomic<int> lastPhase{-1}, hostStage{-1};
        static std::int64_t micros(std::chrono::steady_clock::time_point at) noexcept {
            return std::chrono::duration_cast<std::chrono::microseconds>(at.time_since_epoch()).count();
        }
        static void maximum(std::atomic<std::uint64_t> &target, std::uint64_t value) noexcept {
            auto prior = target.load(std::memory_order_relaxed);
            while (prior < value && !target.compare_exchange_weak(prior, value, std::memory_order_relaxed)) {}
        }
        void firstTerminal(ObservedTerminal reason) noexcept {
            unsigned none = 0;
            terminal.compare_exchange_strong(none, static_cast<unsigned>(reason), std::memory_order_relaxed);
        }
    };
}
#endif
