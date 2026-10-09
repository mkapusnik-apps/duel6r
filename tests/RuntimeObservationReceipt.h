#ifndef DUEL6_RUNTIME_OBSERVATION_RECEIPT_H
#define DUEL6_RUNTIME_OBSERVATION_RECEIPT_H
#include "source/server/RuntimeObservations.h"
#include "source/server/AuthoritativeMatchTypes.h"
#include <iostream>
#include <memory>
#include <string>

namespace Duel6::Test {
    struct RuntimeObservationReceipt {
        std::string scope;
        std::shared_ptr<Server::RuntimeObservations> data = std::make_shared<Server::RuntimeObservations>();
        ~RuntimeObservationReceipt() {
            const auto &v = *data;
            std::cout << "DIAGNOSTIC runtime;scope=" << scope
                << ";offers=" << v.offers << ";grants=" << v.grants << ";confirmations=" << v.confirmations
                << ";snapshots=" << v.snapshots << ";responses=" << v.responses << ";probes=" << v.probes
                << ";calibrated=" << v.calibrated << ";probe-outstanding=" << v.probeOutstanding
                << ";initial-ready=" << v.initialReady << ";probe-sequence=" << v.lastProbeSequence
                << ";response-sequence=" << v.lastResponseSequence
                << ";max-response-rtt-us=" << v.maxResponseRttUs << ";rtt-over-budget-observed=" << v.responsesOverBudget
                << ";last-probe-us=" << v.lastProbeUs << ";last-response-us=" << v.lastResponseUs
                << ";calibration-budget-ms=" << v.calibrationBudgetMs << ";replication-result=" << v.lastReplicationResult
                << ";admissions=" << v.admissions << ";first-terminal=" << v.terminal
                << ";transport-state=" << v.transportState << ";transport-failure=" << v.transportFailure
                << ";close-us=" << v.closeUs << ";deadline-us=" << v.deadlineUs
                << ";host-loops=" << v.hostLoops << ";host-ticks=" << v.hostTicks
                << ";wait-calls=" << v.waitCalls << ";requested-wait-us=" << v.requestedWaitUs
                << ";actual-wait-us=" << v.actualWaitUs << ";max-wait-us=" << v.maxWaitUs
                << ";max-loop-work-us=" << v.maxLoopWorkUs << ";max-tick-debt-us=" << v.maxTickDebtUs
                << ";last-tick-debt-us=" << v.lastTickDebtUs << ";match-start-us=" << v.matchStartUs
                << ";last-match-start-us=" << v.lastMatchStartUs << ";match-starts=" << v.matchStarts
                << ";first-outcome-us=" << v.firstOutcomeUs << ";host-stop-us=" << v.hostStopUs
                << ";last-tick=" << v.lastTick << ";first-outcome-tick=" << v.firstOutcomeTick
                << ";required-round-end-ticks=" << Server::Authoritative::RoundEndTotalTicks
                << ";last-active-authoritative-phase=" << v.lastPhase << ";hosted-stage=" << v.hostStage << '\n';
        }
    };
}
#endif
