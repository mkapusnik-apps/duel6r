#include "source/platform/DarwinProcess.h"
#include <cstdlib>

// Test executable only. Production helper has no inspection override switch.
int main(int argc, char **argv) {
    if (argc < 5) return 2;
    std::vector<std::string> arguments;
    for (int index = 4; index < argc; ++index) arguments.emplace_back(argv[index]);
    return Duel6::Platform::Darwin::runGuardian(std::atoi(argv[1]), std::atoi(argv[2]),
        std::atoi(argv[3]), arguments, [](pid_t) { return Duel6::Platform::Darwin::GroupInspection::Unknown; });
}
