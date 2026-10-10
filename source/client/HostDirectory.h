#ifndef DUEL6_CLIENT_HOSTDIRECTORY_H
#define DUEL6_CLIENT_HOSTDIRECTORY_H
#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "../network/Protocol.h"
#include "../network/PublicSession.h"

namespace Duel6::Client {
    struct DirectoryListing {
        std::string id, sessionId, mode, phase;
        Network::Endpoint endpoint;
        unsigned players = 0, capacity = 0;
        bool passwordRequired = false;
        std::string hosting = "player-hosted", protection = "none", admission;
        double expiresAt = 0;
        unsigned revision = 0;
        bool dedicated() const { return hosting == "dedicated"; }
        std::string admissionState() const {
            return admission.empty() ? (phase == "closed" ? "closed" : players >= capacity ? "full" : "open") : admission;
        }
        bool joinable() const {
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return expiresAt > now && players < capacity && admissionState() == "open"
                && (!dedicated() || Network::PublicSession::supported())
                && (phase == "lobby" || phase == "first-round" || (dedicated() && phase == "idle"));
        }
    };
    struct DirectoryPage {
        bool available = false;
        std::vector<DirectoryListing> listings;
        std::string nextCursor;
    };
    struct DirectoryResponse { long status = 0; std::string body; };
    struct DirectoryPublisherDependencies {
        std::function<std::optional<std::vector<std::string>>()> listeningAddresses;
        std::function<DirectoryResponse(const std::string &, const std::string &, const std::string &,
                const std::string &, unsigned, const std::atomic<bool> *)> request;
    };
    DirectoryResponse directoryRequest(const std::string &method, const std::string &path,
                                       const std::string &body = {}, const std::string &owner = {}, unsigned revision = 0,
                                       const std::atomic<bool> *cancelled = nullptr);
    DirectoryPage directoryPage(const std::string &cursor);
    DirectoryPage decodeDirectoryPage(const std::string &body, const std::string &cursor = {}, bool mixed = false);
    std::string directorySessionId(std::uint64_t id);
    // Compare the URL parser's validated host and effective numeric port, without DNS.
    bool directoryOriginMatchesFrontend(const std::string &origin, const Network::Endpoint &frontend);

    class DirectoryBrowser final {
    public:
        void refresh();
        void next();
        void previous();
        void update();
        bool loading() const { return pending.valid(); }
        bool stale() const;
        bool available() const { return page.available; }
        const DirectoryPage &result() const { return page; }
        bool hasPrevious() const { return !cursors.empty(); }
        std::size_t pageNumber() const { return cursors.size() + 1; }
    private:
        DirectoryPage page;
        std::future<DirectoryPage> pending;
        std::string cursor;
        std::vector<std::string> cursors;
        std::chrono::steady_clock::time_point requested{}, received{};
    };

    class DirectoryPublisher final {
    public:
        explicit DirectoryPublisher(std::vector<std::string> listeningCoverage = {},
                                    DirectoryPublisherDependencies dependencies = {});
        ~DirectoryPublisher();
        void update(DirectoryListing listing);
        void retry();
        void stop();
        bool available() const { return published.load() && std::chrono::steady_clock::now() < validUntil.load(); }
        bool busy() const { return pending.load(); }
    private:
        class Impl;
        std::atomic<bool> published{false};
        std::atomic<bool> pending{false};
        std::atomic<std::chrono::steady_clock::time_point> validUntil{};
        std::unique_ptr<Impl> impl;
    };
}
#endif
