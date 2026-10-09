#ifndef DUEL6_CLIENT_DIRECTORYORIGIN_H
#define DUEL6_CLIENT_DIRECTORYORIGIN_H

#include <string>

namespace Duel6::Client {
    // A null override selects the compiled default; a present invalid override
    // disables directory access. No failed request may select another origin.
    std::string directoryOrigin(const char *overrideUrl, const char *allowHttp);
}

#endif
