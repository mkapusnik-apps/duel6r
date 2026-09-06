#ifndef DUEL6_SERVER_AUTHORITATIVEMATCHCLI_H
#define DUEL6_SERVER_AUTHORITATIVEMATCHCLI_H

#include <functional>
#include <iosfwd>

#include "AuthoritativeMatch.h"

namespace Duel6::Server::Authoritative {
    struct AuthoritativeMatchCliDependencies {
        std::function<bool()> stopRequested;
        std::function<bool()> reportReady;
        std::function<MatchRuntimeDependencies(const MatchConfig &, const std::vector<PlayerDefinition> &,
                                                const Network::ManifestBuildResult &)> runtimeFactory;
    };

    bool authoritativeMatchRequested(int argc, char **argv);
    int runAuthoritativeMatchCli(int argc, char **argv, std::istream &input, std::ostream &output,
                                 AuthoritativeMatchCliDependencies dependencies = {});
}

#endif
