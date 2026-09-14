#include "HostServiceControlProtocol.h"

#include <algorithm>
#include <stdexcept>

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

        void writeU32(std::uint8_t *target, std::uint32_t value) {
            for (unsigned index = 0; index < 4; ++index)
                target[index] = static_cast<std::uint8_t>((value >> ((3u - index) * 8u)) & 0xffu);
        }

        std::uint32_t readU32(const std::uint8_t *source) {
            std::uint32_t value = 0;
            for (unsigned index = 0; index < 4; ++index) value = (value << 8u) | source[index];
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
        if (!validEnvelope(message, size)) return false;
        switch (static_cast<HostServiceCommandCode>(message[5])) {
            case HostServiceCommandCode::Stop:
            case HostServiceCommandCode::EndSession:
            case HostServiceCommandCode::Ready:
            case HostServiceCommandCode::NotReady:
                command = static_cast<HostServiceCommandCode>(message[5]);
                return true;
        }
        return false;
    }

    std::vector<std::uint8_t> encodeHostServicePayload(const std::vector<std::uint8_t> &payload) {
        if (payload.empty() || payload.size() > HostServiceMaximumPayloadBytes)
            throw std::invalid_argument("Invalid host-service payload size");
        std::vector<std::uint8_t> message(HostServicePayloadHeaderBytes + payload.size());
        writeU32(message.data(), HostServicePayloadMagic);
        message[4] = HostServicePayloadVersion;
        message[5] = 0;
        message[6] = 0;
        message[7] = 0;
        writeU32(message.data() + 8, static_cast<std::uint32_t>(payload.size()));
        std::copy(payload.begin(), payload.end(), message.begin() + HostServicePayloadHeaderBytes);
        return message;
    }

    bool decodeHostServicePayloadHeader(const std::uint8_t *message, std::size_t size,
                                        std::size_t &payloadBytes) noexcept {
        if (!message || size < HostServicePayloadHeaderBytes || readU32(message) != HostServicePayloadMagic
            || message[4] != HostServicePayloadVersion || message[5] != 0 || message[6] != 0 || message[7] != 0)
            return false;
        payloadBytes = readU32(message + 8);
        return payloadBytes > 0 && payloadBytes <= HostServiceMaximumPayloadBytes;
    }
}
