#ifndef DUEL6_CLIENT_LOCALSERVERLAUNCHER_H
#define DUEL6_CLIENT_LOCALSERVERLAUNCHER_H

#include <string>
#include <vector>

#include "ConnectionPlan.h"

namespace Duel6::Client {
    class LocalServerLauncher {
    public:
        std::vector<std::string> buildCommand(const ConnectionPlan &plan) const;
        std::string buildCommandLine(const ConnectionPlan &plan) const;
    };
}

#endif
