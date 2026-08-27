#include "ServerConfig.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace Duel6::Server {
    namespace {
        bool startsWith(const std::string &value, const std::string &prefix) {
            return value.compare(0, prefix.size(), prefix) == 0;
        }

        std::string valueAfter(const std::string &argument, const std::string &prefix) {
            if (!startsWith(argument, prefix)) {
                throw std::invalid_argument("Expected argument prefix: " + prefix);
            }
            return argument.substr(prefix.size());
        }

        std::uint16_t parsePort(const std::string &value) {
            if (value.empty() || value[0] == '+' || value[0] == '-') {
                throw std::invalid_argument("Port must be an unsigned integer");
            }
            std::size_t consumed = 0;
            unsigned long port = std::stoul(value, &consumed);
            if (consumed != value.size()) {
                throw std::invalid_argument("Port must be an unsigned integer");
            }
            if (port == 0 || port > 65535) {
                throw std::invalid_argument("Port must be in range 1..65535");
            }
            return static_cast<std::uint16_t>(port);
        }

        std::uint32_t parsePositiveUint32(const std::string &name, const std::string &value) {
            if (value.empty() || value[0] == '+' || value[0] == '-') {
                throw std::invalid_argument(name + " must be an unsigned integer");
            }
            std::size_t consumed = 0;
            unsigned long parsed = std::stoul(value, &consumed);
            if (consumed != value.size() || parsed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument(name + " must be a 32-bit unsigned integer");
            }
            if (parsed == 0) {
                throw std::invalid_argument(name + " must be positive");
            }
            return static_cast<std::uint32_t>(parsed);
        }

        void requireText(const std::string &name, const std::string &value) {
            if (value.empty()) {
                throw std::invalid_argument(name + " must not be empty");
            }
            if (value.size() > Network::MaxProtocolStringBytes) {
                throw std::invalid_argument(name + " is too long");
            }
        }
    }

    ServerConfig parseServerConfig(int argc, char **argv) {
        ServerConfig config;

        for (int i = 1; i < argc; ++i) {
            std::string argument = argv[i];
            if (startsWith(argument, "--host=")) {
                config.listenEndpoint.host = valueAfter(argument, "--host=");
            } else if (startsWith(argument, "--port=")) {
                config.listenEndpoint.port = parsePort(valueAfter(argument, "--port="));
            } else if (startsWith(argument, "--name=")) {
                config.serverName = valueAfter(argument, "--name=");
            } else if (startsWith(argument, "--build-version=")) {
                config.buildVersion = valueAfter(argument, "--build-version=");
            } else if (startsWith(argument, "--resources=")) {
                config.resourcePath = valueAfter(argument, "--resources=");
            } else if (startsWith(argument, "--token=")) {
                throw std::invalid_argument(
                        "Authentication tokens are unsupported; refusing to accept a token in process arguments");
            } else if (startsWith(argument, "--tick-rate=")) {
                config.tickRate = parsePositiveUint32("tick rate", valueAfter(argument, "--tick-rate="));
            } else if (startsWith(argument, "--max-clients=")) {
                config.maxClients = parsePositiveUint32("max clients", valueAfter(argument, "--max-clients="));
            } else if (argument == "--local-only") {
                config.localOnly = true;
                config.listenEndpoint.host = "127.0.0.1";
            } else if (argument == "--help") {
                throw std::invalid_argument(
                        "Usage: duel6r-server [--host=ADDR] [--port=PORT] [--name=NAME] "
                        "[--build-version=VERSION] [--resources=PATH] [--tick-rate=N] "
                        "[--max-clients=N] [--local-only]");
            } else {
                throw std::invalid_argument("Unknown server argument: " + argument);
            }
        }

        requireText("server host", config.listenEndpoint.host);
        requireText("server name", config.serverName);
        requireText("build version", config.buildVersion);
        requireText("resource path", config.resourcePath);
        if (config.localOnly) {
            config.listenEndpoint.host = "127.0.0.1";
        }
        if (config.tickRate > 1000) {
            throw std::invalid_argument("tick rate must not exceed 1000 in this scaffold");
        }
        if (config.maxClients > Network::MaxNetworkPlayers) {
            throw std::invalid_argument("max clients must not exceed 15 in this scaffold");
        }

        return config;
    }
}
