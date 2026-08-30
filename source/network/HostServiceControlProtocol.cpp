#include "HostServiceControlProtocol.h"

namespace Duel6::Network {
    namespace {
        std::array<std::uint8_t, HostServiceControlMessageBytes> encode(std::uint8_t kind) {
            return {{
                    static_cast<std::uint8_t>((HostServiceControlMagic >> 24u) & 0xffu),
                    static_cast<std::uint8_t>((HostServiceControlMagic >> 16u) & 0xffu),
                    static_cast<std::uint8_t>((HostServiceControlMagic >> 8u) & 0xffu),
                    static_cast<std::uint8_t>(HostServiceControlMagic & 0xffu),
                    HostServiceControlVersion,
                    kind,
                    0,
                    0
            }};
        }

        void writeU64(std::uint8_t *target, std::uint64_t value) {
            for (unsigned index = 0; index < 8; ++index)
                target[index] = static_cast<std::uint8_t>((value >> ((7u - index) * 8u)) & 0xffu);
        }

        std::uint64_t readU64(const std::uint8_t *source) {
            std::uint64_t value = 0;
            for (unsigned index = 0; index < 8; ++index) value = (value << 8u) | source[index];
            return value;
        }

        bool validEnvelope(const std::uint8_t *message, std::size_t size) noexcept {
            if (message == nullptr || size != HostServiceControlMessageBytes) return false;
            const std::uint32_t magic = (static_cast<std::uint32_t>(message[0]) << 24u)
                                        | (static_cast<std::uint32_t>(message[1]) << 16u)
                                        | (static_cast<std::uint32_t>(message[2]) << 8u)
                                        | static_cast<std::uint32_t>(message[3]);
            return magic == HostServiceControlMagic && message[4] == HostServiceControlVersion
                   && message[6] == 0 && message[7] == 0;
        }
    }

    std::array<std::uint8_t, HostServiceStatusMessageBytes> encodeHostServiceStatus(
            HostServiceStatusCode status, std::uint64_t monotonicNanoseconds) {
        std::array<std::uint8_t, HostServiceStatusMessageBytes> message{};
        const auto envelope = encode(static_cast<std::uint8_t>(status));
        for (std::size_t index = 0; index < envelope.size(); ++index) message[index] = envelope[index];
        writeU64(message.data() + HostServiceControlMessageBytes, monotonicNanoseconds);
        return message;
    }

    std::array<std::uint8_t, HostServiceControlMessageBytes> encodeHostServiceCommand(
            HostServiceCommandCode command) {
        return encode(static_cast<std::uint8_t>(command));
    }

    bool decodeHostServiceStatus(const std::uint8_t *message, std::size_t size,
                                 HostServiceStatusCode &status,
                                 std::uint64_t &monotonicNanoseconds) noexcept {
        if (size != HostServiceStatusMessageBytes
            || !validEnvelope(message, HostServiceControlMessageBytes)) return false;
        switch (static_cast<HostServiceStatusCode>(message[5])) {
            case HostServiceStatusCode::HostManifestInvalid:
            case HostServiceStatusCode::PortUnavailable:
            case HostServiceStatusCode::StartFailed:
            case HostServiceStatusCode::Ready:
                status = static_cast<HostServiceStatusCode>(message[5]);
                monotonicNanoseconds = readU64(message + HostServiceControlMessageBytes);
                return true;
        }
        return false;
    }

    bool decodeHostServiceCommand(const std::uint8_t *message, std::size_t size,
                                  HostServiceCommandCode &command) noexcept {
        if (!validEnvelope(message, size)
            || message[5] != static_cast<std::uint8_t>(HostServiceCommandCode::Stop)) return false;
        command = HostServiceCommandCode::Stop;
        return true;
    }
}
