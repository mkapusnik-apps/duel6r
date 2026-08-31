#ifndef DUEL6_SERVER_AUTHORITATIVEMATCHCLI_H
#define DUEL6_SERVER_AUTHORITATIVEMATCHCLI_H

#include <functional>
#include <iosfwd>

namespace Duel6::Server::Authoritative {
    struct AuthoritativeMatchCliDependencies {
        std::function<bool()> stopRequested;
        std::function<bool()> reportReady;
    };

    bool authoritativeMatchRequested(int argc, char **argv);
    int runAuthoritativeMatchCli(int argc, char **argv, std::istream &input, std::ostream &output,
                                 AuthoritativeMatchCliDependencies dependencies = {});
}

#endif
