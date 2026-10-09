#include "TestHarness.h"
#include "source/network/NetworkTrustPolicy.h"
#include "source/network/SessionLifecycle.h"
#include "source/server/HeadlessServer.h"
#include "source/server/AuthoritativeMatchCli.h"
#include <sstream>

extern "C" void d6rRandomFailureAt(unsigned);
extern "C" unsigned d6rRandomCalls();

namespace {
    struct RandomFault {
        explicit RandomFault(unsigned call) { d6rRandomFailureAt(call); }
        ~RandomFault() { d6rRandomFailureAt(0); }
    };
}

D6R_TEST_CASE("Darwin real secure random and production credential failures fail closed") {
    using namespace Duel6::Network;
    {
        RandomFault control(0);
        Trust::ReconnectReservation first(1, 2, 3), second(1, 2, 4);
        D6R_REQUIRE(first.valid() && second.valid());
        D6R_REQUIRE(first.credential().bytes != second.credential().bytes);
        D6R_REQUIRE(d6rRandomCalls() >= 2);
    }
    {
        RandomFault fault(1);
        Trust::ReconnectReservation rejected(1, 2, 3);
        D6R_REQUIRE(!rejected.valid() && !rejected.active());
        D6R_REQUIRE(d6rRandomCalls() >= 1);
    }
    {
        RandomFault fault(1);
        Lifecycle::HostSessionLifecycle host(1, 2, 3, {4});
        D6R_REQUIRE(!host.admitGuest(5, 6, {7}, false, true));
        D6R_REQUIRE(d6rRandomCalls() >= 1);
    }
}

D6R_TEST_CASE("Darwin production host session and match seed failures precede listener startup") {
    using namespace Duel6;
    for (unsigned call : {1u, 2u}) {
        RandomFault fault(call);
        Server::ServerConfig config;
        config.transportEnabled = true;
        config.listenEndpoint = {"127.0.0.1", 26660};
        config.resourcePath = D6R_RANDOM_RESOURCES;
        bool listening = false;
        Server::AdmissionRuntimeDependencies dependencies;
        dependencies.lifecycleObserver = [&](const auto &event) {
            if (event.stage == Server::AdmissionLifecycleStage::ListenerStarting) listening = true;
            return true;
        };
        std::ostringstream output;
        Server::HeadlessServer server(config, dependencies);
        D6R_REQUIRE_EQ(2, server.run(output));
        D6R_REQUIRE(!listening && d6rRandomCalls() >= call);
    }
}

D6R_TEST_CASE("Darwin authoritative CLI rejects actual system seed failure before world construction") {
    using namespace Duel6;
    RandomFault fault(1);
    char name[] = "random-failure", option[] = "--authoritative-match";
    std::string resource = std::string("--resources=") + D6R_RANDOM_RESOURCES;
    char *arguments[] = {name, option, resource.data()};
    bool world = false;
    Server::Authoritative::AuthoritativeMatchCliDependencies dependencies;
    dependencies.runtimeFactory = [&](const auto &, const auto &, const auto &) {
        world = true; return Server::Authoritative::MatchRuntimeDependencies{};
    };
    std::istringstream input;
    std::ostringstream output;
    D6R_REQUIRE(Server::Authoritative::runAuthoritativeMatchCli(3, arguments, input, output, dependencies) != 0);
    D6R_REQUIRE(!world && d6rRandomCalls() >= 1);
    D6R_REQUIRE(output.str().find("authoritative-match-runtime-failed") != std::string::npos);
}
