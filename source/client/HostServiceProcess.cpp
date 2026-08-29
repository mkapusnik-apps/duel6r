#include "HostServiceSupervisor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Duel6::Client {
    namespace {
        constexpr std::size_t MaximumProcessArguments = 32;
        std::atomic<bool> productionServiceOwned{false};

        class ExclusiveHostServiceChild final : public HostServiceChild {
        public:
            explicit ExclusiveHostServiceChild(std::unique_ptr<HostServiceChild> owned) : owned(std::move(owned)) {}
            ~ExclusiveHostServiceChild() override { owned.reset(); productionServiceOwned.store(false); }
            bool readStatus(HostServiceStatusEvent &event, std::chrono::milliseconds timeout) override {
                return owned->readStatus(event, timeout);
            }
            bool hasExited() override { return owned->hasExited(); }
            void requestStop() override { owned->requestStop(); }
            bool waitForExit(std::chrono::milliseconds timeout) override { return owned->waitForExit(timeout); }
            void forceTerminate() override { owned->forceTerminate(); }
            bool cleanupConfirmed() override { return owned->cleanupConfirmed(); }
            bool waitForCleanup(std::chrono::milliseconds timeout) override {
                return owned->waitForCleanup(timeout);
            }
            HostServiceProcessSnapshot processSnapshot() const noexcept override {
                return owned->processSnapshot();
            }
        private:
            std::unique_ptr<HostServiceChild> owned;
        };

        std::vector<std::string> serverArguments(const HostServiceStartConfig &config) {
            std::vector<std::string> arguments{
                    config.serverExecutable,
                    "--transport",
                    "--host=" + config.endpoint.host,
                    "--port=" + std::to_string(config.endpoint.port),
                    "--resources=" + config.resourcePath,
                    "--local-players=" + std::to_string(config.localPlayers),
                    "--host-service-ipc"
            };
            for (const auto &script: config.enabledGameplayScripts)
                arguments.push_back("--gameplay-script=" + script);
            if (arguments.size() > MaximumProcessArguments)
                throw std::invalid_argument("Hosted service has too many process arguments");
            return arguments;
        }

#ifdef D6R_TRANSPORT_WINDOWS
        std::wstring utf8ToWide(const std::string &value) {
            if (value.empty()) return {};
            const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                                   static_cast<int>(value.size()), nullptr, 0);
            if (count <= 0) return {};
            std::wstring wide(static_cast<std::size_t>(count), L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                    static_cast<int>(value.size()), wide.data(), count) != count) return {};
            return wide;
        }

        std::wstring quoteWindowsArgument(const std::wstring &argument) {
            if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) return argument;
            std::wstring result = L"\"";
            std::size_t slashes = 0;
            for (wchar_t character: argument) {
                if (character == L'\\') { ++slashes; continue; }
                if (character == L'\"') {
                    result.append(slashes * 2u + 1u, L'\\');
                    result += character;
                    slashes = 0;
                    continue;
                }
                result.append(slashes, L'\\');
                slashes = 0;
                result += character;
            }
            result.append(slashes * 2u, L'\\');
            result += L'\"';
            return result;
        }

        class WindowsHostServiceChild final : public HostServiceChild {
        public:
            WindowsHostServiceChild(HANDLE process, HANDLE job, HANDLE statusRead, HANDLE controlWrite)
                    : process(process), job(job), statusRead(statusRead), controlWrite(controlWrite) {}

            ~WindowsHostServiceChild() override {
                forceTerminate();
                waitForExit(std::chrono::milliseconds::zero());
                if (controlWrite) CloseHandle(controlWrite);
                if (statusRead) CloseHandle(statusRead);
                if (process) CloseHandle(process);
                if (job) CloseHandle(job);
            }

            bool readStatus(HostServiceStatusEvent &event, std::chrono::milliseconds timeout) override {
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                do {
                    DWORD available = 0;
                    if (!PeekNamedPipe(statusRead, nullptr, 0, nullptr, &available, nullptr)) return false;
                    if (available > 0) {
                        if (available != Network::HostServiceStatusMessageBytes) {
                            std::array<std::uint8_t, 64> discard{};
                            DWORD discarded = 0;
                            ReadFile(statusRead, discard.data(),
                                     std::min<DWORD>(available, static_cast<DWORD>(discard.size())),
                                     &discarded, nullptr);
                            return false;
                        }
                        std::array<std::uint8_t, Network::HostServiceStatusMessageBytes> message{};
                        DWORD count = 0;
                        if (!ReadFile(statusRead, message.data(), static_cast<DWORD>(message.size()), &count, nullptr)
                            || count != message.size())
                            return false;
                        std::uint64_t timestamp = 0;
                        if (!Network::decodeHostServiceStatus(message.data(), message.size(), event.code, timestamp))
                            return false;
                        event.receivedAt = HostServiceTimePoint(std::chrono::nanoseconds(timestamp));
                        return true;
                    }
                    if (timeout <= std::chrono::milliseconds::zero()) return false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (std::chrono::steady_clock::now() < deadline);
                return false;
            }

            bool hasExited() override { return WaitForSingleObject(process, 0) == WAIT_OBJECT_0; }

            void requestStop() override {
                std::lock_guard<std::mutex> lock(controlMutex);
                if (stopSent || !controlWrite) return;
                const auto message = Network::encodeHostServiceCommand(Network::HostServiceCommandCode::Stop);
                DWORD count = 0;
                WriteFile(controlWrite, message.data(), static_cast<DWORD>(message.size()), &count, nullptr);
                stopSent = true;
            }

            bool waitForExit(std::chrono::milliseconds timeout) override {
                return WaitForSingleObject(process, static_cast<DWORD>(std::max<std::int64_t>(0, timeout.count())))
                       == WAIT_OBJECT_0;
            }

            void forceTerminate() override {
                if (job && !jobEmpty()) TerminateJobObject(job, 3);
            }

            bool cleanupConfirmed() override {
                return hasExited() && jobEmpty();
            }

            bool waitForCleanup(std::chrono::milliseconds timeout) override {
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                do {
                    if (cleanupConfirmed()) return true;
                    if (timeout <= std::chrono::milliseconds::zero()) return false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (std::chrono::steady_clock::now() < deadline);
                return cleanupConfirmed();
            }

        private:
            bool jobEmpty() const {
                if (!job) return false;
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
                return QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &accounting,
                                                 sizeof(accounting), nullptr)
                       && accounting.ActiveProcesses == 0;
            }

            HANDLE process = nullptr;
            HANDLE job = nullptr;
            HANDLE statusRead = nullptr;
            HANDLE controlWrite = nullptr;
            std::mutex controlMutex;
            bool stopSent = false;
        };
#else
        bool moveAboveHostedDescriptors(int &descriptor) {
            if (descriptor > 4) return true;
            const int moved = fcntl(descriptor, F_DUPFD_CLOEXEC, 5);
            if (moved < 0) return false;
            close(descriptor);
            descriptor = moved;
            return true;
        }

        enum class OwnedProcessGroupState { Empty, LiveDescendant, ZombieDescendants, Unknown };

        bool parseProcProcessId(const char *name, pid_t &result) {
            if (!name || !*name) return false;
            std::uint64_t value = 0;
            for (const char *character = name; *character; ++character) {
                if (*character < '0' || *character > '9') return false;
                value = value * 10u + static_cast<unsigned>(*character - '0');
                if (value > static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max())) return false;
            }
            if (value == 0) return false;
            result = static_cast<pid_t>(value);
            return true;
        }

        OwnedProcessGroupState scanOwnedProcessGroup(pid_t group, std::vector<pid_t> &zombies) {
            constexpr std::size_t MaximumProcEntries = 1u << 20u;
            constexpr std::size_t MaximumOwnedZombies = 4096;
            DIR *directory = opendir("/proc");
            if (!directory) return OwnedProcessGroupState::Unknown;
            struct DirectoryGuard {
                DIR *directory;
                ~DirectoryGuard() { closedir(directory); }
            } guard{directory};

            zombies.clear();
            std::size_t visited = 0;
            for (;;) {
                errno = 0;
                dirent *entry = readdir(directory);
                if (!entry) {
                    if (errno != 0) return OwnedProcessGroupState::Unknown;
                    return zombies.empty() ? OwnedProcessGroupState::Empty
                                           : OwnedProcessGroupState::ZombieDescendants;
                }
                pid_t candidate = 0;
                if (!parseProcProcessId(entry->d_name, candidate) || candidate == group) continue;
                if (++visited > MaximumProcEntries) return OwnedProcessGroupState::Unknown;

                std::ifstream stat("/proc/" + std::to_string(candidate) + "/stat");
                std::string value;
                if (!std::getline(stat, value)) continue;
                const auto commandEnd = value.rfind(')');
                if (commandEnd == std::string::npos || commandEnd + 2 >= value.size())
                    return OwnedProcessGroupState::Unknown;
                std::istringstream fields(value.substr(commandEnd + 2));
                char processState = 0;
                long parent = 0;
                long processGroup = 0;
                if (!(fields >> processState >> parent >> processGroup))
                    return OwnedProcessGroupState::Unknown;
                (void) parent;
                if (processGroup != static_cast<long>(group)) continue;
                if (processState != 'Z' && processState != 'X') return OwnedProcessGroupState::LiveDescendant;
                if (zombies.size() >= MaximumOwnedZombies) return OwnedProcessGroupState::Unknown;
                zombies.push_back(candidate);
            }
        }

        class PosixHostServiceChild final : public HostServiceChild {
        public:
            PosixHostServiceChild(pid_t process, int statusRead, int controlWrite)
                    : process(process), statusRead(statusRead), controlWrite(controlWrite) {}

            ~PosixHostServiceChild() override {
                if (!cleanupWasConfirmed) forceTerminate();
                waitForExit(std::chrono::milliseconds::zero());
                if (controlWrite >= 0) close(controlWrite);
                if (statusRead >= 0) close(statusRead);
            }

            bool readStatus(HostServiceStatusEvent &event, std::chrono::milliseconds timeout) override {
                pollfd descriptor{statusRead, POLLIN, 0};
                const int ready = poll(&descriptor, 1, static_cast<int>(std::max<std::int64_t>(0, timeout.count())));
                if (ready <= 0 || !(descriptor.revents & POLLIN)) return false;
                int available = 0;
                if (ioctl(statusRead, FIONREAD, &available) != 0) return false;
                if (available != static_cast<int>(Network::HostServiceStatusMessageBytes)) {
                    std::array<std::uint8_t, 64> discard{};
                    const ssize_t discarded = read(
                            statusRead, discard.data(),
                            std::min<std::size_t>(discard.size(), static_cast<std::size_t>(available)));
                    (void) discarded;
                    return false;
                }
                std::array<std::uint8_t, Network::HostServiceStatusMessageBytes> message{};
                std::size_t offset = 0;
                while (offset < message.size()) {
                    const ssize_t count = read(statusRead, message.data() + offset, message.size() - offset);
                    if (count < 0 && errno == EINTR) continue;
                    if (count <= 0) return false;
                    offset += static_cast<std::size_t>(count);
                }
                std::uint64_t timestamp = 0;
                if (!Network::decodeHostServiceStatus(message.data(), message.size(), event.code, timestamp))
                    return false;
                event.receivedAt = HostServiceTimePoint(std::chrono::nanoseconds(timestamp));
                return true;
            }

            bool hasExited() override {
                std::lock_guard<std::mutex> lock(processMutex);
                return observeLeaderExitLocked();
            }

            void requestStop() override {
                std::lock_guard<std::mutex> lock(controlMutex);
                if (stopSent || controlWrite < 0) return;
                const auto message = Network::encodeHostServiceCommand(Network::HostServiceCommandCode::Stop);
                std::size_t offset = 0;
                while (offset < message.size()) {
                    const ssize_t count = send(controlWrite, message.data() + offset, message.size() - offset,
                                               MSG_NOSIGNAL);
                    if (count < 0 && errno == EINTR) continue;
                    if (count <= 0) break;
                    offset += static_cast<std::size_t>(count);
                }
                stopSent = true;
            }

            bool waitForExit(std::chrono::milliseconds timeout) override {
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                do {
                    if (hasExited()) return true;
                    if (timeout <= std::chrono::milliseconds::zero()) return false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (std::chrono::steady_clock::now() < deadline);
                return hasExited();
            }

            void forceTerminate() override {
                std::lock_guard<std::mutex> lock(processMutex);
                if (treeComplete || leaderReaped || ownershipAnchorLost || process <= 0) return;
                ++forceSignalAttempts;
                kill(-process, SIGKILL);
            }

            bool cleanupConfirmed() override {
                std::lock_guard<std::mutex> lock(processMutex);
                return cleanupConfirmedLocked();
            }

            bool waitForCleanup(std::chrono::milliseconds timeout) override {
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                do {
                    if (cleanupConfirmed()) return true;
                    if (timeout <= std::chrono::milliseconds::zero()) return false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (std::chrono::steady_clock::now() < deadline);
                return cleanupConfirmed();
            }

            HostServiceProcessSnapshot processSnapshot() const noexcept override {
                std::lock_guard<std::mutex> lock(processMutex);
                return {leaderExitObserved, !leaderReaped && !ownershipAnchorLost,
                        treeComplete, forceSignalAttempts};
            }

        private:
            bool observeLeaderExitLocked() {
                if (leaderExitObserved || leaderReaped) return true;
                siginfo_t information{};
                int result = 0;
                do {
                    result = waitid(P_PID, static_cast<id_t>(process), &information,
                                    WEXITED | WNOHANG | WNOWAIT);
                } while (result < 0 && errno == EINTR);
                if (result == 0 && information.si_pid == process) {
                    leaderExitObserved = true;
                    return true;
                }
                if (result < 0 && errno == ECHILD) {
                    // Another reaper violated the ownership contract. Fail closed without signalling a numeric PGID.
                    leaderExitObserved = true;
                    ownershipAnchorLost = true;
                    return true;
                }
                return false;
            }

            bool reapExitedDescendantsLocked() {
                std::vector<pid_t> zombies;
                for (;;) {
                    const auto groupState = scanOwnedProcessGroup(process, zombies);
                    if (groupState == OwnedProcessGroupState::Empty)
                        return ++consecutiveTreeZeroObservations >= 2;
                    consecutiveTreeZeroObservations = 0;
                    if (groupState != OwnedProcessGroupState::ZombieDescendants) return false;
                    for (pid_t zombie: zombies) {
                        pid_t result = 0;
                        do { result = waitpid(zombie, nullptr, WNOHANG); }
                        while (result < 0 && errno == EINTR);
                        if (result != zombie) return false;
                    }
                }
            }

            bool reapLeaderLocked() {
                pid_t result = 0;
                do { result = waitpid(process, nullptr, 0); }
                while (result < 0 && errno == EINTR);
                if (result != process) {
                    ownershipAnchorLost = true;
                    return false;
                }
                leaderReaped = true;
                treeComplete = true;
                cleanupWasConfirmed = true;
                return true;
            }

            bool cleanupConfirmedLocked() {
                if (treeComplete) return true;
                if (!observeLeaderExitLocked() || ownershipAnchorLost) return false;
                if (!reapExitedDescendantsLocked()) return false;
                return reapLeaderLocked();
            }

            pid_t process = 0;
            int statusRead = -1;
            int controlWrite = -1;
            std::mutex controlMutex;
            mutable std::mutex processMutex;
            bool stopSent = false;
            bool leaderExitObserved = false;
            bool leaderReaped = false;
            bool ownershipAnchorLost = false;
            bool treeComplete = false;
            bool cleanupWasConfirmed = false;
            unsigned consecutiveTreeZeroObservations = 0;
            std::uint64_t forceSignalAttempts = 0;
        };
#endif
    }

    std::unique_ptr<HostServiceChild> launchHostServiceProcess(const HostServiceStartConfig &config) {
        bool expected = false;
        if (!productionServiceOwned.compare_exchange_strong(expected, true)) return nullptr;
        struct Reservation {
            bool released = false;
            ~Reservation() { if (!released) productionServiceOwned.store(false); }
        } reservation;
        std::vector<std::string> arguments = serverArguments(config);
        if (!std::filesystem::path(config.serverExecutable).is_absolute()) return nullptr;
#ifdef D6R_TRANSPORT_WINDOWS
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE parentStatusRead = nullptr, childStatusWrite = nullptr;
        HANDLE childControlRead = nullptr, parentControlWrite = nullptr;
        HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        HANDLE nullOutput = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nullInput == INVALID_HANDLE_VALUE || nullOutput == INVALID_HANDLE_VALUE
            || !CreatePipe(&parentStatusRead, &childStatusWrite, &attributes, 0)
            || !CreatePipe(&childControlRead, &parentControlWrite, &attributes, 0)
            || !SetHandleInformation(parentStatusRead, HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(parentControlWrite, HANDLE_FLAG_INHERIT, 0)) {
            if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);
            if (nullOutput != INVALID_HANDLE_VALUE) CloseHandle(nullOutput);
            if (parentStatusRead) CloseHandle(parentStatusRead);
            if (childStatusWrite) CloseHandle(childStatusWrite);
            if (childControlRead) CloseHandle(childControlRead);
            if (parentControlWrite) CloseHandle(parentControlWrite);
            return nullptr;
        }
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            if (job) CloseHandle(job);
            CloseHandle(nullInput); CloseHandle(nullOutput); CloseHandle(parentStatusRead);
            CloseHandle(childStatusWrite); CloseHandle(childControlRead); CloseHandle(parentControlWrite);
            return nullptr;
        }
        arguments.push_back("--host-service-parent=" + std::to_string(GetCurrentProcessId()));
        arguments.push_back("--host-service-status-handle="
                            + std::to_string(reinterpret_cast<std::uintptr_t>(childStatusWrite)));
        arguments.push_back("--host-service-control-handle="
                            + std::to_string(reinterpret_cast<std::uintptr_t>(childControlRead)));
        std::wstring command;
        for (const auto &argument: arguments) {
            const std::wstring wide = utf8ToWide(argument);
            if (wide.empty()) {
                CloseHandle(job); CloseHandle(nullInput); CloseHandle(nullOutput); CloseHandle(parentStatusRead);
                CloseHandle(childStatusWrite); CloseHandle(childControlRead); CloseHandle(parentControlWrite);
                return nullptr;
            }
            if (!command.empty()) command += L' ';
            command += quoteWindowsArgument(wide);
        }
        std::wstring executable = utf8ToWide(config.serverExecutable);
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        SIZE_T attributeBytes = 0;
        InitializeProcThreadAttributeList(nullptr, 2, 0, &attributeBytes);
        std::vector<std::uint8_t> storage(attributeBytes);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = nullInput;
        startup.StartupInfo.hStdOutput = nullOutput;
        startup.StartupInfo.hStdError = nullOutput;
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        HANDLE inherited[] = {nullInput, nullOutput, childStatusWrite, childControlRead};
        const bool attributesReady = InitializeProcThreadAttributeList(startup.lpAttributeList, 2, 0, &attributeBytes)
                                     && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                                                 sizeof(inherited), nullptr, nullptr)
                                     && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                                                 PROC_THREAD_ATTRIBUTE_JOB_LIST, &job,
                                                                 sizeof(job), nullptr, nullptr);
        PROCESS_INFORMATION process{};
        std::array<wchar_t, 2> emptyEnvironment{{L'\0', L'\0'}};
        const BOOL created = attributesReady && !executable.empty()
                             && CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                                               CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT
                                               | EXTENDED_STARTUPINFO_PRESENT,
                                               emptyEnvironment.data(), nullptr,
                                               &startup.StartupInfo, &process);
        if (startup.lpAttributeList) DeleteProcThreadAttributeList(startup.lpAttributeList);
        CloseHandle(nullInput); CloseHandle(nullOutput); CloseHandle(childStatusWrite); CloseHandle(childControlRead);
        if (!created) {
            CloseHandle(job); CloseHandle(parentStatusRead); CloseHandle(parentControlWrite);
            return nullptr;
        }
        CloseHandle(process.hThread);
        reservation.released = true;
        return std::make_unique<ExclusiveHostServiceChild>(
                std::make_unique<WindowsHostServiceChild>(process.hProcess, job, parentStatusRead,
                                                          parentControlWrite));
