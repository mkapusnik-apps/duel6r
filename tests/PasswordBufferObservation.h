#ifndef DUEL6_TEST_PASSWORD_BUFFER_OBSERVATION_H
#define DUEL6_TEST_PASSWORD_BUFFER_OBSERVATION_H

#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string_view>
#include "source/network/SecureSession.h"

// One explicit test-owned allocation, never a heap scan or an owning reference.
// The production destructor runs first; observation runs before real delete.
// No password bytes, addresses or post-free storage are emitted or inspected.
namespace PasswordBufferObservation {
    inline std::atomic<void *> allocation{nullptr};
    inline const unsigned char *knownBuffer = nullptr;
    inline std::atomic<bool> released{false}, erased{false};
    inline std::shared_ptr<const Duel6::Network::SessionPassword> watch(std::string_view value) {
        auto password = std::shared_ptr<const Duel6::Network::SessionPassword>(new Duel6::Network::SessionPassword(value));
        if (!password->required() || allocation.load()) throw std::runtime_error("password observer precondition");
        knownBuffer = reinterpret_cast<const unsigned char *>(password->value());
        released = erased = false;
        allocation = const_cast<Duel6::Network::SessionPassword *>(password.get());
        return password;
    }
    inline void beforeDelete(void *pointer) noexcept {
        void *match = pointer;
        if (pointer && allocation.compare_exchange_strong(match, nullptr)) {
            // SessionPassword's declared fixed private buffer, still allocated.
            erased = std::all_of(knownBuffer, knownBuffer + 129, [](unsigned char byte) { return byte == 0; });
            knownBuffer = nullptr;
            released = true;
        }
    }
}
void realPasswordDelete(void *) noexcept __asm__("__real__ZdlPv");
void wrapPasswordDelete(void *) noexcept __asm__("__wrap__ZdlPv");
void wrapPasswordDelete(void *pointer) noexcept {
    PasswordBufferObservation::beforeDelete(pointer); realPasswordDelete(pointer);
}
void realPasswordSizedDelete(void *, std::size_t) noexcept __asm__("__real__ZdlPvm");
void wrapPasswordSizedDelete(void *, std::size_t) noexcept __asm__("__wrap__ZdlPvm");
void wrapPasswordSizedDelete(void *pointer, std::size_t size) noexcept {
    PasswordBufferObservation::beforeDelete(pointer); realPasswordSizedDelete(pointer, size);
}
#endif
