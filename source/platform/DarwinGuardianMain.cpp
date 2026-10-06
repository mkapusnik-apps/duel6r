#include "DarwinProcess.h"
#include <charconv>
#include <cstring>
#include <limits>

int main(int argc, char **argv) {
    // Private local entry, not a server deployment interface. Three decimal
    // non-secret parameters: expected parent, liveness FD, status socket FD.
    if (argc < 5) return 2;
    int values[3]{};
    for (int index = 0; index < 3; ++index) {
        const auto end = argv[index + 1] + std::strlen(argv[index + 1]);
        const auto result = std::from_chars(argv[index + 1], end, values[index]);
        if (result.ec != std::errc{} || result.ptr != end || values[index] < (index ? 3 : 2)) return 2;
    }
    if (values[1] == values[2]) return 2;
    std::vector<std::string> worker;
    for (int index = 4; index < argc; ++index) worker.emplace_back(argv[index]);
    return Duel6::Platform::Darwin::runGuardian(values[0], values[1], values[2], worker);
}
