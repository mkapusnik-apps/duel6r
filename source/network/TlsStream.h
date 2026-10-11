#ifndef DUEL6_NETWORK_TLSSTREAM_H
#define DUEL6_NETWORK_TLSSTREAM_H
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <openssl/ssl.h>
#include "SecureSession.h"

namespace Duel6::Network {
    // One verified TLS stream, serialized across the existing transport reader/writer.
    class TlsStream final : public SecureByteStream {
    public:
        static std::shared_ptr<TlsStream> connect(std::intptr_t socket, const std::string &identity,
                std::chrono::steady_clock::time_point deadline, const std::function<bool()> &cancelled);
        ~TlsStream();
        // >0 bytes, 0 closed, -1 fatal, -2 WANT_READ, -3 WANT_WRITE.
        std::ptrdiff_t read(std::uint8_t *data, std::size_t size) override;
        std::ptrdiff_t write(const std::uint8_t *data, std::size_t size) override;
        bool pending() const override;
    private:
        TlsStream() = default;
        SSL_CTX *context = nullptr;
        SSL *stream = nullptr;
        mutable std::mutex mutex;
        const std::uint8_t *retryWrite = nullptr;
        std::size_t retryWriteSize = 0;
        bool failed = false;
        int outcome(int count);
    };
}
#endif
