#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
using Socket = SOCKET;
constexpr Socket Invalid = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
using Socket = int;
constexpr Socket Invalid = -1;
#endif
#include "source/network/SecureSession.h"
#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
struct OwnedSocket {
    Socket value = Invalid;
    ~OwnedSocket() {
#ifdef D6R_TRANSPORT_WINDOWS
        if (value != Invalid) closesocket(value);
#else
        if (value != Invalid) ::close(value);
#endif
    }
};
void reply(std::int32_t size) {
    const auto value = static_cast<std::uint32_t>(size);
    const char bytes[] = {char(value >> 24), char(value >> 16), char(value >> 8), char(value)};
    std::cout.write(bytes, 4); std::cout.flush();
}
template<typename Operation> std::ptrdiff_t progress(Operation operation) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto result = operation();
        if (result != -2) return result;
        std::this_thread::sleep_for(2ms);
    }
    return -1;
}
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
#ifdef D6R_TRANSPORT_WINDOWS
    WSADATA runtime{};
    if (WSAStartup(MAKEWORD(2, 2), &runtime) != 0) return 2;
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
        const auto port = std::stoul(argv[1]);
        if (port == 0 || port > 65535 || !Duel6::Network::SecureSession::supported()) return 2;
        OwnedSocket listener{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        sockaddr_in address{}; address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(static_cast<std::uint16_t>(port));
        if (listener.value == Invalid || ::bind(listener.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
            || ::listen(listener.value, 1) != 0) return 2;
        std::cout.put('\1'); std::cout.flush();
        fd_set incoming; FD_ZERO(&incoming); FD_SET(listener.value, &incoming);
        timeval timeout{}; timeout.tv_sec = 10;
        if (select(static_cast<int>(listener.value + 1), &incoming, nullptr, nullptr, &timeout) <= 0) return 2;
        OwnedSocket peer{::accept(listener.value, nullptr, nullptr)};
        if (peer.value == Invalid) return 2;
#ifdef D6R_TRANSPORT_WINDOWS
        u_long nonblocking = 1;
        if (ioctlsocket(peer.value, FIONBIO, &nonblocking) != 0) return 2;
#else
        if (fcntl(peer.value, F_SETFL, fcntl(peer.value, F_GETFL) | O_NONBLOCK) != 0) return 2;
#endif
        Duel6::Network::SecureSession session(static_cast<std::intptr_t>(peer.value), true, {});
        if (!session.handshake(std::chrono::steady_clock::now() + 10s, [] { return false; })) return 2;
        std::cout.put('\2'); std::cout.flush();
        char command;
        while (std::cin.get(command)) {
            std::array<unsigned char, 4> header{};
            if (!std::cin.read(reinterpret_cast<char *>(header.data()), header.size())) return 2;
            const std::uint32_t size = (std::uint32_t(header[0]) << 24) | (std::uint32_t(header[1]) << 16)
                | (std::uint32_t(header[2]) << 8) | header[3];
            if (size == 0 || size > 1024 * 1024) return 2;
            std::vector<unsigned char> bytes(size);
            if (command == 'R') {
                const auto count = progress([&] { return session.receive(bytes.data(), bytes.size()); });
                reply(static_cast<std::int32_t>(count));
                if (count > 0) { std::cout.write(reinterpret_cast<const char *>(bytes.data()), count); std::cout.flush(); }
            } else if (command == 'W') {
                if (!std::cin.read(reinterpret_cast<char *>(bytes.data()), size)) return 2;
                std::size_t offset = 0;
                while (offset < bytes.size()) {
                    const auto count = progress([&] { return session.send(bytes.data() + offset, bytes.size() - offset); });
                    if (count <= 0) { reply(-1); return 2; }
                    offset += static_cast<std::size_t>(count);
                }
                reply(0);
            } else return 2;
        }
        return 0;
    } catch (...) { return 2; }
}
