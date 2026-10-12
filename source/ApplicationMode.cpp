#include "ApplicationMode.h"
#include <string_view>

namespace Duel6 {
    ApplicationArguments selectApplicationMode(int argc, char **argv) {
        ApplicationArguments result;
        if (argc < 1 || !argv || !argv[0]) { result.mode = ApplicationMode::Invalid; return result; }
        unsigned selectors = 0;
        bool help = false, option = false;
        result.arguments.push_back(argv[0]);
        for (int i = 1; i < argc; ++i) {
            if (!argv[i]) { result.mode = ApplicationMode::Invalid; return result; }
            const std::string_view value(argv[i]);
            if (value == "--server") { ++selectors; continue; }
            if (value.substr(0, 8) == "--server") { result.mode = ApplicationMode::Invalid; return result; }
            if (value == "--help") { help = true; continue; }
            if (value.substr(0, 2) == "--") option = true;
            result.arguments.push_back(argv[i]);
        }
        if (selectors > 1 || (!selectors && option)) result.mode = ApplicationMode::Invalid;
        else if (selectors) result.mode = help ? ApplicationMode::ServerHelp : ApplicationMode::Server;
        else if (help) result.mode = ApplicationMode::Help;
        return result;
    }
}
