#include "TestHarness.h"
#include "source/client/HostServiceSupervisor.h"
#include "source/network/SessionTransport.h"
#include "source/platform/DarwinChild.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace Duel6;
    using namespace std::chrono_literals;
    struct Port {
        int fd = -1;
        std::uint16_t number = 0;
        Port() {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            D6R_REQUIRE(fd >= 0);
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            D6R_REQUIRE(bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
            socklen_t length = sizeof(address);
            D6R_REQUIRE(getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) == 0);
            number = ntohs(address.sin_port);
        }
        void release() { if (fd >= 0) { close(fd); fd = -1; } }
        ~Port() { release(); }
    };
    template<typename Predicate> bool await(Predicate predicate, std::chrono::milliseconds budget = 3s) {
        const auto deadline = std::chrono::steady_clock::now() + budget;
        do {
            if (predicate()) return true;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return predicate();
    }
    Client::HostServiceStartConfig config(std::uint16_t port) {
        Client::HostServiceStartConfig value;
        value.serverExecutable = D6R_ADAPTER_SERVER;
        value.resourcePath = D6R_ADAPTER_RESOURCES;
        value.endpoint = {"127.0.0.1", port};
        return value;
    }
}

D6R_TEST_CASE("Darwin real guarded DNS and encrypted production TCP exchange preserve resolver cleanup") {
    Port port; port.release();
    Network::SessionTransportDependencies dependencies;
    dependencies.secureSession = true;
    dependencies.enforceNetworkSessionPolicy = true;
    dependencies.password = std::make_shared<Network::SessionPassword>("native-adapter-test");
    Network::TcpListener listener(2, dependencies);
    D6R_REQUIRE(listener.start({"127.0.0.1", port.number}));
    D6R_REQUIRE(listener.waitForReady(3s));
    Network::TcpClient client(dependencies);
    D6R_REQUIRE(client.start({"localhost", port.number})); // Actual resolver helper, not an injected DNS seam.
    D6R_REQUIRE(client.waitForConnected(5s));
    std::shared_ptr<Network::TcpConnection> peer;
    D6R_REQUIRE(await([&] { peer = listener.acceptConnection(); return bool(peer); }));
    D6R_REQUIRE(client.connection()->send({1,2,3}) == Network::SendResult::Accepted);
    Network::TransportFrame frame;
    D6R_REQUIRE(await([&] { return peer->receive(frame); }));
    D6R_REQUIRE_EQ((std::vector<std::uint8_t>{1,2,3}), frame.payload);
    client.close(); listener.shutdown();
    for (unsigned attempt = 0; attempt < 40; ++attempt) {
        Network::TcpClient cancelled(dependencies);
        D6R_REQUIRE(cancelled.start({"localhost", port.number}));
        cancelled.cancel(); cancelled.close();
    }
    // Subsequent real resolution must remain usable after the bounded reaper
    // retires cancelled workers; no unbounded detached resolver accumulation.
    Network::TcpListener again(2, dependencies);
    D6R_REQUIRE(again.start({"127.0.0.1", port.number}));
    D6R_REQUIRE(again.waitForReady(3s));
    Network::TcpClient last(dependencies);
    D6R_REQUIRE(last.start({"localhost", port.number}));
    D6R_REQUIRE(last.waitForConnected(5s));
    last.close(); again.shutdown();
}

D6R_TEST_CASE("Darwin guarded spawn failure positively confirms no service without releasing unresolved trees") {
    auto child = Platform::Darwin::GuardedChild::launch({"/duel6r-nonexistent-test-executable"});
    D6R_REQUIRE(child);
    D6R_REQUIRE(child->waitForCleanup(3s));
    D6R_REQUIRE(child->startupFailed());
    D6R_REQUIRE(child->exited());
}

D6R_TEST_CASE("Darwin production host adapter preserves readiness stop cancel and port conflict") {
    using State = Client::HostServiceState;
    Port port; port.release();
    {
        Client::HostServiceSupervisor supervisor;
        auto local = config(port.number);
        local.endpoint.host = "localhost";
        D6R_REQUIRE(supervisor.start(local));
        D6R_REQUIRE(supervisor.waitForState(State::Active, 5s));
        supervisor.applicationExit();
        D6R_REQUIRE(supervisor.waitForState(State::ApplicationExit, 3s));
        D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
    }
    {
        Client::HostServiceSupervisor supervisor;
        D6R_REQUIRE(supervisor.start(config(port.number)));
        D6R_REQUIRE(supervisor.cancelStartup());
        D6R_REQUIRE(supervisor.waitForState(State::NoService, 3s));
        D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
    }
    {
        Port occupied;
        D6R_REQUIRE(listen(occupied.fd, 1) == 0);
        Client::HostServiceSupervisor supervisor;
        D6R_REQUIRE(supervisor.start(config(occupied.number)));
        D6R_REQUIRE(supervisor.waitForState(State::StartupFailed, 5s));
        D6R_REQUIRE(supervisor.snapshot().outcome == Client::HostServiceOutcome::PortUnavailable);
        D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
    }
}
