#include "DirectoryOrigin.h"

namespace Duel6::Client {
    std::string directoryOrigin(const char *overrideUrl, const char *allowHttp) {
        std::string base(overrideUrl ? overrideUrl : D6R_DIRECTORY_DEFAULT_URL);
        const bool local = allowHttp && std::string(allowHttp) == "1"
            && base.rfind("http://127.0.0.1:", 0) == 0;
        if (base.size() > 512 || base.find_first_of("@?#\r\n") != std::string::npos
            || (base.rfind("https://", 0) != 0 && !local)) return {};
        while (!base.empty() && base.back() == '/') base.pop_back();
        return base;
    }
}
