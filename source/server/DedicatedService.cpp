#include "DedicatedService.h"
#include "../network/NetworkTrustPolicy.h"
#include <array>
#include <cstring>
#include <stdexcept>
#ifndef D6R_TRANSPORT_WINDOWS
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace Duel6::Server {
    std::shared_ptr<Network::PublicSession::Secret> loadInvitation(const std::string &path) {
#ifndef D6R_TRANSPORT_WINDOWS
        const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) throw std::runtime_error("Dedicated invitation configuration invalid");
        struct stat status{};
        auto result = std::make_shared<Network::PublicSession::Secret>();
        std::array<char, 257> bytes{};
        const bool safe = fstat(fd, &status) == 0 && S_ISREG(status.st_mode)
                          && status.st_uid == geteuid() && (status.st_mode & 0077) == 0
                          && status.st_size >= 1 && status.st_size <= 256;
        const auto count = safe ? read(fd, bytes.data(), bytes.size()) : -1;
        close(fd);
        if (count > 0) result->value.assign(bytes.data(), static_cast<std::size_t>(count));
        Network::Trust::secureEraseMemory(bytes.data(), bytes.size());
        if (!safe || count != status.st_size || !Network::PublicSession::validInvite(result->value))
            throw std::runtime_error("Dedicated invitation configuration invalid");
        return result;
#else
        throw std::runtime_error("Dedicated service requires Linux");
#endif
    }
#ifndef D6R_TRANSPORT_WINDOWS
    namespace {
        sockaddr_un addressFor(const std::string &path) {
            sockaddr_un address{}; address.sun_family = AF_UNIX;
            if (path.empty() || path.front() != '/' || path.size() >= sizeof(address.sun_path))
                throw std::runtime_error("Readiness socket path invalid");
            std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
            return address;
        }
    }
#endif
    DedicatedReadiness::DedicatedReadiness(const std::string &value) : path(value) {
#ifndef D6R_TRANSPORT_WINDOWS
        auto address = addressFor(path);
        socket = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (socket < 0) throw std::runtime_error("Readiness socket unavailable");
        // Never unlink an existing path: a duplicate process must fail rather than replace a live probe.
        const auto mask = umask(0077);
        const bool bound = bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
        umask(mask);
        if (!bound || listen(socket, 4) != 0) {
            close(socket); socket = -1;
            if (bound) unlink(path.c_str());
            throw std::runtime_error("Readiness socket unavailable");
        }
#else
        throw std::runtime_error("Dedicated service requires Linux");
#endif
    }
    DedicatedReadiness::~DedicatedReadiness() {
#ifndef D6R_TRANSPORT_WINDOWS
        if (socket >= 0) { close(socket); unlink(path.c_str()); }
#endif
    }
    void DedicatedReadiness::poll(bool ready) {
#ifndef D6R_TRANSPORT_WINDOWS
        for (int i = 0; i < 4; ++i) {
            const int peer = accept4(socket, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (peer < 0) break;
            if (ready) (void) send(peer, "ready\n", 6, MSG_NOSIGNAL);
            close(peer);
        }
#endif
    }
    bool DedicatedReadiness::check(const std::string &path) {
#ifndef D6R_TRANSPORT_WINDOWS
        try {
            auto address = addressFor(path);
            const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
            if (fd < 0) return false;
            bool ok = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
            pollfd descriptor{fd, POLLIN, 0};
            if (ok) ok = ::poll(&descriptor, 1, 1000) > 0;
            std::array<char, 7> bytes{};
            if (ok) ok = recv(fd, bytes.data(), bytes.size(), 0) == 6
                         && std::memcmp(bytes.data(), "ready\n", 6) == 0;
            close(fd); return ok;
        } catch (...) { return false; }
#else
        return false;
#endif
    }
}
