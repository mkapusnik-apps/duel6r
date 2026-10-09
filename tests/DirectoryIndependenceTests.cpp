#include "TestHarness.h"
#include "source/client/HostDirectory.h"
#include "source/client/NetworkSessionRuntime.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <mutex>

namespace {
    using namespace Duel6;
    using namespace std::chrono_literals;
    struct Port {
        int socket = ::socket(AF_INET, SOCK_STREAM, 0);
        std::uint16_t number = 0;
        Port() {
            D6R_REQUIRE(socket >= 0);
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            D6R_REQUIRE(bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
            socklen_t length = sizeof(address);
            D6R_REQUIRE(getsockname(socket, reinterpret_cast<sockaddr *>(&address), &length) == 0);
            number = ntohs(address.sin_port);
        }
        void release() { if (socket >= 0) { close(socket); socket = -1; } }
        ~Port() { release(); }
    };
    struct Environment {
        std::string key;
        std::optional<std::string> previous;
        Environment(const char *name, const std::string &value) : key(name) {
            if (const auto *old = std::getenv(name)) previous = old;
            D6R_REQUIRE(setenv(name, value.c_str(), 1) == 0);
        }
        ~Environment() { if (previous) setenv(key.c_str(), previous->c_str(), 1); else unsetenv(key.c_str()); }
    };
    template<typename Predicate> bool await(Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        do { if (predicate()) return true; std::this_thread::sleep_for(5ms); }
        while (std::chrono::steady_clock::now() < deadline);
        return predicate();
    }
}

D6R_TEST_CASE("NET-DIR publication revalidates concrete coverage updates one listing and removes unavailable coverage") {
    // Synchronized protocol mock, not emulator or hosted backend evidence.
    std::mutex mutex;
    std::optional<std::vector<std::string>> addresses = std::vector<std::string>{"127.0.0.1", "10.0.0.20", "10.0.0.3"};
    std::vector<std::pair<std::string, std::string>> requests;
    const std::string id(32, 'a'), owner(64, 'b');
    const auto session = Client::directorySessionId(99);
    unsigned revision = 0;
    Client::DirectoryPublisherDependencies dependencies;
    dependencies.listeningAddresses = [&] { std::lock_guard<std::mutex> lock(mutex); return addresses; };
    dependencies.request = [&](const auto &method, const auto &path, const auto &body, const auto &token, auto expectedRevision, const auto *) {
        std::lock_guard<std::mutex> lock(mutex);
        requests.emplace_back(method, body);
        if (method != "POST") {
            D6R_REQUIRE_EQ(std::string("/v1/listings/") + id, path);
            D6R_REQUIRE_EQ(owner, token); D6R_REQUIRE_EQ(revision, expectedRevision);
        }
        if (method == "DELETE") return Client::DirectoryResponse{204, {}};
        ++revision;
        const auto address = body.find("1.2.3.4") != std::string::npos ? "1.2.3.4" : "10.0.0.3";
        return Client::DirectoryResponse{method == "POST" ? 201 : 200,
            "{\"id\":\"" + id + "\",\"sessionId\":\"" + session + "\",\"address\":\"" + address
            + "\",\"port\":26660,\"players\":1,\"capacity\":15,\"mode\":\"deathmatch\",\"phase\":\"lobby\","
            "\"passwordRequired\":false,\"expiresAt\":2000000000000,\"revision\":" + std::to_string(revision)
            + ",\"ownerToken\":\"" + owner + "\"}"};
    };
    Client::DirectoryPublisher publisher({"0.0.0.0"}, dependencies);
    Client::DirectoryListing listing;
    listing.sessionId = session; listing.endpoint = {"0.0.0.0", 26660};
    listing.players = 1; listing.capacity = 15; listing.mode = "deathmatch"; listing.phase = "lobby";
    publisher.update(listing);
    D6R_REQUIRE(await([&] { return publisher.available() && !publisher.busy(); }));
    {
        std::lock_guard<std::mutex> lock(mutex);
        D6R_REQUIRE_EQ(1u, requests.size());
        D6R_REQUIRE(requests.front().second.find("10.0.0.3") != std::string::npos);
        D6R_REQUIRE(requests.front().second.find("0.0.0.0") == std::string::npos);
        addresses->push_back("1.2.3.4");
    }
    publisher.retry();
    D6R_REQUIRE(await([&] { return publisher.available() && !publisher.busy(); }));
    {
        std::lock_guard<std::mutex> lock(mutex);
        D6R_REQUIRE_EQ(2u, requests.size()); D6R_REQUIRE_EQ(std::string("PUT"), requests.back().first);
        D6R_REQUIRE(requests.back().second.find("1.2.3.4") != std::string::npos);
        addresses.reset();
    }
    publisher.retry();
    D6R_REQUIRE(await([&] { return !publisher.available() && !publisher.busy(); }));
    {
        std::lock_guard<std::mutex> lock(mutex);
        D6R_REQUIRE_EQ(3u, requests.size()); D6R_REQUIRE_EQ(std::string("DELETE"), requests.back().first);
    }
    publisher.stop();
    Client::DirectoryPublisher loopbackOnly({"127.0.0.1"}, dependencies);
    loopbackOnly.update(listing);
    D6R_REQUIRE(await([&] { return !loopbackOnly.busy(); }));
    D6R_REQUIRE(!loopbackOnly.available()); loopbackOnly.stop();
    { std::lock_guard<std::mutex> lock(mutex); D6R_REQUIRE_EQ(3u, requests.size()); }
}

D6R_TEST_CASE("Directory outage does not block actual protected Host direct Join or End") {
    Port offline; // Bound but never listening: a real unavailable loopback origin.
    Environment url("D6R_DIRECTORY_URL", "http://127.0.0.1:" + std::to_string(offline.number));
    Environment http("D6R_DIRECTORY_ALLOW_HTTP", "1");
    Client::DirectoryBrowser browser;
    browser.refresh();
    D6R_REQUIRE(await([&] { browser.update(); return !browser.loading(); }));
    D6R_REQUIRE(!browser.available());
    std::string server = D6R_DIRECTORY_SERVER, resources = D6R_DIRECTORY_RESOURCES;
    if (const char *bundle = std::getenv("D6R_TEST_BUNDLE")) {
        const auto root = std::filesystem::canonical(bundle);
        server = (root / "Contents/MacOS/duel6r-server").string();
        resources = (root / "Contents/Resources").string();
    }
    Port available; available.release();
    Network::Endpoint endpoint{"127.0.0.1", available.number};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Outage host"}; setup.fixedLevel = "levels/duel_01.json";
    setup.password = std::make_shared<Network::SessionPassword>("isolated-outage-fixture");
    Client::NetworkSessionRuntime host, guest;
    const auto pump = [&] { host.update(); guest.update(); };
    const auto awaitStage = [&](const char *stage, auto predicate) {
        if (await(predicate)) return;
        const auto hosted = host.snapshot(), joined = guest.snapshot();
        Test::fail(stage, __FILE__, __LINE__, "host-journey=" + std::to_string(static_cast<unsigned>(hosted.journey))
            + ";guest-journey=" + std::to_string(static_cast<unsigned>(joined.journey))
            + ";host-failure=" + hosted.failure + ";guest-failure=" + joined.failure);
    };
    D6R_REQUIRE(host.startHost(endpoint, server, resources, setup, {{"Outage host", nullptr, {}}}));
    awaitStage("directory outage host readiness", [&] { pump(); return host.snapshot().journey == Client::NetworkJourney::Lobby; });
    D6R_REQUIRE(guest.join(endpoint, resources, {{"Outage guest", nullptr, {}}}, setup.password));
    awaitStage("directory outage protected direct admission", [&] { pump(); return guest.snapshot().journey == Client::NetworkJourney::Lobby; });
    D6R_REQUIRE(!host.snapshot().directoryAvailable);
    D6R_REQUIRE(guest.snapshot().canonical && guest.snapshot().canonical->players.size() == 2);
    host.endSession();
    awaitStage("directory outage intentional End", [&] { pump(); return host.snapshot().journey == Client::NetworkJourney::Inactive
        && guest.snapshot().journey == Client::NetworkJourney::HostEnded; });
}
