#include "TestHarness.h"
#include "source/client/DirectoryOrigin.h"

using Duel6::Client::directoryOrigin;

D6R_TEST_CASE("NET-DIR absent override selects only the compiled distribution origin") {
    D6R_REQUIRE_EQ(std::string(D6R_TEST_DEFAULT_URL), directoryOrigin(nullptr, nullptr));
    D6R_REQUIRE_EQ(std::string(D6R_TEST_DEFAULT_URL), directoryOrigin(nullptr, "1"));
    // Selection has no request state and cannot switch channel after failure.
    D6R_REQUIRE(directoryOrigin("", nullptr).empty());
    D6R_REQUIRE_EQ(std::string(D6R_TEST_DEFAULT_URL), directoryOrigin(nullptr, nullptr));
}

D6R_TEST_CASE("NET-DIR explicit HTTPS overrides either channel and keeps origin validation") {
    D6R_REQUIRE_EQ(std::string("https://override.example"), directoryOrigin("https://override.example", nullptr));
    D6R_REQUIRE_EQ(std::string("https://override.example"), directoryOrigin("https://override.example///", nullptr));
    D6R_REQUIRE_EQ(std::string("https://staging.duel.netusite.cz"), directoryOrigin("https://staging.duel.netusite.cz", nullptr));
    D6R_REQUIRE_EQ(std::string("https://duel.netusite.cz"), directoryOrigin("https://duel.netusite.cz", nullptr));
    for (const auto *invalid : {"", " https://override.example", "HTTPS://override.example",
            "http://override.example", "ftp://override.example", "https://user@override.example",
            "https://override.example?query", "https://override.example#fragment",
            "https://override.example\r", "https://override.example\n"}) {
        D6R_REQUIRE(directoryOrigin(invalid, nullptr).empty());
        D6R_REQUIRE(directoryOrigin(invalid, "1").empty());
    }
    const std::string tooLong = "https://" + std::string(505, 'a');
    D6R_REQUIRE(directoryOrigin(tooLong.c_str(), nullptr).empty());
    const std::string atLimit = "https://" + std::string(504, 'a');
    D6R_REQUIRE_EQ(atLimit, directoryOrigin(atLimit.c_str(), nullptr));
}

D6R_TEST_CASE("NET-DIR HTTP development exception requires explicit loopback opt-in") {
    const char *loopback = "http://127.0.0.1:8081";
    D6R_REQUIRE(directoryOrigin(loopback, nullptr).empty());
    D6R_REQUIRE(directoryOrigin(loopback, "0").empty());
    D6R_REQUIRE(directoryOrigin(loopback, "true").empty());
    D6R_REQUIRE_EQ(std::string(loopback), directoryOrigin(loopback, "1"));
    for (const auto *invalid : {"http://localhost:8081", "http://127.0.0.2:8081",
            "http://192.168.1.1:8081", "http://127.0.0.1:8081@other.example",
            "http://127.0.0.1:8081?query", "http://127.0.0.1:8081#fragment"}) {
        D6R_REQUIRE(directoryOrigin(invalid, "1").empty());
    }
}
