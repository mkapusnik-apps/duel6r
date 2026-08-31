#ifndef DUEL6_MATH_RANDOMSOURCE_H
#define DUEL6_MATH_RANDOMSOURCE_H

#include <cstdint>

namespace Duel6 {
    class RandomSource {
    public:
        virtual ~RandomSource() = default;
        virtual std::uint64_t next() = 0;
        virtual std::uint64_t bounded(std::uint64_t exclusiveUpperBound) = 0;
    };
}

#endif
