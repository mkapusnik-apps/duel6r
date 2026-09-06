#include "HostedServiceChannel.h"

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
#include <signal.h>
#include <sys/prctl.h>
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
        void terminateHostedProcessGroup(int) {
            kill(0, SIGKILL);
            _exit(128 + SIGTERM);
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
        struct sigaction action{};
        action.sa_handler = terminateHostedProcessGroup;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        if (expectedParent > static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max())
            || originalParent != static_cast<pid_t>(expectedParent)
            || sigaction(SIGTERM, &action, nullptr) != 0
            || prctl(PR_SET_PDEATHSIG, SIGTERM) != 0
            || getppid() != static_cast<pid_t>(expectedParent)
            || fcntl(StatusDescriptor, F_GETFD) < 0 || fcntl(ControlDescriptor, F_GETFD) < 0) return nullptr;
        const int flags = fcntl(ControlDescriptor, F_GETFL, 0);
        if (flags < 0 || fcntl(ControlDescriptor, F_SETFL, flags | O_NONBLOCK) != 0) return nullptr;
        channel->statusDescriptor = StatusDescriptor;
        channel->controlDescriptor = ControlDescriptor;
#endif
        return channel;
    }

    HostedServiceChannel::~HostedServiceChannel() {
#ifdef D6R_TRANSPORT_WINDOWS
        if (statusHandle) CloseHandle(static_cast<HANDLE>(statusHandle));
        if (controlHandle) CloseHandle(static_cast<HANDLE>(controlHandle));
#else
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

    bool HostedServiceChannel::stopRequested() noexcept {
        if (stopped || !active()) return stopped;
        std::array<std::uint8_t, Network::HostServiceControlMessageBytes> message{};
#ifdef D6R_TRANSPORT_WINDOWS
        DWORD available = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(controlHandle), nullptr, 0, nullptr, &available, nullptr)) {
            stopped = true;
            return true;
        }
        if (available == 0) return false;
        DWORD readCount = 0;
        if (available != message.size()
            || !ReadFile(static_cast<HANDLE>(controlHandle), message.data(), static_cast<DWORD>(message.size()),
                         &readCount, nullptr) || readCount != message.size()) {
            stopped = true;
            return true;
        }
#else
        const ssize_t readCount = read(controlDescriptor, message.data(), message.size());
        if (readCount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return false;
        if (readCount != static_cast<ssize_t>(message.size())) {
            stopped = true;
            return true;
        }
#endif
        Network::HostServiceCommandCode command{};
        stopped = !Network::decodeHostServiceCommand(message.data(), message.size(), command)
                  || command == Network::HostServiceCommandCode::Stop;
        return stopped;
    }
}
