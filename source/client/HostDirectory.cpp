#include "HostDirectory.h"
#include "../json/JsonParser.h"
#include "../network/NetworkTrustPolicy.h"
#include <curl/curl.h>
#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <set>
#include <stdexcept>
#include <thread>

namespace Duel6::Client {
    namespace {
        using Clock = std::chrono::steady_clock;
        bool hex(const std::string &value, std::size_t length = 32) {
            return value.size() == length && std::all_of(value.begin(), value.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
        }
        bool validCursor(const std::string &value) {
            const auto dash = value.find('-');
            return dash >= 1 && dash <= 16 && value.size() <= 49
                && std::all_of(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(dash),
                               [](char c) { return c >= '0' && c <= '9'; })
                && hex(value.substr(dash + 1));
        }
        std::size_t receive(char *data, std::size_t size, std::size_t count, void *context) {
            auto &body = *static_cast<std::string *>(context);
            if (size != 1 || count > 65536 - body.size()) return 0;
            try { body.append(data, count); return count; } catch (...) { return 0; }
        }
        DirectoryListing decode(const Json::Value &value) {
            DirectoryListing row;
            row.id = value.get("id").asString(); row.sessionId = value.get("sessionId").asString();
            row.mode = value.get("mode").asString(); row.phase = value.get("phase").asString();
            row.endpoint.host = value.get("address").asString();
            const auto integer = [&](const char *key) {
                const double number = value.get(key).asDouble();
                if (!std::isfinite(number) || std::floor(number) != number || number < 0 || number > 2147483647)
                    throw std::invalid_argument("Invalid directory response.");
                return static_cast<int>(number);
            };
            const int port = integer("port");
            const int players = integer("players"), capacity = integer("capacity");
            if (!hex(row.id) || !hex(row.sessionId) || row.sessionId == std::string(32, '0') || port < 1 || port > 65535
                || players < 1 || capacity < players || capacity > 15
                || Network::Trust::classifyIpv4Literal(row.endpoint.host) == Network::Trust::EndpointScope::Unsupported
                || Network::Trust::classifyIpv4Literal(row.endpoint.host) == Network::Trust::EndpointScope::Invalid
                || (row.mode != "deathmatch" && row.mode != "predator" && row.mode != "teams")
                || (row.phase != "lobby" && row.phase != "first-round" && row.phase != "closed"))
                throw std::invalid_argument("Invalid directory response.");
            row.endpoint.port = static_cast<std::uint16_t>(port); row.players = players; row.capacity = capacity;
            row.passwordRequired = value.get("passwordRequired").asBoolean();
            row.expiresAt = value.get("expiresAt").asDouble();
            if (!std::isfinite(row.expiresAt) || row.expiresAt < 0 || row.expiresAt > 253402300799999.0)
                throw std::invalid_argument("Invalid directory response.");
            const int revision = integer("revision");
            if (revision < 1) throw std::invalid_argument("Invalid directory response.");
            row.revision = revision;
            return row;
        }
        Json::Value parse(const std::string &body) {
            if (body.size() > 65536) throw std::invalid_argument("Directory response too large.");
            unsigned depth = 0;
            bool quoted = false, escaped = false, ended = false;
            for (char c: body) {
                if (ended) {
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                        throw std::invalid_argument("Trailing directory response data.");
                    continue;
                }
                if (quoted) {
                    if (escaped) escaped = false;
                    else if (c == '\\') escaped = true;
                    else if (c == '"') quoted = false;
                } else if (c == '"') quoted = true;
                else if (c == '[' || c == '{') {
                    if (++depth > 8) throw std::invalid_argument("Directory response nesting exceeds limit.");
                } else if (c == ']' || c == '}') {
                    if (depth == 0) throw std::invalid_argument("Invalid directory response.");
                    --depth;
                    if (depth == 0) ended = true;
                }
            }
            if (!ended || depth != 0 || quoted) throw std::invalid_argument("Invalid directory response.");
            return Json::Parser().parse(std::vector<Uint8>(body.begin(), body.end()));
        }
        std::string encode(const DirectoryListing &row) {
            // Only validated local fields and fixed enumerations enter this serializer.
            if (!hex(row.sessionId) || row.endpoint.host.find_first_not_of("0123456789.") != std::string::npos
                || (row.phase != "lobby" && row.phase != "first-round" && row.phase != "closed")
                || (row.mode != "deathmatch" && row.mode != "predator" && row.mode != "teams")) return {};
            return "{\"sessionId\":\"" + row.sessionId + "\",\"address\":\"" + row.endpoint.host
                + "\",\"port\":" + std::to_string(row.endpoint.port) + ",\"mode\":\"" + row.mode
                + "\",\"players\":" + std::to_string(row.players) + ",\"capacity\":" + std::to_string(row.capacity)
                + ",\"passwordRequired\":" + (row.passwordRequired ? "true" : "false")
                + ",\"phase\":\"" + row.phase + "\"}";
        }
    }
    std::string directorySessionId(std::uint64_t id) {
        std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(32) << id; return out.str();
    }
    DirectoryResponse directoryRequest(const std::string &method, const std::string &path,
                                       const std::string &body, const std::string &owner, unsigned revision,
                                       const std::atomic<bool> *cancelled) {
        static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
        DirectoryResponse result;
        const char *configured = std::getenv("D6R_DIRECTORY_URL");
        if (initialized != CURLE_OK || !configured || !(curl_version_info(CURLVERSION_NOW)->features & CURL_VERSION_ASYNCHDNS)) return result;
        std::string base(configured);
        const char *dev = std::getenv("D6R_DIRECTORY_ALLOW_HTTP");
        const bool local = dev && std::string(dev) == "1" && base.rfind("http://127.0.0.1:", 0) == 0;
        if (base.size() > 512 || base.find_first_of("@?#\r\n") != std::string::npos
            || (base.rfind("https://", 0) != 0 && !local)) return result;
        while (!base.empty() && base.back() == '/') base.pop_back();
        CURL *curl = curl_easy_init();
        if (!curl) return result;
        curl_slist *headers = curl_slist_append(nullptr, "Content-Type: application/json");
        if (!owner.empty()) {
            if (!hex(owner, 64)) { curl_slist_free_all(headers); curl_easy_cleanup(curl); return result; }
            std::string authorization = "Authorization: Bearer " + owner;
            headers = curl_slist_append(headers, authorization.c_str());
            Network::Trust::secureEraseMemory(authorization.data(), authorization.size());
            headers = curl_slist_append(headers, ("If-Match: " + std::to_string(revision)).c_str());
        }
        const std::string url = base + path;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, local ? "http" : "https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, method == "DELETE" ? 1000L : method == "GET" ? 9000L : 3000L);
        if (cancelled) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancelled);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, +[](void *context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                return static_cast<const std::atomic<bool> *>(context)->load() ? 1 : 0;
            });
        }
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
        if (curl_easy_perform(curl) == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        for (auto *header = headers; header; header = header->next)
            if (header->data) Network::Trust::secureEraseMemory(header->data, std::strlen(header->data));
        curl_slist_free_all(headers); curl_easy_cleanup(curl);
        return result;
    }
    DirectoryPage directoryPage(const std::string &cursor) {
        DirectoryPage page;
        if (!cursor.empty() && !validCursor(cursor)) return page;
        const auto response = directoryRequest("GET", "/v1/listings" + (cursor.empty() ? "" : "?cursor=" + cursor));
        if (response.status != 200) return page;
        return decodeDirectoryPage(response.body, cursor);
    }
    DirectoryPage decodeDirectoryPage(const std::string &body, const std::string &cursor) {
        DirectoryPage page;
        try {
            const auto value = parse(body), rows = value.get("listings");
            if (rows.getLength() > 25) return page;
            std::set<std::string> identifiers;
            for (Size index = 0; index < rows.getLength(); ++index) {
                auto row = decode(rows.get(index));
                if (!identifiers.insert(row.id).second) return {};
                page.listings.push_back(std::move(row));
            }
            const auto next = value.get("nextCursor");
            if (next.getType() != Json::Value::Type::Null) {
                page.nextCursor = next.asString();
                if (!validCursor(page.nextCursor)) return {};
                if (!cursor.empty()) {
                    if (!validCursor(cursor)) return {};
                    const auto priorExpiry = std::stoull(cursor.substr(0, cursor.find('-')));
                    const auto nextExpiry = std::stoull(page.nextCursor.substr(0, page.nextCursor.find('-')));
                    if (nextExpiry < priorExpiry || (nextExpiry == priorExpiry
                        && page.nextCursor.substr(page.nextCursor.find('-') + 1) <= cursor.substr(cursor.find('-') + 1))) return {};
                }
            }
            page.available = true;
        } catch (...) { return {}; }
        return page;
    }
    void DirectoryBrowser::refresh() {
        if (pending.valid()) return;
        requested = Clock::now();
        pending = std::async(std::launch::async, directoryPage, cursor);
    }
    void DirectoryBrowser::next() {
        if (loading() || stale() || page.nextCursor.empty()) return;
        cursors.push_back(cursor); cursor = page.nextCursor; page = {}; refresh();
    }
    void DirectoryBrowser::previous() {
        if (loading() || cursors.empty()) return;
        cursor = cursors.back(); cursors.pop_back(); page = {}; refresh();
    }
    bool DirectoryBrowser::stale() const { return !page.available || received == Clock::time_point{} || Clock::now() - received >= std::chrono::seconds(30); }
    void DirectoryBrowser::update() {
        if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto next = pending.get();
            if (next.available) { page = std::move(next); received = Clock::now(); }
            else page.available = false;
        }
        if (Clock::now() - requested >= std::chrono::seconds(20)) refresh();
    }

    class DirectoryPublisher::Impl {
    public:
        std::mutex mutex;
        std::condition_variable changed;
        DirectoryListing desired;
        bool dirty = false;
        std::atomic<bool> stopping{false};
        std::thread worker;
        std::atomic<bool> &published;
        std::atomic<bool> &pending;
        std::atomic<Clock::time_point> &validUntil;
        Impl(std::atomic<bool> &published, std::atomic<bool> &pending, std::atomic<Clock::time_point> &validUntil)
            : published(published), pending(pending), validUntil(validUntil) {}
        void run() {
            std::string id, owner, previous;
            unsigned revision = 0;
            auto next = Clock::now();
            for (;;) {
                DirectoryListing listing;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait_until(lock, next, [&] { return stopping || dirty; });
                    if (stopping) break;
                    listing = desired; dirty = false;
                }
                const auto body = encode(listing);
                if (body.empty()) { next = Clock::now() + std::chrono::seconds(20); continue; }
                const auto attempted = Clock::now();
                pending.store(true);
                auto response = directoryRequest(id.empty() ? "POST" : "PUT",
                    id.empty() ? "/v1/listings" : "/v1/listings/" + id, body, owner, revision, &stopping);
                bool success = false;
                try {
                    if (response.status == 200 || response.status == 201) {
                        const auto value = parse(response.body);
                        const auto row = decode(value);
                        if (row.sessionId != listing.sessionId) throw std::invalid_argument("Invalid listing.");
                        if (id.empty()) {
                            owner = value.get("ownerToken").asString();
                            if (!hex(owner, 64)) throw std::invalid_argument("Invalid listing.");
                        }
                        id = row.id; revision = row.revision; success = true;
                    }
                } catch (...) {}
                Network::Trust::secureEraseMemory(response.body.data(), response.body.size());
                if (success) validUntil.store(attempted + std::chrono::seconds(55));
                published.store(success);
                // A transient outage must not orphan a still-owned live listing.
                // If a renewal acknowledgement was lost, a revision conflict does
                // not authorize overwriting it: retry until expiry is confirmed.
                if (!success && (response.status == 401 || id.empty())) {
                    id.clear(); Network::Trust::secureEraseMemory(owner.data(), owner.size()); owner.clear();
                }
                next = attempted + std::chrono::seconds(20);
                // Coalesce rapid state changes without an unbounded publication loop.
                std::unique_lock<std::mutex> lock(mutex);
                pending.store(dirty);
                changed.wait_for(lock, std::chrono::seconds(1), [&] { return stopping.load(); });
            }
            if (!id.empty()) (void) directoryRequest("DELETE", "/v1/listings/" + id, {}, owner, revision);
            Network::Trust::secureEraseMemory(owner.data(), owner.size());
            published.store(false);
            pending.store(false);
        }
    };
    DirectoryPublisher::DirectoryPublisher() : impl(std::make_unique<Impl>(published, pending, validUntil)) {}
    DirectoryPublisher::~DirectoryPublisher() { stop(); }
    void DirectoryPublisher::update(DirectoryListing listing) {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->stopping) return;
        if (encode(impl->desired) == encode(listing)) return;
        impl->desired = std::move(listing); impl->dirty = true; pending.store(true);
        if (!impl->worker.joinable()) impl->worker = std::thread([this] { impl->run(); });
        impl->changed.notify_all();
    }
    void DirectoryPublisher::retry() {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (pending.load() || impl->stopping.load()) return;
        impl->dirty = true; pending.store(true); impl->changed.notify_all();
    }
    void DirectoryPublisher::stop() {
        { std::lock_guard<std::mutex> lock(impl->mutex); impl->stopping = true; impl->changed.notify_all(); }
        if (impl->worker.joinable()) impl->worker.join();
    }
}
