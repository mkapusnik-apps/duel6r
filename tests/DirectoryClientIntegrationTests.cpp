#include "source/client/HostDirectory.h"
#include "source/client/NetworkSessionRuntime.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef D6R_TRANSPORT_WINDOWS
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
using namespace Duel6::Client;
using namespace std::chrono_literals;
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }

template<typename Predicate> void eventually(Predicate predicate, std::chrono::seconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (predicate()) return;
        std::this_thread::sleep_for(200ms);
    } while (std::chrono::steady_clock::now() < deadline);
    require(predicate(), "Directory lifecycle did not converge within deadline.");
}
std::uint16_t unusedPort() {
#ifdef D6R_TRANSPORT_WINDOWS
    WSADATA data{}; require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock initialization failed.");
    const auto fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    require(fd != INVALID_SOCKET, "Port probe failed.");
    int length = sizeof(sockaddr_in);
#else
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    require(fd >= 0, "Port probe failed.");
    socklen_t length = sizeof(sockaddr_in);
#endif
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool bound = ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0
        && getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) == 0;
#ifdef D6R_TRANSPORT_WINDOWS
    closesocket(fd); WSACleanup();
#else
    ::close(fd);
#endif
    require(bound, "Port probe bind failed.");
    return ntohs(address.sin_port);
}
}

