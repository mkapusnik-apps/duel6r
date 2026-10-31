#ifndef DUEL6_TESTS_HOSTDIAGNOSTICFIXTURE_H
#define DUEL6_TESTS_HOSTDIAGNOSTICFIXTURE_H
#ifdef __linux__
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tests/TestHarness.h"
#include "source/network/HostServiceDiagnostics.h"

namespace Duel6::Test {
    struct HostDiagnosticRecord {
        long long pid = 0, ns = 0, a = 0, b = 0, c = 0;
        unsigned seq = 0, dropped = 0;
        int event = 0;
    };
    struct HostDiagnosticFixture {
        std::filesystem::path directory, fifo;
        int reader = -1;
        HostDiagnosticFixture() {
            std::string pattern = (std::filesystem::temp_directory_path() / "d6r-diagnostics-XXXXXX").string();
            const char *created = mkdtemp(pattern.data());
            D6R_REQUIRE(created != nullptr);
            directory = created;
            fifo = directory / "trace.fifo";
            D6R_REQUIRE(mkfifo(fifo.c_str(), 0600) == 0);
            reader = ::open(fifo.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            D6R_REQUIRE(reader >= 0);
        }
        ~HostDiagnosticFixture() {
            if (reader >= 0) close(reader);
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
        std::vector<HostDiagnosticRecord> drain() const {
            std::string text;
            char bytes[4096];
            for (;;) {
                const auto count = read(reader, bytes, sizeof(bytes));
                if (count <= 0) break;
                text.append(bytes, count);
            }
            std::istringstream lines(text);
            std::string line;
            std::vector<HostDiagnosticRecord> records;
            while (std::getline(lines, line)) {
                HostDiagnosticRecord record;
                int consumed = 0;
                D6R_REQUIRE_EQ(8, std::sscanf(line.c_str(),
                        "HSD1 pid=%lld seq=%u ns=%lld event=%d a=%lld b=%lld c=%lld dropped=%u%n",
                        &record.pid, &record.seq, &record.ns, &record.event,
                        &record.a, &record.b, &record.c, &record.dropped, &consumed));
                D6R_REQUIRE_EQ(line.size(), static_cast<std::size_t>(consumed));
                records.push_back(record);
            }
            return records;
        }
    };
}
#endif
#endif
