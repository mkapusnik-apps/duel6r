#include "ResolverProtocol.h"
#include "Protocol.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

namespace {
    bool readAll(std::uint8_t *target, std::size_t size) {
        while (size > 0) {
            std::size_t count = std::fread(target, 1, size, stdin);
            if (count == 0) return false;
            target += count;
            size -= count;
        }
        return true;
    }

    bool writeAll(const std::uint8_t *source, std::size_t size) {
        while (size > 0) {
            std::size_t count = std::fwrite(source, 1, size, stdout);
            if (count == 0) return false;
            source += count;
            size -= count;
        }
        return std::fflush(stdout) == 0;
    }
}

int main() {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) return 2;
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 2;
#endif

    std::array<std::uint8_t, Duel6::Network::ResolverProtocol::HeaderBytes> request{};
    if (!readAll(request.data(), request.size())) return 2;
    std::uint32_t magic = Duel6::Network::ResolverProtocol::readU32(request.data());
    std::uint32_t hostLength = Duel6::Network::ResolverProtocol::readU32(request.data() + 4);
    if (magic != Duel6::Network::ResolverProtocol::RequestMagic || hostLength == 0
        || hostLength > Duel6::Network::MaxProtocolStringBytes) return 2;
    std::string host(hostLength, '\0');
    if (!readAll(reinterpret_cast<std::uint8_t *>(host.data()), host.size())) return 2;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *addresses = nullptr;
    int resolution = getaddrinfo(host.c_str(), nullptr, &hints, &addresses);
    std::vector<std::array<std::uint8_t, 4>> resolved;
    if (resolution == 0) {
        for (addrinfo *address = addresses; address != nullptr
             && resolved.size() < Duel6::Network::ResolverProtocol::MaxAddresses; address = address->ai_next) {
            if (address->ai_family == AF_INET && address->ai_addrlen >= sizeof(sockaddr_in)) {
                std::array<std::uint8_t, 4> bytes{};
                const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(address->ai_addr);
                std::memcpy(bytes.data(), &ipv4->sin_addr.s_addr, bytes.size());
                resolved.push_back(bytes);
            }
        }
    }
    if (addresses) freeaddrinfo(addresses);

    std::array<std::uint8_t, Duel6::Network::ResolverProtocol::HeaderBytes> response{};
    Duel6::Network::ResolverProtocol::writeU32(response.data(), Duel6::Network::ResolverProtocol::ResponseMagic);
    Duel6::Network::ResolverProtocol::writeU32(response.data() + 4,
                                                resolution == 0 && !resolved.empty() ? 0u : 1u);
    Duel6::Network::ResolverProtocol::writeU32(response.data() + 8,
                                                static_cast<std::uint32_t>(resolved.size()));
    if (!writeAll(response.data(), response.size())) return 2;
    for (const auto &address: resolved) {
        if (!writeAll(address.data(), address.size())) return 2;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
