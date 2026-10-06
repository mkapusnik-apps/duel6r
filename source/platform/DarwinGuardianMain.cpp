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
    Duel6::Platform::Darwin::WorkerChannels channels;
    int firstWorker = 4;
    if (std::strcmp(argv[4], "--worker-io") == 0) {
        if (argc < 8) return 2;
        int *descriptors[] = {&channels.output, &channels.input};
        for (int index = 0; index < 2; ++index) {
            const char *text = argv[5 + index];
            const auto end = text + std::strlen(text);
            const auto result = std::from_chars(text, end, *descriptors[index]);
            if (result.ec != std::errc{} || result.ptr != end || *descriptors[index] < 3
                || *descriptors[index] == values[1] || *descriptors[index] == values[2]) return 2;
        }
        if (channels.output == channels.input) return 2;
        firstWorker = 7;
    }
    std::vector<std::string> worker;
    for (int index = firstWorker; index < argc; ++index) worker.emplace_back(argv[index]);
    return Duel6::Platform::Darwin::runGuardian(values[0], values[1], values[2], worker,
                                               Duel6::Platform::Darwin::inspectGroup, channels);
}
