#include "TestHarness.h"
#include "source/ApplicationMode.h"
#include <string>
#include <vector>

namespace {
    Duel6::ApplicationArguments select(std::vector<std::string> values) {
        std::vector<char *> args;
        for (auto &value : values) args.push_back(value.data());
        // Only the mode is inspected here; argument preservation is tested below
        // with storage whose lifetime spans the assertions.
        return Duel6::selectApplicationMode(static_cast<int>(args.size()), args.data());
    }
}

D6R_TEST_CASE("application mode selects client and explicit headless mode without initialization") {
    using Duel6::ApplicationMode;
    D6R_REQUIRE(select({"duel6r"}).mode == ApplicationMode::Client);
    D6R_REQUIRE(select({"duel6r", "rounds 3", "echo hello world"}).mode == ApplicationMode::Client);
    D6R_REQUIRE(select({"duel6r", "--server"}).mode == ApplicationMode::Server);
    D6R_REQUIRE(select({"duel6r", "--port=26660", "--server"}).mode == ApplicationMode::Server);
    D6R_REQUIRE(select({"duel6r", "--help"}).mode == ApplicationMode::Help);
    D6R_REQUIRE(select({"duel6r", "--help", "--server"}).mode == ApplicationMode::ServerHelp);
}

D6R_TEST_CASE("application mode rejects missing malformed repeated and unknown selectors") {
    for (auto values : std::vector<std::vector<std::string>>{
            {"duel6r", "--dedicated"}, {"duel6r", "--transport"},
            {"duel6r", "--help", "--port=1234"}, {"duel6r", "--unknown"},
            {"duel6r", "--server=yes"}, {"duel6r", "--server-extra"},
            {"duel6r", "--server", "--server"}})
        D6R_REQUIRE(select(std::move(values)).mode == Duel6::ApplicationMode::Invalid);
}

D6R_TEST_CASE("application mode preserves ordered console and stripped server argument vectors") {
    std::vector<std::string> values{"duel6r", "rounds 2", "rounds 3", "echo two words"};
    std::vector<char *> args;
    for (auto &value : values) args.push_back(value.data());
    auto result = Duel6::selectApplicationMode(static_cast<int>(args.size()), args.data());
    D6R_REQUIRE(result.arguments == args);
    values = {"duel6r", "--port=26660", "--server", "--transport"};
    args.clear();
    for (auto &value : values) args.push_back(value.data());
    result = Duel6::selectApplicationMode(static_cast<int>(args.size()), args.data());
    D6R_REQUIRE_EQ(3u, result.arguments.size());
    D6R_REQUIRE_EQ(std::string("--port=26660"), std::string(result.arguments[1]));
    D6R_REQUIRE_EQ(std::string("--transport"), std::string(result.arguments[2]));
}
