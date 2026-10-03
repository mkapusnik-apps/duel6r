#include "HostedServiceChannel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Duel6::Server {
    namespace {
        bool startsWith(const std::string &value, const char *prefix) {
            return value.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
        }

        bool parseUnsigned(const std::string &value, std::uint64_t &result) {
            if (value.empty() || value.size() > 20) return false;
            result = 0;
            for (char character: value) {
                if (character < '0' || character > '9') return false;
                const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
                if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) return false;
                result = result * 10u + digit;
            }
            return result != 0;
        }

#ifdef D6R_TRANSPORT_WINDOWS
        bool currentProcessHasParent(std::uint64_t expectedParent) {
            if (expectedParent > std::numeric_limits<DWORD>::max()) return false;
            const DWORD current = GetCurrentProcessId();
            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot == INVALID_HANDLE_VALUE) return false;
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            bool matches = false;
            if (Process32FirstW(snapshot, &entry)) {
                do {
                    if (entry.th32ProcessID == current) {
                        matches = entry.th32ParentProcessID == static_cast<DWORD>(expectedParent);
                        break;
                    }
                } while (Process32NextW(snapshot, &entry));
            }
            CloseHandle(snapshot);
            return matches;
        }

        bool writeExact(HANDLE handle, const std::uint8_t *data, std::size_t size) {
            while (size > 0) {
                DWORD written = 0;
                if (!WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr) || written == 0)
                    return false;
                data += written;
                size -= written;
            }
            return true;
        }
#else
        // Reserved only by an authenticated Linux hosted service. Application
        // SIGTERM handling remains cooperative; it cannot replace this guard.
        constexpr int HostedParentDeathSignal = SIGUSR2;
        volatile sig_atomic_t authenticatedGroupLeader = 0;

        void clearForkedGroupOwnership() { authenticatedGroupLeader = 0; }

        bool ownsCurrentHostedGroup(std::int64_t leader) {
            // The caller itself is the live ownership anchor, never a cached
            // numeric PGID. Forked descendants lose the inherited token.
            return leader > 0 && authenticatedGroupLeader == leader
                   && getpid() == leader && getpgrp() == leader;
        }

        bool isOwningParentControl(int descriptor, std::int64_t parent) {
            int type = 0;
            socklen_t size = sizeof(type);
            if (getsockopt(descriptor, SOL_SOCKET, SO_TYPE, &type, &size) != 0 || type != SOCK_STREAM)
                return false;
            ucred peer{};
            size = sizeof(peer);
            return getsockopt(descriptor, SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0
                   && size == sizeof(peer) && peer.pid == parent && peer.uid == geteuid();
        }

        void terminateHostedProcessGroup(int signal) {
            const pid_t leader = getpid();
            // Pin the target to this still-live PID/PGID anchor. A concurrent
            // group move must never redirect kill(0) to an unrelated group.
            if (ownsCurrentHostedGroup(leader)) kill(-leader, SIGKILL);
            _exit(128 + signal);
        }

        bool writeExact(int descriptor, const std::uint8_t *data, std::size_t size) {
            while (size > 0) {
                const ssize_t written = write(descriptor, data, size);
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) return false;
                data += written;
                size -= static_cast<std::size_t>(written);
            }
            return true;
        }
