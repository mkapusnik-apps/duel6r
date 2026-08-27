#include <exception>
#include <iostream>

#include "HeadlessServer.h"
#include "ServerConfig.h"

int main(int argc, char **argv) {
    try {
        Duel6::Server::ServerConfig config = Duel6::Server::parseServerConfig(argc, argv);
        Duel6::Server::HeadlessServer server(config);
        return server.run(std::cout);
    } catch (const std::exception &exception) {
        std::cerr << "duel6r-server error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "duel6r-server error: unexpected failure\n";
        return 1;
    }
}
