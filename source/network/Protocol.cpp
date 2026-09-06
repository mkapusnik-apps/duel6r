#include "Protocol.h"

namespace Duel6::Network {
    bool hasAction(const InputCommand &command, InputAction action) {
        return (command.actions & static_cast<std::uint32_t>(action)) != 0;
    }
}
