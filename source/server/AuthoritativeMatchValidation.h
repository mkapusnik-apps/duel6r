#ifndef DUEL6_SERVER_AUTHORITATIVEMATCHVALIDATION_H
#define DUEL6_SERVER_AUTHORITATIVEMATCHVALIDATION_H

#include <string>
#include <vector>

#include "AuthoritativeMatchTypes.h"
#include "../network/CompatibilityManifest.h"

namespace Duel6::Server::Authoritative {
    struct ValidationResult {
        bool valid = false;
        std::string diagnostic;
    };

    ValidationResult validateMatchConfig(const MatchConfig &config,
                                         const std::vector<PlayerDefinition> &roster);
    ValidationResult validateFrozenContent(const MatchConfig &config,
                                           const Network::GameplayManifest &manifest);
}

#endif
