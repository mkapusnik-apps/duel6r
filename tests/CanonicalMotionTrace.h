#ifndef DUEL6_TEST_CANONICAL_MOTION_TRACE_H
#define DUEL6_TEST_CANONICAL_MOTION_TRACE_H

#include <fstream>
#include <string>
#include <vector>
#include "source/network/StateReplicationProtocol.h"
#include "tests/TestHarness.h"

namespace Duel6::Test {
// A process boundary deliberately keeps the headless gameplay ABI separate
// from the GL4 presenter ABI. Payloads use the real replication wire codec.
inline constexpr const char *CanonicalMotionProducer =
        "PR87 real canonical inputs and water timer produce orientation trace";
struct CanonicalMotionSample {
    std::string name;
    Network::Replication::FullSnapshot snapshot;
};

inline void writeCanonicalMotionTrace(const std::string &path,
                                      const std::vector<CanonicalMotionSample> &samples) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    D6R_REQUIRE(output.good());
    output << "D6R-MOTION-1\n" << samples.size() << '\n';
    for (const auto &sample : samples) {
        const auto bytes = Network::Replication::serializeReplicationSnapshot(sample.snapshot);
        D6R_REQUIRE(!bytes.empty());
        output << sample.name << ' ' << bytes.size() << '\n';
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        output << '\n';
    }
    output.close();
    D6R_REQUIRE(!output.fail());
}

inline std::vector<CanonicalMotionSample> readCanonicalMotionTrace(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    std::getline(input, magic);
    D6R_REQUIRE_EQ(std::string("D6R-MOTION-1"), magic);
    std::size_t count = 0;
    input >> count;
    D6R_REQUIRE_EQ(8u, count);
    D6R_REQUIRE_EQ('\n', input.get());
    std::vector<CanonicalMotionSample> samples;
    for (std::size_t i = 0; i < count; ++i) {
        std::string name;
        std::size_t size = 0;
        input >> name >> size;
        D6R_REQUIRE(size > 0 && size <= 4 * 1024 * 1024);
        D6R_REQUIRE_EQ('\n', input.get());
        std::vector<std::uint8_t> bytes(size);
        input.read(reinterpret_cast<char *>(bytes.data()), size);
        D6R_REQUIRE(input.good());
        const auto decoded = Network::Replication::deserializeReplicationFrame(bytes);
        D6R_REQUIRE(decoded && decoded->snapshot);
        Network::Replication::ReplicatedState client;
        D6R_REQUIRE(client.apply(*decoded->snapshot) == Network::Replication::ApplyResult::Applied);
        samples.push_back({name, *decoded->snapshot});
        D6R_REQUIRE_EQ('\n', input.get());
    }
    D6R_REQUIRE_EQ(std::char_traits<char>::eof(), input.peek());
    return samples;
}
}
#endif
