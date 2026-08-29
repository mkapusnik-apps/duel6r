#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "source/network/HostServiceControlProtocol.h"
#include "source/server/HostedServiceChannel.h"

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
extern char **environ;
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::string modeFromArguments(int count, char **arguments) {
    constexpr const char *prefix = "--resources=";
    for (int index = 1; index < count; ++index) {
        const std::string argument = arguments[index] ? arguments[index] : "";
        if (argument.compare(0, std::char_traits<char>::length(prefix), prefix) == 0)
            return std::filesystem::path(argument.substr(std::char_traits<char>::length(prefix))).filename().string();
    }
    return {};
}

std::string gameplayScriptFromArguments(int count, char **arguments) {
    constexpr const char *prefix = "--gameplay-script=";
    for (int index = 1; index < count; ++index) {
        const std::string argument = arguments[index] ? arguments[index] : "";
        if (argument.compare(0, std::char_traits<char>::length(prefix), prefix) == 0)
            return argument.substr(std::char_traits<char>::length(prefix));
    }
    return {};
}

bool publishMarker(const std::string &pathValue, const std::string &contents) {
    const std::filesystem::path target(pathValue);
#ifdef _WIN32
    const auto processId = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto processId = static_cast<unsigned long long>(getpid());
#endif
    const std::filesystem::path temporary = target.string() + ".tmp-" + std::to_string(processId);
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.flush();
    if (!stream) {
        stream.close();
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}
}

int main(int count, char **arguments) {
#ifdef _WIN32
    for (int index = 1; index < count; ++index) {
        if (arguments[index] && std::string(arguments[index]) == "--descendant") {
            for (;;) Sleep(1000);
        }
    }
#endif
    const auto channel = Duel6::Server::HostedServiceChannel::fromCommandLine(count, arguments);
    if (!channel || !channel->active()) return 70;
    const std::string mode = modeFromArguments(count, arguments);
    if (mode == "early") return 71;
    if (mode == "port-unavailable") {
        channel->send(Duel6::Network::HostServiceStatusCode::PortUnavailable);
        return 72;
    }
    if (mode == "start-failed") {
        channel->send(Duel6::Network::HostServiceStatusCode::StartFailed);
        return 73;
    }
    if (mode == "manifest-invalid") {
        channel->send(Duel6::Network::HostServiceStatusCode::HostManifestInvalid);
        return 74;
    }
    if (mode == "unexpected-stop") {
        if (!channel->send(Duel6::Network::HostServiceStatusCode::Ready)) return 75;
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
        return 76;
    }
#ifndef _WIN32
    if (mode == "secure-ready") {
        if (environ && environ[0]) {
            channel->send(Duel6::Network::HostServiceStatusCode::StartFailed);
            return 78;
        }
        for (int descriptor = 5; descriptor < 256; ++descriptor) {
            errno = 0;
            if (fcntl(descriptor, F_GETFD) >= 0 || errno != EBADF) {
                channel->send(Duel6::Network::HostServiceStatusCode::StartFailed);
                return 79;
            }
        }
        if (!channel->send(Duel6::Network::HostServiceStatusCode::Ready)) return 80;
    }
    if (mode == "tree") {
        const std::string pidFile = gameplayScriptFromArguments(count, arguments);
        const pid_t descendant = fork();
        if (descendant < 0) return 81;
        if (descendant == 0) {
            for (;;) pause();
        }
        if (!publishMarker(pidFile, std::to_string(descendant) + "\n")) return 82;
    }
#else
    if (mode == "tree") {
        const std::string pidFile = gameplayScriptFromArguments(count, arguments);
        std::array<wchar_t, 32768> executable{};
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size()) return 81;
        std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" --descendant";
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION descendant{};
        if (!CreateProcessW(executable.data(), mutableCommand.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &descendant)) return 81;
        CloseHandle(descendant.hThread);
        const bool published = publishMarker(pidFile, std::to_string(descendant.dwProcessId) + "\n");
        CloseHandle(descendant.hProcess);
        if (!published) return 82;
    }
#endif
    if (mode == "parent-mismatch") {
        const std::string markerPath = gameplayScriptFromArguments(count, arguments);
        std::vector<std::string> changed;
        changed.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            std::string argument = arguments[index] ? arguments[index] : "";
            if (argument.compare(0, 22, "--host-service-parent=") == 0)
                argument = "--host-service-parent=1";
            changed.push_back(std::move(argument));
        }
        std::vector<char *> raw;
        for (auto &argument: changed) raw.push_back(argument.data());
        const auto mismatched = Duel6::Server::HostedServiceChannel::fromCommandLine(count, raw.data());
        if (!publishMarker(markerPath, mismatched ? "accepted\n" : "rejected\n") || mismatched) return 83;
    }
    if (mode == "ready" && !channel->send(Duel6::Network::HostServiceStatusCode::Ready)) return 77;

    // "timeout" deliberately never reports status. All long-lived modes cooperate with Stop.
    for (;;) {
        if (channel->stopRequested()) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
