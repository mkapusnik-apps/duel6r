#include "HostServiceSupervisor.h"

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
    volatile std::sig_atomic_t exitRequested = 0;

    void requestExit(int) { exitRequested = 1; }

    bool startsWith(const std::string &value, const char *prefix) {
        return value.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
    }

    std::uint32_t parsePositive(const std::string &value, std::uint32_t maximum) {
        if (value.empty() || value.size() > 10 || value[0] == '+' || value[0] == '-')
            throw std::invalid_argument("invalid numeric argument");
        std::uint64_t result = 0;
        for (char character: value) {
            if (character < '0' || character > '9') throw std::invalid_argument("invalid numeric argument");
            result = result * 10u + static_cast<std::uint64_t>(character - '0');
            if (result > maximum) throw std::invalid_argument("numeric argument exceeds its bound");
        }
        if (result == 0) throw std::invalid_argument("numeric argument must be positive");
        return static_cast<std::uint32_t>(result);
    }

    std::filesystem::path executablePath() {
#ifdef D6R_TRANSPORT_WINDOWS
        std::array<wchar_t, 32768> path{};
        const DWORD count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (count == 0 || count >= path.size()) return {};
        return std::filesystem::path(std::wstring(path.data(), count));
#else
        std::array<char, 4096> path{};
        const ssize_t count = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (count <= 0 || static_cast<std::size_t>(count) >= path.size()) return {};
        return std::filesystem::path(std::string(path.data(), static_cast<std::size_t>(count)));
#endif
    }

    std::string siblingServerPath() {
        const auto self = executablePath();
        if (self.empty() || !self.is_absolute()) return {};
#ifdef D6R_TRANSPORT_WINDOWS
        return (self.parent_path() / "duel6r-server.exe").u8string();
#else
        return (self.parent_path() / "duel6r-server").string();
#endif
    }
}

int main(int argumentCount, char **arguments) {
    try {
        Duel6::Client::HostServiceStartConfig config;
        config.serverExecutable = siblingServerPath();
        config.endpoint = {"127.0.0.1", Duel6::Network::DefaultServerPort};
        config.resourcePath = "resources";
        bool cancelImmediately = false;
        bool exitAfterReady = false;
        bool endAfterReady = false;
        for (int index = 1; index < argumentCount; ++index) {
            const std::string argument = arguments[index] ? arguments[index] : "";
            if (startsWith(argument, "--server=")) config.serverExecutable = argument.substr(9);
            else if (startsWith(argument, "--host=")) config.endpoint.host = argument.substr(7);
            else if (startsWith(argument, "--port="))
                config.endpoint.port = static_cast<std::uint16_t>(parsePositive(argument.substr(7), 65535));
            else if (startsWith(argument, "--resources=")) config.resourcePath = argument.substr(12);
            else if (startsWith(argument, "--local-players="))
                config.localPlayers = static_cast<std::uint8_t>(parsePositive(argument.substr(16), 15));
            else if (startsWith(argument, "--gameplay-script="))
                config.enabledGameplayScripts.push_back(argument.substr(18));
            else if (argument == "--cancel-immediately") cancelImmediately = true;
            else if (argument == "--exit-after-ready") exitAfterReady = true;
            else if (argument == "--end-after-ready") endAfterReady = true;
            else throw std::invalid_argument("unsupported supervisor argument");
        }
#ifdef D6R_TRANSPORT_WINDOWS
        config.serverExecutable = std::filesystem::absolute(config.serverExecutable).lexically_normal().u8string();
        config.resourcePath = std::filesystem::absolute(config.resourcePath).lexically_normal().u8string();
#else
        config.serverExecutable = std::filesystem::absolute(config.serverExecutable).lexically_normal().string();
        config.resourcePath = std::filesystem::absolute(config.resourcePath).lexically_normal().string();
#endif

        std::signal(SIGINT, requestExit);
        std::signal(SIGTERM, requestExit);
        Duel6::Client::HostServiceDependencies dependencies;
        dependencies.intentionalEndHandoff = [](const char *) {
            std::cout << "intentional-host-end\n";
            std::cout.flush();
        };
        Duel6::Client::HostServiceSupervisor supervisor(std::move(dependencies));
        if (!supervisor.start(config)) {
            std::cout << "host-service-start-failed\nHosted session could not start.\n";
            return 2;
        }
        if (cancelImmediately) supervisor.cancelStartup();

        bool activeReported = false;
        for (;;) {
            const auto current = supervisor.snapshot();
            if (current.state == Duel6::Client::HostServiceState::Active && !activeReported) {
                activeReported = true;
                std::cout << "host-service-active\n"
                          << "scaffold only: no graphical network UI or playable network session is implemented.\n";
                std::cout.flush();
                if (endAfterReady) supervisor.endSession();
                else if (exitAfterReady) supervisor.applicationExit();
            }
            if (current.state == Duel6::Client::HostServiceState::StartupFailed
                || current.state == Duel6::Client::HostServiceState::SessionFailed) {
                std::cout << Duel6::Client::hostServiceOutcomeIdentifier(current.outcome) << '\n'
                          << Duel6::Client::hostServiceOutcomeCopy(current.outcome) << '\n';
                return 2;
            }
            if (current.state == Duel6::Client::HostServiceState::NoService && (cancelImmediately || endAfterReady))
                return 0;
            if (current.state == Duel6::Client::HostServiceState::ApplicationExit && current.cleanupComplete)
                return 0;
            if (exitRequested) supervisor.applicationExit();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } catch (...) {
        std::cout << "host-service-start-failed\nHosted session could not start.\n";
        return 2;
    }
}
