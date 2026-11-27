#include "HostDirectory.h"
#include "../network/ListeningSelection.h"
#include "DirectoryOrigin.h"
#include "../json/JsonParser.h"
#include "../network/NetworkTrustPolicy.h"
#include "../network/StateReplication.h"
#ifdef __APPLE__
#include "../network/SecureSession.h"
#endif
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
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
        constexpr std::string_view RosterAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

        bool validRoster(const std::vector<std::string> &names, unsigned occupancy) {
            return names.size() <= Network::MaxNetworkPlayers && names.size() <= occupancy
                && std::all_of(names.begin(), names.end(), Network::Trust::validParticipantName);
        }

        std::string rosterNameToWire(std::string_view name) {
            std::string output;
            unsigned bits = 0;
            std::uint32_t packed = 0;
            for (unsigned char byte: name) {
                packed = (packed << 8) | byte;
                bits += 8;
                while (bits >= 6) {
                    bits -= 6;
                    output += RosterAlphabet[(packed >> bits) & 63u];
                }
            }
            if (bits != 0) output += RosterAlphabet[(packed << (6 - bits)) & 63u];
            return output;
        }

        std::optional<std::vector<std::string>> decodeRoster(const Json::Value &value, unsigned occupancy) {
            try {
                const auto roster = value.get("connectedPlayers");
                if (roster.getType() != Json::Value::Type::Array) return std::nullopt;
                const auto count = roster.getLength();
                if (count > Network::MaxNetworkPlayers || count > occupancy) return std::nullopt;
                std::vector<std::string> names;
                for (Size index = 0; index < count; ++index) {
                    const auto wire = roster.get(index).asString();
                    if (wire.size() < 2 || wire.size() > (Network::Trust::MaxParticipantNameBytes * 8 + 5) / 6)
                        return std::nullopt;
                    std::string name;
                    unsigned bits = 0;
                    std::uint32_t packed = 0;
                    for (char ch: wire) {
                        const auto digit = RosterAlphabet.find(ch);
                        if (digit == std::string_view::npos) return std::nullopt;
                        packed = (packed << 6) | static_cast<std::uint32_t>(digit);
                        bits += 6;
                        if (bits >= 8) {
                            bits -= 8;
                            name += static_cast<char>((packed >> bits) & 255u);
                        }
                    }
                    if (!Network::Trust::validParticipantName(name) || rosterNameToWire(name) != wire)
                        return std::nullopt;
                    names.push_back(std::move(name));
                }
                return names;
            } catch (...) { return std::nullopt; }
        }

        std::string encodeRoster(const DirectoryListing &row) {
            if (!row.connectedPlayers || !validRoster(*row.connectedPlayers, row.players)) return {};
            std::string output = ",\"connectedPlayers\":[";
            for (std::size_t index = 0; index < row.connectedPlayers->size(); ++index) {
                if (index) output += ',';
                output += "\"" + rosterNameToWire((*row.connectedPlayers)[index]) + "\"";
            }
            return output + ']';
        }

        bool supportsRoster(const Json::Value &acknowledgement) {
            try { return acknowledgement.get("connectedRosterSupported").asBoolean(); }
            catch (...) { return false; }
        }

        bool hex(const std::string &value, std::size_t length = 32) {
            return value.size() == length && std::all_of(value.begin(), value.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
        }
        bool validCursor(const std::string &input, bool mixed = false) {
            if (mixed && input.rfind("m2_", 0) != 0) return false;
            const auto value = mixed ? input.substr(3) : input;
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
        DirectoryListing decode(const Json::Value &value, bool mixed = false) {
            DirectoryListing row;
            row.id = value.get("id").asString(); row.sessionId = value.get("sessionId").asString();
            row.mode = value.get("mode").asString(); row.phase = value.get("phase").asString();
            row.endpoint.host = value.get("address").asString();
            row.passwordRequired = value.get("passwordRequired").asBoolean();
            if (mixed) {
                row.hosting = value.get("hosting").asString();
                row.protection = value.get("protection").asString();
                row.admission = value.get("admission").asString();
                if ((row.hosting != "player-hosted" && row.hosting != "dedicated")
                    || row.protection != (row.passwordRequired ? "password" : "none")
                    || (row.admission != "open" && row.admission != "closed" && row.admission != "full" && row.admission != "busy"))
                    throw std::invalid_argument("Invalid directory classification.");
            } else row.protection = row.passwordRequired ? "password" : "none";
            const auto integer = [&](const char *key) {
                const double number = value.get(key).asDouble();
                if (!std::isfinite(number) || std::floor(number) != number || number < 0 || number > 2147483647)
                    throw std::invalid_argument("Invalid directory response.");
                return static_cast<int>(number);
            };
            const int port = integer("port");
            const int players = integer("players"), capacity = integer("capacity");
            if (!hex(row.id) || !hex(row.sessionId) || row.sessionId == std::string(32, '0') || port < 1 || port > 65535
                || players < (row.dedicated() && row.phase == "idle" ? 0 : 1) || capacity < 1 || capacity < players || capacity > 15
                || (row.dedicated() ? !Network::PublicSession::validDirectoryFrontend(row.endpoint.host)
                    : Network::Trust::classifyIpv4Literal(row.endpoint.host) == Network::Trust::EndpointScope::Unsupported
                        || Network::Trust::classifyIpv4Literal(row.endpoint.host) == Network::Trust::EndpointScope::Invalid)
                || (row.mode != "deathmatch" && row.mode != "predator" && row.mode != "teams")
                || (row.phase != "lobby" && row.phase != "first-round" && row.phase != "started" && row.phase != "final-round" && row.phase != "closed"
                    && !(row.dedicated() && (row.phase == "idle" || row.phase == "reserved" || row.phase == "cleanup")))
                || (mixed && ((row.admission == "open" && players >= capacity)
                    || (row.admission == "full" && players != capacity)
                    || (row.phase == "idle" && (players != 0 || row.admission != "open"))
                    || ((row.phase == "closed" || row.phase == "reserved") && row.admission != "closed")
                    || ((row.phase == "cleanup") != (row.admission == "busy")))))
                throw std::invalid_argument("Invalid directory response.");
            row.endpoint.port = static_cast<std::uint16_t>(port); row.players = players; row.capacity = capacity;
            row.expiresAt = value.get("expiresAt").asDouble();
            if (!std::isfinite(row.expiresAt) || row.expiresAt < 0 || row.expiresAt > 253402300799999.0)
                throw std::invalid_argument("Invalid directory response.");
            const int revision = integer("revision");
            if (revision < 1) throw std::invalid_argument("Invalid directory response.");
            row.revision = revision;
            if (mixed) row.connectedPlayers = decodeRoster(value, row.players);
            return row;
        }
        bool jsonWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

        // The legacy parser does not consume JSON escapes. Inside the optional
        // roster only, normalize string tokens to the wire alphabet or an invalid
        // sentinel. Keep punctuation and every core field for the existing parser.
        std::string safeRosterString(std::string_view raw) {
            std::string text;
            bool inAlphabet = true;
            for (std::size_t i = 0; i < raw.size(); ++i) {
                unsigned value = static_cast<unsigned char>(raw[i]);
                if (value < 0x20) throw std::invalid_argument("Invalid directory JSON string.");
                if (value == '\\') {
                    if (++i == raw.size()) throw std::invalid_argument("Invalid directory JSON string.");
                    switch (raw[i]) {
                        case 'u':
                            value = 0;
                            for (int digit = 0; digit < 4; ++digit) {
                                if (++i == raw.size()) throw std::invalid_argument("Invalid directory JSON escape.");
                                constexpr std::string_view digits = "0123456789abcdef";
                                const auto nibble = digits.find(static_cast<char>(std::tolower(static_cast<unsigned char>(raw[i]))));
                                if (nibble == std::string_view::npos) throw std::invalid_argument("Invalid directory JSON escape.");
                                value = value * 16 + static_cast<unsigned>(nibble);
                            }
                            break;
                        case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
                            // None of these escapes can decode to a base64url character.
                            inAlphabet = false;
                            continue;
                        default: throw std::invalid_argument("Invalid directory JSON escape.");
                    }
                }
                if (value > 127 || RosterAlphabet.find(static_cast<char>(value)) == std::string_view::npos)
                    inAlphabet = false;
                else text += static_cast<char>(value);
            }
            return inAlphabet ? text : "!";
        }

        Json::Value parse(const std::string &body, bool mixedPage = false) {
            if (body.size() > 65536) throw std::invalid_argument("Directory response too large.");
            unsigned depth = 0;
            bool quoted = false, escaped = false, stringHasEscape = false, ended = false;
            bool inListings = false, inRoster = false;
            std::size_t stringStart = 0, listingsStart = std::string::npos, keptFrom = 0;
            std::string normalized;
            for (std::size_t index = 0; index < body.size(); ++index) {
                const char c = body[index];
                if (ended) {
                    if (!jsonWhitespace(c))
                        throw std::invalid_argument("Trailing directory response data.");
                    continue;
                }
                if (quoted) {
                    if (escaped) escaped = false;
                    else if (c == '\\') escaped = stringHasEscape = true;
                    else if (c == '"') {
                        quoted = false;
                        if (!mixedPage) continue;
                        const auto token = std::string_view(body).substr(stringStart + 1, index - stringStart - 1);
                        if (inRoster && stringHasEscape) {
                            normalized.append(body, keptFrom, stringStart + 1 - keptFrom);
                            normalized += safeRosterString(token);
                            keptFrom = index;
                        } else if (!inRoster && ((depth == 1 && token == "listings")
                                   || (inListings && depth == 3 && token == "connectedPlayers"))) {
                            const auto colon = body.find_first_not_of(" \t\r\n", index + 1);
                            if (colon == std::string::npos || body[colon] != ':') continue;
                            if (depth == 1) listingsStart = body.find_first_not_of(" \t\r\n", colon + 1);
                            else inRoster = true;
                        }
                    }
                    continue;
                }
                if (inRoster && depth == 3 && (c == ',' || c == '}' || c == ']')) inRoster = false;
                if (c == '"') { quoted = true; stringHasEscape = false; stringStart = index; }
                else if (c == '[' || c == '{') {
                    if (index == listingsStart && c == '[') inListings = true;
                    if (++depth > 8) throw std::invalid_argument("Directory response nesting exceeds limit.");
                } else if (c == ']' || c == '}') {
                    if (depth == 0) throw std::invalid_argument("Invalid directory response.");
                    if (depth == 2) inListings = false;
                    --depth;
                    if (depth == 0) ended = true;
                }
            }
            if (!ended || depth != 0 || quoted) throw std::invalid_argument("Invalid directory response.");
            if (keptFrom != 0) {
                normalized.append(body, keptFrom, std::string::npos);
                return Json::Parser().parse(std::vector<Uint8>(normalized.begin(), normalized.end()));
            }
            return Json::Parser().parse(std::vector<Uint8>(body.begin(), body.end()));
        }
        std::string encode(const DirectoryListing &row) {
            // Only validated local fields and fixed enumerations enter this serializer.
            if (!hex(row.sessionId) || row.sessionId == std::string(32, '0')
                || (row.hosting != "player-hosted" && row.hosting != "dedicated")
                || row.endpoint.port == 0 || row.capacity < 1 || row.capacity > 15 || row.players > row.capacity
                || row.players < (row.dedicated() && row.phase == "idle" ? 0u : 1u)
                || row.protection != (row.passwordRequired ? "password" : "none")
                || (row.dedicated() && ((row.admissionState() != "open" && row.admissionState() != "full"
                        && row.admissionState() != "closed" && row.admissionState() != "busy")
                    || (row.phase == "idle" && (row.players != 0 || row.admissionState() != "open"))
                    || ((row.phase == "reserved" || row.phase == "closed") && row.admissionState() != "closed")
                    || ((row.phase == "cleanup") != (row.admissionState() == "busy"))
                    || (row.admissionState() == "open" && row.players >= row.capacity)
                    || (row.admissionState() == "full" && row.players != row.capacity)))
                || (row.dedicated() ? !Network::PublicSession::validDirectoryFrontend(row.endpoint.host)
                    : row.endpoint.host.find_first_not_of("0123456789.") != std::string::npos)
                || (row.phase != "lobby" && row.phase != "first-round" && row.phase != "started" && row.phase != "final-round" && row.phase != "closed"
                    && !(row.dedicated() && (row.phase == "idle" || row.phase == "reserved" || row.phase == "cleanup")))
                || (row.mode != "deathmatch" && row.mode != "predator" && row.mode != "teams")) return {};
            return "{\"sessionId\":\"" + row.sessionId + "\",\"address\":\"" + row.endpoint.host
                + "\",\"port\":" + std::to_string(row.endpoint.port) + ",\"mode\":\"" + row.mode
                + "\",\"players\":" + std::to_string(row.players) + ",\"capacity\":" + std::to_string(row.capacity)
                + ",\"passwordRequired\":" + (row.passwordRequired ? "true" : "false")
                + ",\"phase\":\"" + row.phase + "\""
                + (row.dedicated() ? ",\"hosting\":\"dedicated\",\"protection\":\"" + row.protection
                    + "\",\"admission\":\"" + row.admissionState() + "\"" : "") + "}";
        }
    }
    std::string directorySessionId(std::uint64_t id) {
        std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(32) << id; return out.str();
    }
    std::optional<std::vector<std::string>> directoryConnectedPlayers(
            const Network::Replication::CanonicalState &state, unsigned occupancy) {
        namespace R = Network::Replication;
        std::vector<const R::PlayerState *> connected;
        for (const auto &player: state.players) {
            if (player.lifeState == R::LifeState::Departed) continue;
            const auto owner = std::find_if(state.participants.begin(), state.participants.end(), [&](const auto &p) {
                return p.participantId == player.ownerParticipantId && p.connection == R::ConnectionState::Connected
                    && std::find(p.ownedPlayerIds.begin(), p.ownedPlayerIds.end(), player.playerId) != p.ownedPlayerIds.end();
            });
            if (owner != state.participants.end()) connected.push_back(&player);
        }
        std::sort(connected.begin(), connected.end(), [](const auto *a, const auto *b) {
            return a->rosterPosition < b->rosterPosition;
        });
        std::vector<std::string> names;
        for (const auto *player: connected) names.push_back(player->displayName);
        if (!validRoster(names, occupancy)) return std::nullopt;
        return names;
    }
    bool directoryOriginMatchesFrontend(const std::string &origin, const Network::Endpoint &frontend) {
        // Use the same maintained URL parser as HTTP, not a second URL grammar or
        // textual port comparison. No transfer, resolution or trust lookup occurs.
        std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> url(curl_url(), curl_url_cleanup);
        if (!url || curl_url_set(url.get(), CURLUPART_URL, origin.c_str(), 0) != CURLUE_OK) return false;
        char *rawHost = nullptr, *rawPort = nullptr;
        const auto hostStatus = curl_url_get(url.get(), CURLUPART_HOST, &rawHost, 0);
        const auto portStatus = curl_url_get(url.get(), CURLUPART_PORT, &rawPort, CURLU_DEFAULT_PORT);
        std::unique_ptr<char, decltype(&curl_free)> host(rawHost, curl_free), port(rawPort, curl_free);
        if (hostStatus != CURLUE_OK || portStatus != CURLUE_OK || !host || !port) return false;
        std::string originHost(host.get()), frontendHost(frontend.host);
        const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        std::transform(originHost.begin(), originHost.end(), originHost.begin(), lower);
        std::transform(frontendHost.begin(), frontendHost.end(), frontendHost.begin(), lower);
        try {
            std::size_t consumed = 0;
            const std::string numericPort(port.get());
            const auto effectivePort = std::stoul(numericPort, &consumed, 10);
            return consumed == numericPort.size() && effectivePort == frontend.port && originHost == frontendHost;
        } catch (...) { return false; }
    }
    DirectoryResponse directoryRequest(const std::string &method, const std::string &path,
                                       const std::string &body, const std::string &owner, unsigned revision,
                                       const std::atomic<bool> *cancelled) {
        DirectoryResponse result;
#ifdef __APPLE__
        // Browse can be entered without starting a gameplay socket. Its HTTPS
        // initialization must obey the same physical capability admission gate.
        if (!Network::SecureSession::supported()) return result;
#endif
        static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (initialized != CURLE_OK || !(curl_version_info(CURLVERSION_NOW)->features & CURL_VERSION_ASYNCHDNS)) return result;
        const auto base = directoryOrigin(std::getenv("D6R_DIRECTORY_URL"), std::getenv("D6R_DIRECTORY_ALLOW_HTTP"));
        if (base.empty()) return result;
        const bool local = base.rfind("http://", 0) == 0;
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
        if (!cursor.empty() && !validCursor(cursor, true)) return page;
        const auto response = directoryRequest("GET", "/v2/listings" + (cursor.empty() ? "" : "?cursor=" + cursor));
        if (response.status != 200) return page;
        return decodeDirectoryPage(response.body, cursor, true);
    }
    DirectoryPage decodeDirectoryPage(const std::string &body, const std::string &cursor, bool mixed) {
        DirectoryPage page;
        try {
            const auto value = parse(body, mixed), rows = value.get("listings");
            if (mixed && value.get("contract").asString() != "mixed-v2") return page;
            if (!cursor.empty() && !validCursor(cursor, mixed)) return page;
            if (rows.getLength() > 25) return page;
            std::set<std::string> identifiers;
            for (Size index = 0; index < rows.getLength(); ++index) {
                auto row = decode(rows.get(index), mixed);
                if (!identifiers.insert(row.id).second) return {};
                page.listings.push_back(std::move(row));
            }
            const auto next = value.get("nextCursor");
            if (next.getType() != Json::Value::Type::Null) {
                page.nextCursor = next.asString();
                if (!validCursor(page.nextCursor, mixed)) return {};
                if (!cursor.empty()) {
                    const auto offset = mixed ? 3 : 0;
                    const auto priorExpiry = std::stoull(cursor.substr(offset, cursor.find('-') - offset));
                    const auto nextExpiry = std::stoull(page.nextCursor.substr(offset, page.nextCursor.find('-') - offset));
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
        for (auto &row: page.listings) row.connectedPlayers.reset();
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
            else {
                page.available = false;
                for (auto &row: page.listings) row.connectedPlayers.reset();
            }
        }
        if (page.available) {
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const bool unconfirmed = stale() || loading();
            for (auto &row: page.listings)
                if (unconfirmed || row.expiresAt <= now) row.connectedPlayers.reset();
        }
        if (Clock::now() - requested >= std::chrono::seconds(20)) refresh();
    }

    class DirectoryPublisher::Impl {
    public:
        std::mutex mutex;
        std::condition_variable changed;
        DirectoryListing desired;
        std::vector<std::string> coverage;
        DirectoryPublisherDependencies dependencies;
        bool dirty = false;
        std::atomic<bool> stopping{false};
        std::thread worker;
        std::atomic<bool> &published;
        std::atomic<bool> &pending;
        std::atomic<Clock::time_point> &validUntil;
        Impl(std::atomic<bool> &published, std::atomic<bool> &pending, std::atomic<Clock::time_point> &validUntil)
            : published(published), pending(pending), validUntil(validUntil) {}
        void run() {
            std::string id, owner, session, collection;
            unsigned revision = 0;
            bool rosterSupported = false;
            bool rosterFollowUp = false;
            auto next = Clock::now();
            for (;;) {
                DirectoryListing listing;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait_until(lock, next, [&] { return stopping || dirty; });
                    if (stopping) break;
                    listing = desired; dirty = false;
                }
                const auto candidate = listing.dedicated() ? std::optional<std::string>(listing.endpoint.host) : Network::publicationAddress(
                        coverage.empty() ? std::vector<std::string>{listing.endpoint.host} : coverage,
                        dependencies.listeningAddresses());
                if (candidate) listing.endpoint.host = *candidate;
                const auto body = candidate ? encode(listing) : std::string();
                if (!id.empty() && session != listing.sessionId) {
                    // Old admission identity is already invalid in the authority.
                    // Lost removal expires normally; never reuse its owner on a new session.
                    (void) dependencies.request("DELETE", collection + "/" + id, {}, owner, revision, &stopping);
                    id.clear(); Network::Trust::secureEraseMemory(owner.data(), owner.size()); owner.clear();
                    published.store(false);
                    rosterFollowUp = false;
                }
                collection = listing.dedicated() ? "/v2/listings" : "/v1/listings";
                if (body.empty()) {
                    rosterFollowUp = false;
                    published.store(false);
                    if (!id.empty()) {
                        const auto removal = dependencies.request("DELETE", collection + "/" + id, {}, owner, revision, &stopping);
                        if (removal.status == 200 || removal.status == 204 || removal.status == 404) {
                            id.clear(); Network::Trust::secureEraseMemory(owner.data(), owner.size()); owner.clear();
                        }
                    }
                    pending.store(false);
                    next = Clock::now() + std::chrono::seconds(20); continue;
                }
                const auto attempted = Clock::now();
                pending.store(true);
                const auto method = id.empty() ? "POST" : "PUT";
                const auto path = id.empty() ? collection : collection + "/" + id;
                const auto roster = rosterSupported ? encodeRoster(listing) : std::string{};
                bool wroteRoster = !roster.empty();
                auto response = dependencies.request(method, path,
                    roster.empty() ? body : body.substr(0, body.size() - 1) + roster + "}", owner, revision, &stopping);
                if (!roster.empty() && response.status == 400 && !stopping) {
                    // A strict older instance rejected the extension before writing.
                    // Retry only that definite rejection, using the same owner and revision.
                    response = dependencies.request(method, path, body, owner, revision, &stopping);
                    wroteRoster = false;
                }
                bool success = false;
                bool sendRosterNext = false;
                try {
                    if (response.status == 200 || response.status == 201) {
                        const auto value = parse(response.body);
                        const auto row = decode(value, listing.dedicated());
                        if (row.sessionId != listing.sessionId || row.endpoint.host != listing.endpoint.host
                            || (!id.empty() && (row.id != id || row.revision != revision + 1))
                            || (id.empty() && row.revision != 1)
                            || row.endpoint.port != listing.endpoint.port || row.hosting != listing.hosting
                            || row.passwordRequired != listing.passwordRequired || row.phase != listing.phase
                            || row.players != listing.players || row.capacity != listing.capacity || row.mode != listing.mode
                            || (listing.dedicated() && row.admission != listing.admissionState()))
                            throw std::invalid_argument("Invalid listing.");
                        if (id.empty()) {
                            owner = value.get("ownerToken").asString();
                            if (!hex(owner, 64)) throw std::invalid_argument("Invalid listing.");
                        }
                        id = row.id; session = row.sessionId; revision = row.revision; success = true;
                        rosterSupported = supportsRoster(value);
                        sendRosterNext = rosterSupported && !wroteRoster && !rosterFollowUp && !encodeRoster(listing).empty();
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
                next = sendRosterNext ? Clock::now() : attempted + std::chrono::seconds(20);
                // A strict/capable fleet can accept the fallback repeatedly. Limit
                // each write cycle to one prompt repair before normal renewal.
                rosterFollowUp = sendRosterNext;
                // Coalesce rapid state changes without an unbounded publication loop.
                std::unique_lock<std::mutex> lock(mutex);
                pending.store(dirty);
                changed.wait_for(lock, std::chrono::seconds(1), [&] { return stopping.load(); });
            }
            if (!id.empty()) (void) dependencies.request("DELETE", collection + "/" + id, {}, owner, revision, nullptr);
            Network::Trust::secureEraseMemory(owner.data(), owner.size());
            published.store(false);
            pending.store(false);
        }
    };
    DirectoryPublisher::DirectoryPublisher(std::vector<std::string> listeningCoverage, DirectoryPublisherDependencies dependencies)
            : impl(std::make_unique<Impl>(published, pending, validUntil)) {
        impl->coverage = std::move(listeningCoverage);
        if (!dependencies.listeningAddresses) dependencies.listeningAddresses = Network::Trust::localListenerAddresses;
        if (!dependencies.request) dependencies.request = directoryRequest;
        auto request = std::move(dependencies.request);
        dependencies.request = [request = std::move(request)](const auto &method, const auto &path, const auto &body,
                const auto &owner, unsigned revision, const std::atomic<bool> *cancelled) {
            try { return request(method, path, body, owner, revision, cancelled); }
            catch (...) { return DirectoryResponse{}; }
        };
        impl->dependencies = std::move(dependencies);
    }
    DirectoryPublisher::~DirectoryPublisher() { stop(); }
    void DirectoryPublisher::update(DirectoryListing listing) {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->stopping) return;
        if (encode(impl->desired) == encode(listing) && impl->desired.connectedPlayers == listing.connectedPlayers) return;
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