#endif
    }

    HostedServiceChannel::HostedServiceChannel() = default;

    std::shared_ptr<HostedServiceChannel> HostedServiceChannel::fromCommandLine(
            int argumentCount, char **arguments) {
        bool requested = false;
        std::uint64_t expectedParent = 0;
#ifdef D6R_TRANSPORT_WINDOWS
        std::uint64_t statusValue = 0;
        std::uint64_t controlValue = 0;
#endif
        for (int index = 1; index < argumentCount; ++index) {
            const std::string argument = arguments[index] ? arguments[index] : "";
            if (argument == "--host-service-ipc") requested = true;
            else if (startsWith(argument, "--host-service-parent=")) {
                if (!parseUnsigned(argument.substr(22), expectedParent)) return nullptr;
            }
#ifdef D6R_TRANSPORT_WINDOWS
            else if (startsWith(argument, "--host-service-status-handle=")) {
                if (!parseUnsigned(argument.substr(29), statusValue)) return nullptr;
            } else if (startsWith(argument, "--host-service-control-handle=")) {
                if (!parseUnsigned(argument.substr(30), controlValue)) return nullptr;
            }
#endif
        }
        if (!requested || expectedParent == 0) return nullptr;

        auto channel = std::shared_ptr<HostedServiceChannel>(new HostedServiceChannel());
#ifdef D6R_TRANSPORT_WINDOWS
        channel->statusHandle = reinterpret_cast<void *>(static_cast<std::uintptr_t>(statusValue));
        channel->controlHandle = reinterpret_cast<void *>(static_cast<std::uintptr_t>(controlValue));
        BOOL inJob = FALSE;
        if (channel->statusHandle == nullptr || channel->controlHandle == nullptr
            || GetFileType(static_cast<HANDLE>(channel->statusHandle)) != FILE_TYPE_PIPE
            || GetFileType(static_cast<HANDLE>(channel->controlHandle)) != FILE_TYPE_PIPE
            || !IsProcessInJob(GetCurrentProcess(), nullptr, &inJob) || !inJob
            || !currentProcessHasParent(expectedParent)) return nullptr;
#else
        constexpr int StatusDescriptor = 3;
        constexpr int ControlDescriptor = 4;
        const pid_t originalParent = getppid();
        const pid_t leader = getpid();
        struct stat status{};
        const int statusFlags = fcntl(StatusDescriptor, F_GETFL, 0);
        if (expectedParent > static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max())
            || originalParent != static_cast<pid_t>(expectedParent) || getpgrp() != leader
            || fstat(StatusDescriptor, &status) != 0 || !S_ISFIFO(status.st_mode)
            || statusFlags < 0 || (statusFlags & O_ACCMODE) != O_WRONLY
            || !isOwningParentControl(ControlDescriptor, originalParent)) return nullptr;
        static const bool forkGuardInstalled = pthread_atfork(nullptr, nullptr, clearForkedGroupOwnership) == 0;
        if (!forkGuardInstalled) return nullptr;
        struct sigaction previous{};
        if (sigaction(HostedParentDeathSignal, nullptr, &previous) != 0
            || (previous.sa_flags & SA_SIGINFO) != 0
            || (previous.sa_handler != SIG_DFL && previous.sa_handler != terminateHostedProcessGroup)
            || (previous.sa_handler == terminateHostedProcessGroup && !ownsCurrentHostedGroup(leader))) return nullptr;
        const int flags = fcntl(ControlDescriptor, F_GETFL, 0);
        if (flags < 0 || fcntl(ControlDescriptor, F_SETFL, flags | O_NONBLOCK) != 0) return nullptr;
        sigset_t guardSignal;
        sigemptyset(&guardSignal);
        sigaddset(&guardSignal, HostedParentDeathSignal);
        // This mask is thread-local. Other workers may remain unblocked: the
        // authenticated token is valid before publishing the process handler.
        if (pthread_sigmask(SIG_BLOCK, &guardSignal, nullptr) != 0) return nullptr;
        authenticatedGroupLeader = leader;
        struct sigaction action{};
        action.sa_handler = terminateHostedProcessGroup;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        if (sigaction(HostedParentDeathSignal, &action, nullptr) != 0
            || prctl(PR_SET_PDEATHSIG, HostedParentDeathSignal) != 0
            || getppid() != static_cast<pid_t>(expectedParent)
            || fcntl(StatusDescriptor, F_GETFD) < 0 || fcntl(ControlDescriptor, F_GETFD) < 0)
            terminateHostedProcessGroup(HostedParentDeathSignal);
        channel->statusDescriptor = StatusDescriptor;
        channel->controlDescriptor = ControlDescriptor;
        channel->ownedLeader = leader;
        channel->owningParent = originalParent;
        // Never restore an inherited blocked guard mask. Pending parent-death
        // delivery must stay effective through channel destruction/final exit.
        if (pthread_sigmask(SIG_UNBLOCK, &guardSignal, nullptr) != 0)
            terminateHostedProcessGroup(HostedParentDeathSignal);
