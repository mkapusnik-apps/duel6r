#include "LocalServerLauncher.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace Duel6::Client {
    namespace {
        bool isShellSafeUnquoted(char chr) {
            unsigned char value = static_cast<unsigned char>(chr);
            return std::isalnum(value) || chr == '-' || chr == '_' || chr == '.' || chr == '/' || chr == ':' || chr == '=';
        }

        std::string quoteArgument(const std::string &argument) {
            if (!argument.empty()) {
                bool safe = true;
                for (char chr: argument) {
                    if (!isShellSafeUnquoted(chr)) {
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
            for (char chr: argument) {
                if (chr == '"' || chr == '\\') {
                    quoted += '\\';
                } else if (chr == '%') {
                    quoted += '%';
                } else if (chr == '^' || chr == '&' || chr == '|' || chr == '<' || chr == '>' || chr == '!') {
                    quoted += '^';
                }
                quoted += chr;
            }
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
