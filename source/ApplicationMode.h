#ifndef DUEL6_APPLICATIONMODE_H
#define DUEL6_APPLICATIONMODE_H

#include <vector>

namespace Duel6 {
    enum class ApplicationMode { Client, Server, Help, ServerHelp, Invalid };
    struct ApplicationArguments {
        ApplicationMode mode = ApplicationMode::Client;
        std::vector<char *> arguments;
    };
    // Selection is pure: no platform, configuration, resource or secret access.
    ApplicationArguments selectApplicationMode(int argc, char **argv);
}
#endif
