#ifndef DUEL6_NETWORK_HOSTSERVICEDIAGNOSTICS_H
#define DUEL6_NETWORK_HOSTSERVICEDIAGNOSTICS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#ifdef __linux__
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Duel6::Network {
    // Task-only Linux process diagnosis, NOT a gameplay/IPC protocol or telemetry.
    // Values are numeric and allowlisted; never pass payloads, paths or exception text.
    enum class HostDiagnosticEvent {
        Begin = 1, ChildStarted, Status, StopSelected, ObserverException,
        StopRequested, EndRequested, ForceRequested, NativeExit, OwnershipLost,
        ObservationError, BeforeReset, BackendBegin, BackendFailure, BackendOutcome,
        CanonicalIndicatorRejected, PublicationContext, PublicationEventRejected,
        PublicationBudgetRejected, PublicationResultRejected
    };
    enum class HostDiagnosticFailure {
        UnclassifiedRuntime = 1, PresentationRejected, PresentationException,
        InputRejected, MissingMatch, AdvanceRejected, OutcomeObservationRejected,
        TickException, WaitException, CanonicalValidationRejected, CaptureMappingRejected,
        PublicationRejected
    };

    class HostServiceDiagnostics final {
    public:
        static constexpr unsigned MaximumRecords = 128;
        static constexpr const char *Environment = "D6R_HOST_DIAGNOSTICS_FIFO";

        // Caller pre-creates a 0600 FIFO in an owned 0700 directory and drains it
        // into task-local evidence. No file is created/truncated by the game.
        static std::shared_ptr<HostServiceDiagnostics> open(const std::string &path) noexcept {
#ifdef __linux__
            const int saved = errno;
            int directory = -1, descriptor = -1;
            std::shared_ptr<HostServiceDiagnostics> result;
            try {
                const auto slash = path.find_last_of('/');
                if (!path.empty() && path.front() == '/' && path.size() <= 4096
                    && path.find('\0') == std::string::npos && slash != std::string::npos
                    && slash + 1 < path.size()) {
                    const auto parent = path.substr(0, slash);
                    const auto name = path.substr(slash + 1);
                    directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                    struct stat dir{}, before{}, after{};
                    if (directory >= 0 && fstat(directory, &dir) == 0
                        && dir.st_uid == geteuid() && (dir.st_mode & 0777) == 0700
                        && fstatat(directory, name.c_str(), &before, AT_SYMLINK_NOFOLLOW) == 0
                        && S_ISFIFO(before.st_mode) && before.st_uid == geteuid()
                        && (before.st_mode & 0777) == 0600) {
                        // RDWR retains a reader even if the collector disappears:
                        // no SIGPIPE, no signal-mask/handler change, no waiting for a reader.
                        descriptor = openat(directory, name.c_str(), O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
                        if (descriptor >= 0 && fstat(descriptor, &after) == 0
                            && S_ISFIFO(after.st_mode) && before.st_dev == after.st_dev
                            && before.st_ino == after.st_ino && after.st_uid == geteuid()
                            && (after.st_mode & 0777) == 0600) {
                            result.reset(new HostServiceDiagnostics(path));
                            result->descriptor = descriptor;
                            descriptor = -1;
                        }
                    }
                }
            } catch (...) {}
            if (descriptor >= 0) close(descriptor);
            if (directory >= 0) close(directory);
            errno = saved;
            return result;
#else
            (void) path;
            return {};
#endif
        }

        static std::shared_ptr<HostServiceDiagnostics> fromEnvironment() noexcept {
#ifdef __linux__
            try {
                const char *path = std::getenv(Environment);
                if (path && *path) return open(path);
            } catch (...) {}
#endif
            return {};
        }

        ~HostServiceDiagnostics() {
#ifdef __linux__
            if (descriptor >= 0) close(descriptor);
#endif
        }
        const std::string &destination() const noexcept { return path; }

        // One atomic FIFO write (< Linux PIPE_BUF), no retry, allocation, file
        // flush, callback or blocking lock. Backpressure/contention loses evidence,
        // never stalls lifecycle. seq gaps/dropped expose loss; cap bounds output.
        void record(HostDiagnosticEvent event, std::int64_t a = 0, std::int64_t b = 0,
                    std::int64_t c = 0) noexcept {
#ifdef __linux__
            if (busy.test_and_set(std::memory_order_acquire)) {
                dropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if (sequence < MaximumRecords) {
                const int saved = errno;
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                char bytes[256];
                const int count = std::snprintf(bytes, sizeof(bytes),
                        "HSD1 pid=%lld seq=%u ns=%lld event=%d a=%lld b=%lld c=%lld dropped=%u\n",
                        static_cast<long long>(getpid()), ++sequence, static_cast<long long>(ns),
                        static_cast<int>(event), static_cast<long long>(a), static_cast<long long>(b),
                        static_cast<long long>(c), dropped.load(std::memory_order_relaxed));
                if (count <= 0 || count >= static_cast<int>(sizeof(bytes))
                    || write(descriptor, bytes, static_cast<std::size_t>(count)) != count)
                    dropped.fetch_add(1, std::memory_order_relaxed);
                errno = saved;
            }
            busy.clear(std::memory_order_release);
#else
            (void) event; (void) a; (void) b; (void) c;
#endif
        }
        HostServiceDiagnostics(const HostServiceDiagnostics &) = delete;
        HostServiceDiagnostics &operator=(const HostServiceDiagnostics &) = delete;
    private:
        explicit HostServiceDiagnostics(std::string path) : path(std::move(path)) {}
        std::string path;
        int descriptor = -1;
        std::atomic_flag busy = ATOMIC_FLAG_INIT;
        std::atomic<unsigned> dropped{0};
        unsigned sequence = 0;
    };
}
#endif