#else
        if (access(config.serverExecutable.c_str(), X_OK) != 0) return nullptr;
        int subreaper = 0;
        if (prctl(PR_GET_CHILD_SUBREAPER, &subreaper) != 0
            || (!subreaper && prctl(PR_SET_CHILD_SUBREAPER, 1) != 0)) return nullptr;
        int statusPipe[2] = {-1, -1};
        int controlPipe[2] = {-1, -1};
        if (pipe2(statusPipe, O_CLOEXEC) != 0
            || socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, controlPipe) != 0) {
            if (statusPipe[0] >= 0) close(statusPipe[0]);
            if (statusPipe[1] >= 0) close(statusPipe[1]);
            if (controlPipe[0] >= 0) close(controlPipe[0]);
            if (controlPipe[1] >= 0) close(controlPipe[1]);
            return nullptr;
        }
        for (int *descriptor: {&statusPipe[0], &statusPipe[1], &controlPipe[0], &controlPipe[1]}) {
            if (!moveAboveHostedDescriptors(*descriptor)) {
                close(statusPipe[0]); close(statusPipe[1]); close(controlPipe[0]); close(controlPipe[1]);
                return nullptr;
            }
        }
        arguments.push_back("--host-service-parent=" + std::to_string(static_cast<long long>(getpid())));
        std::vector<char *> rawArguments;
        rawArguments.reserve(arguments.size() + 1);
        for (auto &argument: arguments) rawArguments.push_back(argument.data());
        rawArguments.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attributes;
        int status = posix_spawn_file_actions_init(&actions);
        bool actionsInitialized = status == 0;
        int attributeStatus = posix_spawnattr_init(&attributes);
        bool attributesInitialized = attributeStatus == 0;
        if (status == 0) status = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        if (status == 0) status = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        if (status == 0) status = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        if (status == 0) status = posix_spawn_file_actions_adddup2(&actions, statusPipe[1], 3);
        if (status == 0) status = posix_spawn_file_actions_adddup2(&actions, controlPipe[0], 4);
        if (status == 0) status = posix_spawn_file_actions_addclosefrom_np(&actions, 5);
        if (attributeStatus == 0) attributeStatus = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        if (attributeStatus == 0) attributeStatus = posix_spawnattr_setpgroup(&attributes, 0);
        pid_t process = 0;
        char *emptyEnvironment[] = {nullptr};
        const int spawned = status != 0 ? status : attributeStatus != 0 ? attributeStatus
                : posix_spawn(&process, config.serverExecutable.c_str(), &actions, &attributes,
                              rawArguments.data(), emptyEnvironment);
        if (actionsInitialized) posix_spawn_file_actions_destroy(&actions);
        if (attributesInitialized) posix_spawnattr_destroy(&attributes);
        close(statusPipe[1]); close(controlPipe[0]);
        if (spawned != 0) {
            close(statusPipe[0]); close(controlPipe[1]);
            return nullptr;
        }
        reservation.released = true;
        return std::make_unique<ExclusiveHostServiceChild>(
                std::make_unique<PosixHostServiceChild>(process, statusPipe[0], controlPipe[1]));
#endif
    }
}
