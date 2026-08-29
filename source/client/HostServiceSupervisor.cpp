#include "HostServiceSupervisor.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace Duel6::Client {
    namespace {
        HostServiceTimePoint realNow() { return std::chrono::steady_clock::now(); }

        HostServiceOutcome outcomeForStatus(Network::HostServiceStatusCode status) {
            switch (status) {
                case Network::HostServiceStatusCode::HostManifestInvalid:
                    return HostServiceOutcome::HostManifestInvalid;
                case Network::HostServiceStatusCode::PortUnavailable:
                    return HostServiceOutcome::PortUnavailable;
                case Network::HostServiceStatusCode::StartFailed:
                    return HostServiceOutcome::StartFailed;
                case Network::HostServiceStatusCode::Ready:
                    return HostServiceOutcome::None;
            }
            return HostServiceOutcome::StartFailed;
        }
    }

    HostServiceSupervisor::HostServiceSupervisor(HostServiceDependencies dependencies)
            : dependencies(std::move(dependencies)) {
        if (!this->dependencies.now) this->dependencies.now = realNow;
        if (!this->dependencies.launcher) this->dependencies.launcher = launchHostServiceProcess;
    }

    HostServiceSupervisor::~HostServiceSupervisor() {
        applicationExit();
        if (monitor.joinable()) monitor.join();
    }

    bool HostServiceSupervisor::validConfig(const HostServiceStartConfig &config) {
        if (config.serverExecutable.empty() || config.serverExecutable.size() > 4096
            || !std::filesystem::path(config.serverExecutable).is_absolute()
            || config.endpoint.host.empty() || config.endpoint.host.size() > Network::MaxProtocolStringBytes
            || config.endpoint.port == 0 || config.resourcePath.empty() || config.resourcePath.size() > 4096
            || config.localPlayers == 0 || config.localPlayers > Network::MaxNetworkPlayers
            || config.enabledGameplayScripts.size() > 16) return false;
        for (const auto &script: config.enabledGameplayScripts) {
            if (script.empty() || script.size() > 4096 || script.find('\0') != std::string::npos) return false;
        }
        return config.serverExecutable.find('\0') == std::string::npos
               && config.endpoint.host.find('\0') == std::string::npos
               && config.resourcePath.find('\0') == std::string::npos;
    }

    HostServiceTimePoint HostServiceSupervisor::now() const noexcept {
        try { return dependencies.now(); }
        catch (...) { return HostServiceTimePoint::max(); }
    }

    HostServiceSnapshot HostServiceSupervisor::snapshotLocked() const {
        return {state, outcome, cleanupComplete, retryEligible && cleanupComplete};
    }

    void HostServiceSupervisor::observe(const HostServiceSnapshot &value) const noexcept {
        if (!dependencies.lifecycleObserver) return;
        try { dependencies.lifecycleObserver(value); } catch (...) {}
    }

    void HostServiceSupervisor::joinCompletedMonitor() {
        if (!monitor.joinable()) return;
        bool completed = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            completed = child == nullptr;
        }
        if (completed) monitor.join();
    }

    bool HostServiceSupervisor::start(const HostServiceStartConfig &config) {
        joinCompletedMonitor();
        if (!validConfig(config)) return false;
        HostServiceSnapshot current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state != HostServiceState::NoService
                && !(state == HostServiceState::StartupFailed && retryEligible && cleanupComplete)) return false;
            state = HostServiceState::Starting;
            outcome = HostServiceOutcome::None;
            cleanupComplete = false;
            retryEligible = false;
            applicationExitPending = false;
            selectedStop = SelectedStop::None;
            selectedOutcome = HostServiceOutcome::None;
            retainedConfig = config;
            const auto began = now();
            startupBegan = began;
            const auto latest = HostServiceTimePoint::max() - StartupDeadline;
            startupDeadline = began >= latest ? HostServiceTimePoint::max() : began + StartupDeadline;
            try { child = dependencies.launcher(retainedConfig); } catch (...) { child.reset(); }
            if (!child) {
                state = HostServiceState::StartupFailed;
                outcome = HostServiceOutcome::StartFailed;
                cleanupComplete = true;
                retryEligible = true;
                current = snapshotLocked();
                changed.notify_all();
            } else {
                current = snapshotLocked();
                monitor = std::thread(&HostServiceSupervisor::monitorOwnedChild, this);
            }
        }
        observe(current);
        return true;
    }

    bool HostServiceSupervisor::retry() {
        HostServiceStartConfig config;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state != HostServiceState::StartupFailed || !cleanupComplete || !retryEligible) return false;
            config = retainedConfig;
        }
        return start(config);
    }

    bool HostServiceSupervisor::cancelStartup() {
        HostServiceSnapshot current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state != HostServiceState::Starting || selectedStop != SelectedStop::None) return false;
            selectedStop = SelectedStop::Cancel;
            state = HostServiceState::Stopping;
            current = snapshotLocked();
            changed.notify_all();
        }
        observe(current);
        return true;
    }

    bool HostServiceSupervisor::endSession() {
        HostServiceSnapshot current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state != HostServiceState::Active || selectedStop != SelectedStop::None) return false;
            selectedStop = SelectedStop::EndSession;
            state = HostServiceState::Stopping;
            current = snapshotLocked();
            changed.notify_all();
        }
        observe(current);
        return true;
    }

    void HostServiceSupervisor::applicationExit() {
        HostServiceSnapshot current;
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state == HostServiceState::ApplicationExit) return;
            applicationExitPending = true;
            if (child && selectedStop == SelectedStop::None) selectedStop = SelectedStop::ApplicationExit;
            state = HostServiceState::ApplicationExit;
            retryEligible = false;
            if (!child) cleanupComplete = true;
            current = snapshotLocked();
            changed.notify_all();
            notify = true;
        }
        if (notify) observe(current);
    }

    HostServiceSnapshot HostServiceSupervisor::snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);
        return snapshotLocked();
    }

    bool HostServiceSupervisor::waitForState(HostServiceState expected, std::chrono::milliseconds timeout) const {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, timeout, [&] { return state == expected; });
    }

    void HostServiceSupervisor::monitorOwnedChild() {
        for (;;) {
            SelectedStop stop = SelectedStop::None;
            HostServiceOutcome stopOutcome = HostServiceOutcome::None;
            HostServiceStatusEvent event;
            bool received = false;
            bool exited = false;
            try { received = child->readStatus(event, std::chrono::milliseconds(5)); } catch (...) {}
            try { exited = child->hasExited(); } catch (...) { exited = true; }

            HostServiceSnapshot transition;
            bool didTransition = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (selectedStop != SelectedStop::None) {
                    stop = selectedStop;
                    stopOutcome = selectedOutcome;
                } else if (state == HostServiceState::Starting && received) {
                    const bool currentAttempt = event.receivedAt >= startupBegan;
                    const bool beforeDeadline = event.receivedAt < startupDeadline;
                    if (!currentAttempt) {
                        // The per-child channel should make this impossible; reject it without changing state.
                    } else if (event.code == Network::HostServiceStatusCode::Ready) {
                        if (beforeDeadline) {
                            state = HostServiceState::Active;
                            outcome = HostServiceOutcome::None;
                            didTransition = true;
                        } else {
                            selectedStop = stop = SelectedStop::StartupFailure;
                            selectedOutcome = stopOutcome = HostServiceOutcome::StartupTimedOut;
                        }
                    } else if (beforeDeadline) {
                        selectedStop = stop = SelectedStop::StartupFailure;
                        selectedOutcome = stopOutcome = outcomeForStatus(event.code);
                    } else {
                        selectedStop = stop = SelectedStop::StartupFailure;
                        selectedOutcome = stopOutcome = HostServiceOutcome::StartupTimedOut;
                    }
                }
                if (stop == SelectedStop::None && state == HostServiceState::Starting && now() >= startupDeadline) {
                    selectedStop = stop = SelectedStop::StartupFailure;
                    selectedOutcome = stopOutcome = HostServiceOutcome::StartupTimedOut;
                }
                if (stop == SelectedStop::None && exited) {
                    if (state == HostServiceState::Starting) {
                        selectedStop = stop = SelectedStop::StartupFailure;
                        selectedOutcome = stopOutcome = HostServiceOutcome::ExitedBeforeReady;
                    } else if (state == HostServiceState::Active) {
                        selectedStop = stop = SelectedStop::SessionFailure;
                        selectedOutcome = stopOutcome = HostServiceOutcome::StoppedUnexpectedly;
                    }
                }
                if (didTransition) {
                    transition = snapshotLocked();
                    changed.notify_all();
                }
            }
            if (didTransition) observe(transition);
            if (stop != SelectedStop::None) {
                finishOwnedChild(stop, stopOutcome);
                return;
            }
        }
    }

    void HostServiceSupervisor::finishOwnedChild(SelectedStop selected, HostServiceOutcome selectedFailure) {
        try { child->requestStop(); } catch (...) {}
        const auto began = now();
        const auto latest = HostServiceTimePoint::max() - CleanupDeadline;
        const auto hardDeadline = began >= latest ? HostServiceTimePoint::max() : began + CleanupDeadline;
        const auto forceBudget = std::chrono::milliseconds(100);
        const auto gracefulDeadline = hardDeadline == HostServiceTimePoint::max()
                                      ? hardDeadline : hardDeadline - forceBudget;
        bool exited = false;
        while (!exited && now() < gracefulDeadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(gracefulDeadline - now());
            const auto slice = std::min(remaining, std::chrono::milliseconds(10));
            if (slice <= std::chrono::milliseconds::zero()) break;
            try { exited = child->waitForExit(slice); } catch (...) { break; }
        }
        if (!exited) {
            try { child->forceTerminate(); } catch (...) {}
            while (!exited && now() < hardDeadline) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(hardDeadline - now());
                const auto slice = std::min(remaining, std::chrono::milliseconds(10));
                if (slice <= std::chrono::milliseconds::zero()) break;
                try { exited = child->waitForExit(slice); } catch (...) { break; }
            }
        }

        HostServiceSnapshot current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            child.reset();
            cleanupComplete = true;
            selectedStop = SelectedStop::None;
            selectedOutcome = HostServiceOutcome::None;
            if (applicationExitPending) {
                state = HostServiceState::ApplicationExit;
                outcome = HostServiceOutcome::None;
                retryEligible = false;
            } else switch (selected) {
                case SelectedStop::Cancel:
                case SelectedStop::EndSession:
                    state = HostServiceState::NoService;
                    outcome = HostServiceOutcome::None;
                    retryEligible = false;
                    break;
                case SelectedStop::ApplicationExit:
                    state = HostServiceState::ApplicationExit;
                    outcome = HostServiceOutcome::None;
                    retryEligible = false;
                    break;
                case SelectedStop::StartupFailure:
                    state = HostServiceState::StartupFailed;
                    outcome = selectedFailure;
                    retryEligible = selectedFailure != HostServiceOutcome::HostManifestInvalid;
                    break;
                case SelectedStop::SessionFailure:
                    state = HostServiceState::SessionFailed;
                    outcome = selectedFailure;
                    retryEligible = false;
                    break;
                case SelectedStop::None:
                    break;
            }
            current = snapshotLocked();
            changed.notify_all();
        }
        observe(current);
    }

    const char *hostServiceOutcomeIdentifier(HostServiceOutcome value) noexcept {
        switch (value) {
            case HostServiceOutcome::HostManifestInvalid: return "host-gameplay-content-manifest-invalid";
            case HostServiceOutcome::StartFailed: return "host-service-start-failed";
            case HostServiceOutcome::PortUnavailable: return "host-service-port-unavailable";
            case HostServiceOutcome::ExitedBeforeReady: return "host-service-exited-before-ready";
            case HostServiceOutcome::StartupTimedOut: return "host-service-startup-timed-out";
            case HostServiceOutcome::StoppedUnexpectedly: return "host-service-stopped-unexpectedly";
            case HostServiceOutcome::None: return "";
        }
        return "host-service-start-failed";
    }

    const char *hostServiceOutcomeCopy(HostServiceOutcome value) noexcept {
        switch (value) {
            case HostServiceOutcome::HostManifestInvalid:
                return "Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.";
            case HostServiceOutcome::StartFailed: return "Hosted session could not start.";
            case HostServiceOutcome::PortUnavailable:
                return "The selected port is unavailable. Choose another port and try again.";
            case HostServiceOutcome::ExitedBeforeReady: return "Hosted session stopped before it was ready.";
            case HostServiceOutcome::StartupTimedOut: return "Hosted session startup timed out.";
            case HostServiceOutcome::StoppedUnexpectedly: return "Hosted session stopped unexpectedly.";
            case HostServiceOutcome::None: return "";
        }
        return "Hosted session could not start.";
    }
}
