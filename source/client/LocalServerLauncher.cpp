#include "LocalServerLauncher.h"

#include <sstream>
#include <stdexcept>

namespace Duel6::Client {
    namespace {
        bool isAsciiAlphaNumeric(char chr) {
            return (chr >= '0' && chr <= '9')
                   || (chr >= 'A' && chr <= 'Z')
                   || (chr >= 'a' && chr <= 'z');
        }

        bool isCommandLineSafeUnquoted(char chr) {
            return isAsciiAlphaNumeric(chr)
                   || chr == '-' || chr == '_' || chr == '.' || chr == '/'
                   || chr == ':' || chr == '='
#ifdef _WIN32
                   || chr == '\\'
#endif
                    ;
        }

        std::string quoteArgument(const std::string &argument) {
            if (!argument.empty()) {
                bool safe = true;
                for (char chr: argument) {
                    if (!isCommandLineSafeUnquoted(chr)) {
                        safe = false;
                        break;
                    }
                }
                if (safe) {
                    return argument;
                }
            }

#ifdef _WIN32
            std::string quoted = "\"";
            std::size_t backslashes = 0;
            for (char chr: argument) {
                if (chr == '\\') {
                    ++backslashes;
                    continue;
                }
                if (chr == '"') {
                    quoted.append(backslashes * 2 + 1, '\\');
                    quoted += chr;
                    backslashes = 0;
                    continue;
                }
                quoted.append(backslashes, '\\');
                backslashes = 0;
                quoted += chr;
            }
            quoted.append(backslashes * 2, '\\');
            quoted += '"';
            return quoted;
#else
            std::string quoted = "'";
            for (char chr: argument) {
                if (chr == '\'') {
                    quoted += "'\\''";
                } else {
                    quoted += chr;
                }
            }
            quoted += '\'';
            return quoted;
#endif
        }
    }

    std::vector<std::string> LocalServerLauncher::buildCommand(const ConnectionPlan &plan) const {
        if (!plan.launchesLocalServer) {
            throw std::invalid_argument("Connection plan does not launch a local server");
        }
        if (plan.localServerArguments.empty() || plan.localServerArguments.front().empty()) {
            throw std::invalid_argument("Local server command is missing an executable");
        }
        for (const std::string &argument: plan.localServerArguments) {
            if (argument.compare(0, 8, "--token=") == 0) {
                throw std::invalid_argument("Authentication tokens must not be exposed in process arguments");
            }
        }
        return plan.localServerArguments;
    }

    std::string LocalServerLauncher::buildCommandLine(const ConnectionPlan &plan) const {
        const std::vector<std::string> command = buildCommand(plan);
        std::ostringstream stream;
        for (std::size_t i = 0; i < command.size(); ++i) {
            if (i > 0) {
                stream << ' ';
            }
            stream << quoteArgument(command[i]);
        }
        return stream.str();
    }
}
