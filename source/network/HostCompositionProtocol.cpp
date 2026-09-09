#include "HostCompositionProtocol.h"

#include <limits>
#include <stdexcept>

#include "NetworkTrustPolicy.h"

namespace Duel6::Network::HostComposition {
    namespace {
        class Writer {
        public:
            void u8(std::uint8_t value) { bytes.push_back(value); }
            void u16(std::uint16_t value) {
                bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
                bytes.push_back(static_cast<std::uint8_t>(value));
            }
            void u32(std::uint32_t value) {
                for (unsigned shift: {24u, 16u, 8u, 0u})
                    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(std::uint64_t value) {
                for (unsigned shift: {56u, 48u, 40u, 32u, 24u, 16u, 8u, 0u})
                    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void boolean(bool value) { u8(value ? 1 : 0); }
            void text(const std::string &value, std::size_t maximum) {
                if (value.size() > maximum || value.size() > std::numeric_limits<std::uint16_t>::max())
                    throw std::invalid_argument("Host composition text is too long");
                u16(static_cast<std::uint16_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end());
            }
            std::vector<std::uint8_t> take() { return std::move(bytes); }
        private:
            std::vector<std::uint8_t> bytes;
        };

        class Reader {
        public:
            explicit Reader(const std::vector<std::uint8_t> &bytes) : bytes(bytes) {}
            std::uint8_t u8() {
                require(1); return bytes[offset++];
            }
            std::uint16_t u16() {
                require(2); const auto value = static_cast<std::uint16_t>(bytes[offset] << 8u | bytes[offset + 1]);
                offset += 2; return value;
            }
            std::uint32_t u32() {
                require(4); std::uint32_t value = 0;
                for (unsigned index = 0; index < 4; ++index) value = value << 8u | bytes[offset++];
                return value;
            }
            std::uint64_t u64() {
                require(8); std::uint64_t value = 0;
                for (unsigned index = 0; index < 8; ++index) value = value << 8u | bytes[offset++];
                return value;
            }
            bool boolean() {
                const auto value = u8();
                if (value > 1) throw std::invalid_argument("Invalid host composition boolean");
                return value != 0;
            }
            std::string text(std::size_t maximum) {
                const auto size = u16();
                if (size > maximum) throw std::invalid_argument("Host composition text is too long");
                require(size); std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                                 bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
                offset += size; return value;
            }
            std::vector<std::uint8_t> rest() {
                std::vector<std::uint8_t> value(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
                offset = bytes.size(); return value;
            }
            bool done() const { return offset == bytes.size(); }
        private:
            const std::vector<std::uint8_t> &bytes;
            std::size_t offset = 0;
            void require(std::size_t count) {
                if (count > bytes.size() - offset) throw std::invalid_argument("Truncated host composition message");
            }
        };

        void envelope(Writer &writer, Kind kind) {
            writer.u32(ProtocolIdentifier);
            writer.u16(ProtocolVersion);
            writer.u16(static_cast<std::uint16_t>(kind));
        }

        bool validMode(const std::string &value) {
            return value == "Deathmatch" || value == "Predator" || value == "Team deathmatch";
        }

        bool validLevelPlan(const std::string &value) {
            return value == "Fixed level" || value == "Shuffle all levels" || value == "Random level";
        }
    }

    std::vector<std::uint8_t> serializeSetup(const Setup &setup) {
        if (setup.localPlayerNames.empty() || setup.localPlayerNames.size() > Trust::MaxParticipants
            || !validMode(setup.mode) || !validLevelPlan(setup.levelPlan)
            || setup.roundLimit == 0 || setup.roundLimit > 99
            || (setup.mode == "Team deathmatch" && (setup.teamCount < 2 || setup.teamCount > 4)))
            throw std::invalid_argument("Invalid host composition setup");
        Writer writer; envelope(writer, Kind::Setup);
        writer.u8(static_cast<std::uint8_t>(setup.localPlayerNames.size()));
        for (const auto &name: setup.localPlayerNames) {
            if (name.empty() || !Trust::validGeneralString(name) || name.size() > MaximumDisplayNameBytes)
                throw std::invalid_argument("Invalid host player name");
            writer.text(name, MaximumDisplayNameBytes);
        }
        writer.text(setup.mode, 32);
        writer.u8(setup.teamCount);
        writer.boolean(setup.friendlyFire);
        writer.text(setup.levelPlan, 32);
        writer.text(setup.fixedLevel, 240);
        writer.u8(setup.roundLimit);
        writer.boolean(setup.assistance);
        writer.boolean(setup.quickLiquid);
        writer.boolean(setup.burnableTrees);
        return writer.take();
    }

    std::vector<std::uint8_t> serializeSetupUpdate(const Setup &setup) {
        auto message = serializeSetup(setup);
        message[6] = 0; message[7] = static_cast<std::uint8_t>(Kind::UpdateSetup);
        return message;
    }

    std::vector<std::uint8_t> serializeAction(Kind kind) {
        if (kind != Kind::StartMatch && kind != Kind::ReturnToLobby && kind != Kind::AdvanceRound
            && kind != Kind::ConfigurationChanged)
            throw std::invalid_argument("Invalid host composition action");
        Writer writer; envelope(writer, kind); return writer.take();
    }

    std::vector<std::uint8_t> serializePayload(Kind kind, const std::vector<std::uint8_t> &payload) {
        if ((kind != Kind::PlayerInput && kind != Kind::CanonicalSnapshot
             && kind != Kind::PlayerInputOutcome) || payload.empty())
            throw std::invalid_argument("Invalid host composition payload");
        Writer writer; envelope(writer, kind);
        auto result = writer.take();
        result.insert(result.end(), payload.begin(), payload.end());
        return result;
    }

    std::vector<std::uint8_t> serializeRosterMove(std::uint64_t playerId, std::int8_t direction) {
        if (playerId == 0 || (direction != -1 && direction != 1))
            throw std::invalid_argument("Invalid roster move");
        Writer writer; envelope(writer, Kind::RosterMove); writer.u64(playerId);
        writer.u8(static_cast<std::uint8_t>(direction)); return writer.take();
    }

    std::vector<std::uint8_t> serializeOwnedPersons(const std::vector<std::string> &names) {
        if (names.empty() || names.size() > Trust::MaxParticipants)
            throw std::invalid_argument("Invalid owned person configuration");
        Writer writer; envelope(writer, Kind::UpdateOwnedPersons);
        writer.u8(static_cast<std::uint8_t>(names.size()));
        for (const auto &name: names) {
            if (name.empty() || name.size() > MaximumDisplayNameBytes || !Trust::validGeneralString(name))
                throw std::invalid_argument("Invalid local player name");
            writer.text(name, MaximumDisplayNameBytes);
        }
        return writer.take();
    }

    std::optional<Message> deserialize(const std::vector<std::uint8_t> &payload) noexcept {
        try {
            Reader reader(payload);
            if (reader.u32() != ProtocolIdentifier || reader.u16() != ProtocolVersion) return std::nullopt;
            const auto rawKind = reader.u16();
            if (rawKind < static_cast<std::uint16_t>(Kind::Setup)
                || rawKind > static_cast<std::uint16_t>(Kind::UpdateOwnedPersons)) return std::nullopt;
            Message message; message.kind = static_cast<Kind>(rawKind);
            if (message.kind == Kind::Setup || message.kind == Kind::UpdateSetup) {
                Setup setup;
                const auto count = reader.u8();
                if (count == 0 || count > Trust::MaxParticipants) return std::nullopt;
                for (std::uint8_t index = 0; index < count; ++index) {
                    auto name = reader.text(MaximumDisplayNameBytes);
                    if (name.empty() || !Trust::validGeneralString(name)) return std::nullopt;
                    setup.localPlayerNames.push_back(std::move(name));
                }
                setup.mode = reader.text(32);
                setup.teamCount = reader.u8();
                setup.friendlyFire = reader.boolean();
                setup.levelPlan = reader.text(32);
                setup.fixedLevel = reader.text(240);
                setup.roundLimit = reader.u8();
                setup.assistance = reader.boolean();
                setup.quickLiquid = reader.boolean();
                setup.burnableTrees = reader.boolean();
                if (!reader.done() || !validMode(setup.mode) || !validLevelPlan(setup.levelPlan)
                    || setup.roundLimit == 0 || setup.roundLimit > 99
                    || (setup.mode == "Team deathmatch" && (setup.teamCount < 2 || setup.teamCount > 4)))
                    return std::nullopt;
                message.setup = std::move(setup);
            } else if (message.kind == Kind::StartMatch || message.kind == Kind::ReturnToLobby
                       || message.kind == Kind::AdvanceRound || message.kind == Kind::ConfigurationChanged) {
                if (!reader.done()) return std::nullopt;
            } else if (message.kind == Kind::RosterMove) {
                message.rosterPlayerId = reader.u64();
                const auto rawDirection = reader.u8();
                message.rosterDirection = rawDirection == 255 ? -1 : rawDirection == 1 ? 1 : 0;
                if (!reader.done() || message.rosterPlayerId == 0
                    || (message.rosterDirection != -1 && message.rosterDirection != 1)) return std::nullopt;
            } else if (message.kind == Kind::UpdateOwnedPersons) {
                const auto count = reader.u8();
                if (count == 0 || count > Trust::MaxParticipants) return std::nullopt;
                for (std::uint8_t index = 0; index < count; ++index) {
                    auto name = reader.text(MaximumDisplayNameBytes);
                    if (name.empty() || !Trust::validGeneralString(name)) return std::nullopt;
                    message.ownedPersonNames.push_back(std::move(name));
                }
                if (!reader.done()) return std::nullopt;
            } else {
                message.payload = reader.rest();
                if (message.payload.empty()) return std::nullopt;
            }
            return message;
        } catch (...) { return std::nullopt; }
    }
}