int main(int argc, char **argv) {
    // An explicit local service backed by the real emulator is mandatory. Never
    // silently skip, fall back to a fake, or select a production cloud service.
    const char *url = std::getenv("D6R_DIRECTORY_URL");
    const char *local = std::getenv("D6R_DIRECTORY_ALLOW_HTTP");
    if (!url || std::string(url).rfind("http://127.0.0.1:", 0) != 0 || !local || std::string(local) != "1") {
        std::cerr << "An explicit loopback emulator-backed directory is required.\n"; return 2;
    }
    if (argc != 3) { std::cerr << "Provide the server executable and resource root.\n"; return 2; }
    try {
        DirectoryListing listing;
        listing.sessionId = directorySessionId(987654321);
        listing.endpoint = {"127.0.0.1", 25660}; listing.mode = "predator";
        listing.phase = "lobby"; listing.players = 2; listing.capacity = 15;
        listing.passwordRequired = true;
        DirectoryPublisher publisher;
        publisher.update(listing);
        std::string id;
        eventually([&] {
            const auto page = directoryPage({});
            if (!page.available) return false;
            const auto found = std::find_if(page.listings.begin(), page.listings.end(), [&](const auto &row) {
                return row.sessionId == listing.sessionId;
            });
            if (found == page.listings.end()) return false;
            require(found->passwordRequired && found->joinable(), "Locked lobby must be eligible.");
            id = found->id; return true;
        });
        for (const auto &phase: {"first-round", "closed"}) {
            listing.phase = phase; publisher.update(listing);
            eventually([&] {
                const auto page = directoryPage({});
                if (!page.available) return false;
                for (const auto &row: page.listings) if (row.id == id && row.phase == phase) {
                    require(row.joinable() == (row.phase == "first-round"), "Phase admission eligibility is incorrect.");
                    return true;
                }
                return false;
            });
        }
        listing.phase = "lobby"; listing.players = listing.capacity; publisher.update(listing);
        eventually([&] {
            const auto page = directoryPage({});
            for (const auto &row: page.listings) if (row.id == id && row.players == row.capacity) {
                require(!row.joinable(), "Full listing must not be eligible."); return page.available;
            }
            return false;
        });
        DirectoryBrowser browser; browser.refresh();
        eventually([&] { browser.update(); return !browser.loading(); });
        require(browser.available() && !browser.stale(), "Browser failed to consume real directory response.");
        require(std::any_of(browser.result().listings.begin(), browser.result().listings.end(),
            [&](const auto &row) { return row.id == id && !row.joinable(); }), "Browser hid a full active listing.");
        publisher.stop();
        eventually([&] {
            const auto page = directoryPage({});
            return page.available && std::none_of(page.listings.begin(), page.listings.end(), [&](const auto &row) { return row.id == id; });
        });
        std::cout << "[PASS] Native publication, phase updates, full/locked browsing, reopening and owner removal against the real directory.\n";
        NetworkSessionRuntime host, guest, arrival;
        Duel6::Network::HostComposition::Setup setup;
        setup.localPlayerNames = {"Directory host"}; setup.fixedLevel = "levels/duel_01.json";
        setup.quickLiquid = false; setup.roundLimit = 2;
        setup.password = std::make_shared<Duel6::Network::SessionPassword>("directory-runtime-fixture");
        const Duel6::Network::Endpoint endpoint{"127.0.0.1", unusedPort()};
        std::cout << "[STEP] Real hosted readiness and publication\n" << std::flush;
        require(host.startHost(endpoint, argv[1], argv[2], setup, {{"Directory host", nullptr, {}}}), "Hosted runtime did not start.");
        const auto pump = [&] { host.update(); guest.update(); arrival.update(); };
        try {
            eventually([&] { pump(); return host.snapshot().journey == NetworkJourney::Lobby && host.snapshot().directoryAvailable; }, 10s);
        } catch (...) {
            const auto state = host.snapshot();
            std::cerr << "Host journey=" << static_cast<unsigned>(state.journey) << "; publication=" << state.directoryAvailable
                      << "; failure=" << state.failure << '\n';
            throw;
        }
        const auto sessionId = directorySessionId(host.snapshot().canonical->sessionId);
        DirectoryListing advertised;
        const auto findListing = [&](const char *phase) {
            pump();
            const auto page = directoryPage({});
            if (!page.available) return false;
            for (const auto &row: page.listings) if (row.sessionId == sessionId && row.phase == phase) {
                advertised = row; return true;
            }
            return false;
        };
        eventually([&] { return findListing("lobby"); });
        std::cout << "[STEP] Protected directory-selected lobby join\n" << std::flush;
        require(advertised.passwordRequired && advertised.joinable(), "Live protected lobby was not advertised.");
        require(guest.join(advertised.endpoint, argv[2], {{"Directory guest", nullptr, {}}}, setup.password, advertised.sessionId), "Selected-session connection did not start.");
        try { eventually([&] { pump(); return guest.snapshot().journey == NetworkJourney::Lobby; }, 10s); }
        catch (...) {
            std::cerr << "Guest journey=" << static_cast<unsigned>(guest.snapshot().journey)
                      << "; failure=" << guest.snapshot().failure << '\n';
            throw;
        }
        host.setReady(true); guest.setReady(true);
        eventually([&] {
            pump(); const auto state = host.snapshot().canonical;
            return state && std::all_of(state->participants.begin(), state->participants.end(), [](const auto &p) { return p.ready; });
        });
        host.startMatch();
        std::cout << "[STEP] First-round publication and selected live arrival\n" << std::flush;
        eventually([&] { return findListing("first-round"); });
        const auto before = *host.snapshot().canonical;
        require(arrival.join(advertised.endpoint, argv[2], {{"Directory arrival", nullptr, {}}}, setup.password, advertised.sessionId), "Live selected-session connection did not start.");
        eventually([&] { pump(); return arrival.snapshot().journey == NetworkJourney::Match; }, 10s);
        const auto after = *arrival.snapshot().canonical;
        require(after.players.size() == 3 && before.round && after.round && before.round->roundId == after.round->roundId,
                "Directory arrival did not enter the current round.");
        host.endSession();
        std::cout << "[STEP] Hosted shutdown and listing removal\n" << std::flush;
        eventually([&] { pump(); return host.snapshot().journey == NetworkJourney::Inactive; });
        eventually([&] {
            const auto page = directoryPage({});
            return page.available && std::none_of(page.listings.begin(), page.listings.end(), [&](const auto &row) { return row.sessionId == sessionId; });
        });
        std::cout << "[PASS] Real host readiness/publication, protected directory-selected lobby and live joins, and session shutdown removal.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[FAIL] " << error.what() << '\n'; return 1;
    }
}
