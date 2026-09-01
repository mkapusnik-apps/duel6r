#ifndef DUEL6_SERVER_DETERMINISTICRANDOM_H
#define DUEL6_SERVER_DETERMINISTICRANDOM_H

#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>
#include <string>
#include <string_view>

#include "../math/RandomSource.h"

namespace Duel6::Server::Authoritative {
    class DeterministicRandom : public RandomSource {
    public:
        struct Decision {
            std::uint64_t index = 0;
            std::uint64_t bound = 0;
            std::uint64_t value = 0;
            std::string purpose;
        };

        explicit DeterministicRandom(std::uint64_t seed);

        std::uint64_t next(std::string_view purpose) override;
        std::uint64_t bounded(std::uint64_t exclusiveUpperBound, std::string_view purpose) override;
        std::uint64_t decisionCount() const noexcept;
        std::uint64_t decisionDigest() const noexcept;
        const std::vector<Decision> &decisionTrace() const noexcept;

        template<typename Value>
        void shuffle(std::vector<Value> &values, std::string_view purpose) {
            for (std::size_t remaining = values.size(); remaining > 1; --remaining) {
                const std::size_t selected = static_cast<std::size_t>(bounded(remaining, purpose));
                Value temporary = std::move(values[remaining - 1]);
                values[remaining - 1] = std::move(values[selected]);
                values[selected] = std::move(temporary);
            }
        }

    private:
        std::uint64_t state;
        std::uint64_t decisions = 0;
        std::uint64_t digest = UINT64_C(14695981039346656037);
        std::vector<Decision> trace;

        std::uint64_t draw();
        void record(std::string_view purpose, std::uint64_t bound, std::uint64_t value);
    };
}

#endif
