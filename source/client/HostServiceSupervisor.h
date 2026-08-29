#ifndef DUEL6_CLIENT_HOSTSERVICESUPERVISOR_H
#define DUEL6_CLIENT_HOSTSERVICESUPERVISOR_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../network/HostServiceControlProtocol.h"
#include "../network/Protocol.h"

namespace Duel6::Client {
    enum class HostServiceState {
        NoService,
        Starting,
        Active,
        Stopping,
        StartupFailed,
        SessionFailed,
        ApplicationExit
    };

    enum class HostServiceOutcome {
        None,
        HostManifestInvalid,
        StartFailed,
        PortUnavailable,
        ExitedBeforeReady,
        StartupTimedOut,
        StoppedUnexpectedly
    };

    enum class HostServiceStopReason {
        Cancel,
        EndSession,
        ApplicationExit,
        Failure
    };

    using HostServiceTimePoint = std::chrono::steady_clock::time_point;

    struct HostServiceStatusEvent {
        Network::HostServiceStatusCode code = Network::HostServiceStatusCode::StartFailed;
        HostServiceTimePoint receivedAt{};
    };

    struct HostServiceStartConfig {
        std::string serverExecutable;
        Network::Endpoint endpoint;
        std::string resourcePath;
        std::vector<std::string> enabledGameplayScripts;
        std::uint8_t localPlayers = 1;
    };

    struct HostServiceSnapshot {
        HostServiceState state = HostServiceState::NoService;
        HostServiceOutcome outcome = HostServiceOutcome::None;
        bool cleanupComplete = true;
        bool retryAllowed = false;
    };

    class HostServiceChild {
    public:
        virtual ~HostServiceChild() = default;
        virtual bool readStatus(HostServiceStatusEvent &event, std::chrono::milliseconds timeout) = 0;
        virtual bool hasExited() = 0;
        virtual void requestStop() = 0;
        virtual bool waitForExit(std::chrono::milliseconds timeout) = 0;
        virtual void forceTerminate() = 0;
    };

    using HostServiceLauncher = std::function<std::unique_ptr<HostServiceChild>(const HostServiceStartConfig &)>;

    struct HostServiceDependencies {
        std::function<HostServiceTimePoint()> now;
        HostServiceLauncher launcher;
        std::function<void(const HostServiceSnapshot &)> lifecycleObserver;
    };

    class HostServiceSupervisor {
    public:
        static constexpr auto StartupDeadline = std::chrono::seconds(10);
        static constexpr auto CleanupDeadline = std::chrono::seconds(3);

        explicit HostServiceSupervisor(HostServiceDependencies dependencies = {});
        ~HostServiceSupervisor();

        HostServiceSupervisor(const HostServiceSupervisor &) = delete;
        HostServiceSupervisor &operator=(const HostServiceSupervisor &) = delete;

        bool start(const HostServiceStartConfig &config);
        bool retry();
        bool cancelStartup();
        bool endSession();
        void applicationExit();

        HostServiceSnapshot snapshot() const;
        bool waitForState(HostServiceState state, std::chrono::milliseconds timeout) const;

    private:
        enum class SelectedStop {
            None,
            Cancel,
            EndSession,
            ApplicationExit,
            StartupFailure,
            SessionFailure
        };

        static bool validConfig(const HostServiceStartConfig &config);
        HostServiceTimePoint now() const noexcept;
        HostServiceSnapshot snapshotLocked() const;
        void observe(const HostServiceSnapshot &value) const noexcept;
        void monitorOwnedChild();
        void finishOwnedChild(SelectedStop selected, HostServiceOutcome selectedOutcome);
        void joinCompletedMonitor();

        HostServiceDependencies dependencies;
        mutable std::mutex mutex;
        mutable std::condition_variable changed;
        HostServiceState state = HostServiceState::NoService;
        HostServiceOutcome outcome = HostServiceOutcome::None;
        bool cleanupComplete = true;
        bool retryEligible = false;
        bool applicationExitPending = false;
        SelectedStop selectedStop = SelectedStop::None;
        HostServiceOutcome selectedOutcome = HostServiceOutcome::None;
        HostServiceTimePoint startupBegan{};
        HostServiceTimePoint startupDeadline{};
        HostServiceStartConfig retainedConfig;
        std::unique_ptr<HostServiceChild> child;
        std::thread monitor;
    };

    std::unique_ptr<HostServiceChild> launchHostServiceProcess(const HostServiceStartConfig &config);
    const char *hostServiceOutcomeIdentifier(HostServiceOutcome outcome) noexcept;
    const char *hostServiceOutcomeCopy(HostServiceOutcome outcome) noexcept;
}

#endif
