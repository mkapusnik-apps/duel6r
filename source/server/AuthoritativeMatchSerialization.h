#ifndef DUEL6_SERVER_AUTHORITATIVEMATCHSERIALIZATION_H
#define DUEL6_SERVER_AUTHORITATIVEMATCHSERIALIZATION_H

#include <optional>
#include <string>

#include "AuthoritativeMatchTypes.h"

namespace Duel6::Server::Authoritative {
    std::optional<std::string> serializeSessionResult(const SessionResult &result);
}

#endif
