#ifndef DUEL6_NETWORK_TLSSTREAM_H
#define DUEL6_NETWORK_TLSSTREAM_H
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <openssl/ssl.h>

namespace Duel6::Network {
    // One verified TLS stream, serialized across the existing transport reader/writer.
    class TlsStream final {
    public:
        static std::shared_ptr<TlsStream> connect(std::intptr_t socket, const std::string &identity,
                std::chrono::steady_clock::time_point deadline, const std::function<bool()> &cancelled);
        ~TlsStream();
        // >0 bytes, 0 closed, -1 fatal, -2 nonblocking retry.
        int read(std::uint8_t *data, std::size_t size);
        int write(const std::uint8_t *data, std::size_t size);
    private:
        TlsStream() = default;
        SSL_CTX *context = nullptr;
        SSL *stream = nullptr;
        std::mutex mutex;
        int outcome(int count);
    };
}
#endif