#endif
        return channel;
    }

    HostedServiceChannel::~HostedServiceChannel() {
#ifdef D6R_TRANSPORT_WINDOWS
        if (statusHandle) CloseHandle(static_cast<HANDLE>(statusHandle));
        if (controlHandle) CloseHandle(static_cast<HANDLE>(controlHandle));
#else
        // The authenticated SIGUSR2 handler/token are process-lifetime state,
        // not channel state. Do not clear, disarm or restore their disposition
        // here: parent loss after this destructor must still kill the owned tree.
        if (statusDescriptor >= 0) close(statusDescriptor);
        if (controlDescriptor >= 0) close(controlDescriptor);
#endif
    }

    bool HostedServiceChannel::active() const noexcept {
#ifdef D6R_TRANSPORT_WINDOWS
        return statusHandle != nullptr && controlHandle != nullptr;
#else
        return statusDescriptor >= 0 && controlDescriptor >= 0;
#endif
    }

    bool HostedServiceChannel::send(Network::HostServiceStatusCode status) noexcept {
        if (!active()) return false;
        const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto timestamp = count <= 0 ? 0u : static_cast<std::uint64_t>(count);
        const auto message = Network::encodeHostServiceStatus(status, timestamp);
#ifdef D6R_TRANSPORT_WINDOWS
        return writeExact(static_cast<HANDLE>(statusHandle), message.data(), message.size());
#else
        return writeExact(statusDescriptor, message.data(), message.size());
#endif
    }

    bool HostedServiceChannel::sendSessionPayload(const std::vector<std::uint8_t> &payload) noexcept {
        if (!active()) return false;
        std::vector<std::uint8_t> message;
        try { message = Network::encodeHostServicePayload(payload); } catch (...) { return false; }
#ifdef D6R_TRANSPORT_WINDOWS
        return writeExact(static_cast<HANDLE>(statusHandle), message.data(), message.size());
#else
        return writeExact(statusDescriptor, message.data(), message.size());
#endif
    }

    void HostedServiceChannel::pollCommand() noexcept {
        if (stopped || intentionalEnd || !active()) return;
        std::array<std::uint8_t, 256> received{};
#ifdef D6R_TRANSPORT_WINDOWS
        while (!stopped && !intentionalEnd) {
            DWORD available = 0;
            if (!PeekNamedPipe(static_cast<HANDLE>(controlHandle), nullptr, 0, nullptr, &available, nullptr)) {
                stopped = true;
                return;
            }
            if (available == 0) break;
            const DWORD requested = (std::min)(available, static_cast<DWORD>(received.size()));
            DWORD readCount = 0;
            if (!ReadFile(static_cast<HANDLE>(controlHandle), received.data(), requested,
                          &readCount, nullptr) || readCount == 0) {
                stopped = true;
                return;
            }
            try {
                commandBytes.insert(commandBytes.end(), received.begin(),
                                    received.begin() + static_cast<std::ptrdiff_t>(readCount));
            } catch (...) {
                stopped = true;
                return;
            }
            decodeCommands();
        }
#else
        while (!stopped && !intentionalEnd) {
            const ssize_t readCount = read(controlDescriptor, received.data(), received.size());
            if (readCount < 0 && errno == EINTR) continue;
            if (readCount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (readCount <= 0) {
                // Only an authenticated owned channel may kill its live group.
                // This path cannot run after decoded Stop/End, which already
                // ended polling and retain their existing graceful cleanup.
                if (ownsCurrentHostedGroup(ownedLeader)
                    && isOwningParentControl(controlDescriptor, owningParent))
                    terminateHostedProcessGroup(HostedParentDeathSignal);
                stopped = true;
                return;
            }
            try {
                commandBytes.insert(commandBytes.end(), received.begin(),
                                    received.begin() + static_cast<std::ptrdiff_t>(readCount));
            } catch (...) {
                stopped = true;
                return;
            }
            decodeCommands();
        }
#endif
    }

    void HostedServiceChannel::decodeCommands() noexcept {
        std::size_t consumed = 0;
        while (!stopped && !intentionalEnd && commandBytes.size() - consumed >= 4) {
            const auto *message = commandBytes.data() + consumed;
            const std::uint32_t magic = (static_cast<std::uint32_t>(message[0]) << 24u)
                                        | (static_cast<std::uint32_t>(message[1]) << 16u)
                                        | (static_cast<std::uint32_t>(message[2]) << 8u)
                                        | static_cast<std::uint32_t>(message[3]);
            if (magic == Network::HostServicePayloadMagic) {
                if (commandBytes.size() - consumed < Network::HostServicePayloadHeaderBytes) break;
                std::size_t payloadBytes = 0;
                if (!Network::decodeHostServicePayloadHeader(
                        message, Network::HostServicePayloadHeaderBytes, payloadBytes)) {
                    stopped = true; break;
                }
                const auto messageBytes = Network::HostServicePayloadHeaderBytes + payloadBytes;
                if (commandBytes.size() - consumed < messageBytes) break;
                try {
                    sessionPayloads.emplace_back(
                            commandBytes.begin() + static_cast<std::ptrdiff_t>(consumed
                                    + Network::HostServicePayloadHeaderBytes),
                            commandBytes.begin() + static_cast<std::ptrdiff_t>(consumed + messageBytes));
                } catch (...) { stopped = true; break; }
                consumed += messageBytes;
                continue;
            }
            if (magic != Network::HostServiceControlMagic
                || commandBytes.size() - consumed < Network::HostServiceControlMessageBytes) break;
            Network::HostServiceCommandCode command{};
            if (!Network::decodeHostServiceCommand(
                    message, Network::HostServiceControlMessageBytes, command)) {
                stopped = true;
                intentionalEnd = false;
                commandBytes.clear();
                return;
            }
            consumed += Network::HostServiceControlMessageBytes;
            if (command == Network::HostServiceCommandCode::Stop) stopped = true;
            else if (command == Network::HostServiceCommandCode::EndSession) intentionalEnd = true;
            else readinessChange = command == Network::HostServiceCommandCode::Ready;
        }
        if (stopped || intentionalEnd) commandBytes.clear();
        else if (consumed != 0)
            commandBytes.erase(commandBytes.begin(), commandBytes.begin() + static_cast<std::ptrdiff_t>(consumed));
    }

    bool HostedServiceChannel::stopRequested() noexcept {
        pollCommand();
        return stopped;
    }

    bool HostedServiceChannel::intentionalEndRequested() noexcept {
        pollCommand();
        return intentionalEnd;
    }

    std::optional<bool> HostedServiceChannel::takeReadinessChange() noexcept {
        pollCommand();
        auto result = readinessChange;
        readinessChange.reset();
        return result;
    }

    std::optional<std::vector<std::uint8_t>> HostedServiceChannel::takeSessionPayload() noexcept {
        pollCommand();
        if (sessionPayloads.empty()) return std::nullopt;
        auto result = std::move(sessionPayloads.front());
        sessionPayloads.erase(sessionPayloads.begin());
        return result;
    }
}
