#include "PlayerInputProtocol.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

#include "NetworkTrustPolicy.h"

namespace Duel6::Network::Input {
    namespace {
        class Writer {
        public:
            template<typename T> void integer(T value) {
                using U = std::make_unsigned_t<T>;
                U bits = static_cast<U>(value);
                for (std::size_t index = 0; index < sizeof(T); ++index) {
                    bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu));
                    bits >>= 8u;
                }
                if (bytes.size() > Trust::MaxAdmissionPayloadBytes)
                    throw std::length_error("Player input payload is too large");
            }
            std::vector<std::uint8_t> finish() { return std::move(bytes); }
        private:
            std::vector<std::uint8_t> bytes;
        };

        class Reader {
        public:
            explicit Reader(const std::vector<std::uint8_t> &bytes) : bytes(bytes) {
                if (bytes.empty() || bytes.size() > Trust::MaxAdmissionPayloadBytes)
                    throw std::invalid_argument("Invalid player input payload size");
            }
            template<typename T> T integer() {
                if (sizeof(T) > bytes.size() - offset) throw std::invalid_argument("Truncated player input payload");
                using U = std::make_unsigned_t<T>;
                U value = 0;
                for (std::size_t index = 0; index < sizeof(T); ++index)
                    value |= static_cast<U>(bytes[offset++]) << (index * 8u);
                return static_cast<T>(value);
            }
            bool complete() const noexcept { return offset == bytes.size(); }
        private:
            const std::vector<std::uint8_t> &bytes;
            std::size_t offset = 0;
        };

        void header(Writer &writer, FrameKind kind) {
            writer.integer(PlayerInputProtocolIdentifier);
            writer.integer(PlayerInputProtocolVersion);
            writer.integer(static_cast<std::uint16_t>(kind));
        }
    }

    std::uint32_t PlayerActionState::mask() const noexcept {
        return (moveLeft ? MoveLeft : 0u) | (moveRight ? MoveRight : 0u) | (jump ? Jump : 0u)
               | (crouch ? Crouch : 0u) | (shoot ? Shoot : 0u)
               | (pickOrSwapWeapon ? PickOrSwapWeapon : 0u) | (showStatus ? ShowStatus : 0u);
    }

    bool isPlayerInputFrame(const std::vector<std::uint8_t> &payload) noexcept {
        if (payload.size() < sizeof(std::uint32_t)) return false;
        std::uint32_t identifier = 0;
        for (std::size_t index = 0; index < sizeof(identifier); ++index)
            identifier |= static_cast<std::uint32_t>(payload[index]) << (index * 8u);
        return identifier == PlayerInputProtocolIdentifier;
    }

    std::vector<std::uint8_t> serializeCommand(const Command &command) {
        if (command.participantId == 0 || command.playerId == 0 || command.sequence == 0
            || (command.actions & ~AllActions) != 0) throw std::invalid_argument("Invalid player input command");
        Writer writer;
        header(writer, FrameKind::Command);
        writer.integer(command.participantId);
        writer.integer(command.playerId);
        writer.integer(command.sequence);
        writer.integer(command.targetTick);
        writer.integer(command.actions);
        return writer.finish();
    }

    std::vector<std::uint8_t> serializeOutcome(const Outcome &outcome) {
        if ((outcome.category != OutcomeCategory::Invalid
             && (outcome.playerId == 0 || outcome.sequence == 0))
            || outcome.category > OutcomeCategory::OverLimit)
            throw std::invalid_argument("Invalid player input outcome");
        const FrameKind kind = outcome.category == OutcomeCategory::Applied
                               ? FrameKind::AppliedAcknowledgment : FrameKind::Outcome;
        Writer writer;
        header(writer, kind);
        writer.integer(static_cast<std::uint8_t>(outcome.category));
        writer.integer(outcome.playerId);
        writer.integer(outcome.sequence);
        writer.integer(outcome.effectiveTick);
        return writer.finish();
    }

    std::vector<std::uint8_t> serializeSessionPolicyViolation() {
        Writer writer;
        header(writer, FrameKind::SessionPolicyViolation);
        return writer.finish();
    }

    std::optional<Frame> deserializeFrame(const std::vector<std::uint8_t> &payload) noexcept {
        try {
            Reader reader(payload);
            if (reader.integer<std::uint32_t>() != PlayerInputProtocolIdentifier
                || reader.integer<std::uint16_t>() != PlayerInputProtocolVersion) return std::nullopt;
            Frame frame;
            frame.kind = static_cast<FrameKind>(reader.integer<std::uint16_t>());
            if (frame.kind == FrameKind::Command) {
                Command command;
                command.participantId = reader.integer<Identity>();
                command.playerId = reader.integer<Identity>();
                command.sequence = reader.integer<std::uint64_t>();
                command.targetTick = reader.integer<Tick>();
                command.actions = reader.integer<std::uint32_t>();
                frame.command = command;
            } else if (frame.kind == FrameKind::Outcome || frame.kind == FrameKind::AppliedAcknowledgment) {
                Outcome outcome;
                outcome.category = static_cast<OutcomeCategory>(reader.integer<std::uint8_t>());
                outcome.playerId = reader.integer<Identity>();
                outcome.sequence = reader.integer<std::uint64_t>();
                outcome.effectiveTick = reader.integer<Tick>();
                if (outcome.category > OutcomeCategory::OverLimit
                    || (outcome.category != OutcomeCategory::Invalid
                        && (outcome.playerId == 0 || outcome.sequence == 0))
                    || (frame.kind == FrameKind::AppliedAcknowledgment
                        && outcome.category != OutcomeCategory::Applied)) return std::nullopt;
                frame.outcome = outcome;
            } else if (frame.kind != FrameKind::SessionPolicyViolation) return std::nullopt;
            if (!reader.complete()) return std::nullopt;
            return frame;
        } catch (...) { return std::nullopt; }
    }

    OwnedPlayerCommandSource::OwnedPlayerCommandSource(
            Identity participantId, std::vector<Identity> ownedPlayerIds) : participantId(participantId) {
        if (participantId == 0 || ownedPlayerIds.empty() || ownedPlayerIds.size() > Trust::MaxParticipants)
            throw std::invalid_argument("Invalid owned player input mapping");
        for (Identity playerId: ownedPlayerIds)
            if (playerId == 0 || !sequences.emplace(playerId, 0).second)
                throw std::invalid_argument("Invalid owned player input mapping");
    }

    std::optional<Command> OwnedPlayerCommandSource::sample(
            Identity playerId, Tick targetTick, std::uint32_t actions) {
        auto found = sequences.find(playerId);
        if (found == sequences.end() || found->second == UINT64_MAX || (actions & ~AllActions) != 0)
            return std::nullopt;
        ++found->second;
        return Command{participantId, playerId, found->second, targetTick, actions};
    }

    std::optional<Command> OwnedPlayerCommandSource::sample(
            Identity playerId, Tick targetTick, const PlayerActionState &actions) {
        return sample(playerId, targetTick, actions.mask());
    }

    void OwnedPlayerCommandSource::reset() noexcept {
        for (auto &entry: sequences) entry.second = 0;
    }

    ClientCommandSession::ClientCommandSession(
            Identity participantId, std::vector<Identity> ownedPlayerIds, Sender sender)
            : source(participantId, std::move(ownedPlayerIds)), sender(std::move(sender)) {
        if (!this->sender) throw std::invalid_argument("Player input transport dispatch is required");
    }

    std::optional<Command> ClientCommandSession::submit(
            Identity playerId, Tick targetTick, std::uint32_t actions) {
        auto command = source.sample(playerId, targetTick, actions);
        if (!command) return std::nullopt;
        const auto key = std::make_pair(playerId, command->sequence);
        states[key] = ClientCommandState::Submitted;
        if (sender(serializeCommand(*command)) != SendResult::Accepted) {
            states.erase(key);
            return std::nullopt;
        }
        return command;
    }

    std::optional<Command> ClientCommandSession::submit(
            Identity playerId, Tick targetTick, const PlayerActionState &actions) {
        return submit(playerId, targetTick, actions.mask());
    }

    bool ClientCommandSession::receive(const std::vector<std::uint8_t> &payload) {
        const auto frame = deserializeFrame(payload);
        if (!frame) return false;
        if (frame->kind == FrameKind::SessionPolicyViolation) {
            endedForPolicyViolation = true;
            return true;
        }
        if (!frame->outcome) return false;
        const Outcome &outcome = *frame->outcome;
        const auto key = std::make_pair(outcome.playerId, outcome.sequence);
        auto found = states.find(key);
        if (found == states.end()) return false;
        switch (outcome.category) {
            case OutcomeCategory::Pending: found->second = ClientCommandState::Pending; break;
            case OutcomeCategory::Superseded: found->second = ClientCommandState::Superseded; break;
            case OutcomeCategory::Applied:
                if (found->second != ClientCommandState::Pending) return false;
                found->second = ClientCommandState::Applied;
                break;
            case OutcomeCategory::Unauthorized:
                return false;
            default: found->second = ClientCommandState::Rejected; break;
        }
        latestOutcome = outcome;
        return true;
    }

    std::optional<ClientCommandState> ClientCommandSession::state(
            Identity playerId, std::uint64_t sequence) const noexcept {
        const auto found = states.find({playerId, sequence});
        return found == states.end() ? std::nullopt : std::optional<ClientCommandState>(found->second);
    }

    std::optional<Outcome> ClientCommandSession::lastOutcome() const noexcept { return latestOutcome; }
    bool ClientCommandSession::policyViolation() const noexcept { return endedForPolicyViolation; }

    void ClientCommandSession::reset() noexcept {
        source.reset();
        states.clear();
        latestOutcome.reset();
        endedForPolicyViolation = false;
    }
}
