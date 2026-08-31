#ifndef DUEL6_SERVER_DETERMINISTICRANDOM_H
#define DUEL6_SERVER_DETERMINISTICRANDOM_H

#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

#include "../math/RandomSource.h"

namespace Duel6::Server::Authoritative {
    class DeterministicRandom : public RandomSource {
    public:
        explicit DeterministicRandom(std::uint64_t seed);

        std::uint64_t next() override;
        std::uint64_t bounded(std::uint64_t exclusiveUpperBound) override;

        template<typename Value>
        void shuffle(std::vector<Value> &values) {
            for (std::size_t remaining = values.size(); remaining > 1; --remaining) {
                const std::size_t selected = static_cast<std::size_t>(bounded(remaining));
                Value temporary = std::move(values[remaining - 1]);
                values[remaining - 1] = std::move(values[selected]);
                values[selected] = std::move(temporary);
            }
        }

    private:
        std::uint64_t state;
    };
}

#endif
