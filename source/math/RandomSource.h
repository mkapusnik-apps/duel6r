#ifndef DUEL6_MATH_RANDOMSOURCE_H
#define DUEL6_MATH_RANDOMSOURCE_H

#include <cstdint>
#include <string_view>

namespace Duel6 {
    class RandomSource {
    public:
        virtual ~RandomSource() = default;
        virtual std::uint64_t next(std::string_view purpose) = 0;
        virtual std::uint64_t bounded(std::uint64_t exclusiveUpperBound, std::string_view purpose) = 0;
    };
}

#endif
