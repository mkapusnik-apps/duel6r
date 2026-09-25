#include "source/client/HostDirectory.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

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
}

int main() {
    // An explicit local service backed by the real emulator is mandatory. Never
    // silently skip, fall back to a fake, or select a production cloud service.
    const char *url = std::getenv("D6R_DIRECTORY_URL");
    const char *local = std::getenv("D6R_DIRECTORY_ALLOW_HTTP");
    if (!url || std::string(url).rfind("http://127.0.0.1:", 0) != 0 || !local || std::string(local) != "1") {
        std::cerr << "An explicit loopback emulator-backed directory is required.\n"; return 2;
    }
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
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[FAIL] " << error.what() << '\n'; return 1;
    }
}
