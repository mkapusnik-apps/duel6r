#include <exception>
#include <iostream>
#include <utility>

#include "HeadlessServer.h"
#include "HostedServiceChannel.h"
#include "ServerConfig.h"

int main(int argc, char **argv) {
    const auto hostedChannel = Duel6::Server::HostedServiceChannel::fromCommandLine(argc, argv);
    try {
        Duel6::Server::ServerConfig config = Duel6::Server::parseServerConfig(argc, argv);
        if (config.hostedServiceIpc && (!hostedChannel || !hostedChannel->active())) return 1;
        Duel6::Server::AdmissionRuntimeDependencies dependencies;
        if (hostedChannel) {
            dependencies.cancelled = [hostedChannel] { return hostedChannel->stopRequested(); };
            dependencies.hostedServiceStatus = [hostedChannel](Duel6::Network::HostServiceStatusCode status) {
                return hostedChannel->send(status);
            };
        }
        Duel6::Server::HeadlessServer server(config, std::move(dependencies));
        return server.run(std::cout);
    } catch (const std::exception &exception) {
        if (hostedChannel) hostedChannel->send(Duel6::Network::HostServiceStatusCode::StartFailed);
        std::cerr << "duel6r-server error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        if (hostedChannel) hostedChannel->send(Duel6::Network::HostServiceStatusCode::StartFailed);
        std::cerr << "duel6r-server error: unexpected failure\n";
        return 1;
    }
}
