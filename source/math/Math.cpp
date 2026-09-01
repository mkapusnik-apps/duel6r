
/*
* Copyright (c) 2006, Ondrej Danek (www.ondrej-danek.net)
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Ondrej Danek nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../File.h"
#include "Math.h"

namespace Duel6 {
    namespace {
        class LocalRandomSource final : public RandomSource {
        public:
            std::uint64_t next(std::string_view) override {
                std::uniform_int_distribution<std::uint64_t> distribution;
                return distribution(Math::randomEngine);
            }

            std::uint64_t bounded(std::uint64_t exclusiveUpperBound, std::string_view) override {
                if (exclusiveUpperBound == 0) throw std::logic_error("Random bound is invalid");
                std::uniform_int_distribution<std::uint64_t> distribution(0, exclusiveUpperBound - 1);
                return distribution(Math::randomEngine);
            }
        };
    }

    std::random_device Math::randomDevice;
    std::default_random_engine Math::randomEngine;
    bool Math::randomEngineSeeded = false;
    thread_local RandomSource *Math::authoritativeRandom = nullptr;
    thread_local bool Math::authoritativeRequired = false;
    thread_local std::uint64_t Math::forbiddenRandomAccessCount = 0;
    const Float64 Math::Pi = 3.14159265358979323846;

    void Math::ensureRandomEngineSeeded() {
        if (!randomEngineSeeded) {
            randomEngine.seed(randomDevice());
            randomEngineSeeded = true;
        }
    }

    Math::RandomScope::RandomScope(RandomSource &source)
            : previous(authoritativeRandom), previousRequired(authoritativeRequired),
              previousRounding(std::fegetround()) {
        authoritativeRequired = true;
        authoritativeRandom = &source;
        if (std::fesetround(FE_TONEAREST) != 0) {
            authoritativeRandom = previous;
            authoritativeRequired = previousRequired;
            throw std::logic_error("Authoritative floating-point mode is unavailable");
        }
    }

    Math::RandomScope::~RandomScope() {
        authoritativeRandom = previous;
        authoritativeRequired = previousRequired;
        if (previousRounding != -1) std::fesetround(previousRounding);
    }

    Int32 Math::random(Int32 max, std::string_view purpose) {
        return random(0, max - 1, purpose);
    }

    Int32 Math::random(Int32 max, RandomSource &source, std::string_view purpose) {
        return random(0, max - 1, source, purpose);
    }

    Int32 Math::random(Int32 min, Int32 max, RandomSource &source, std::string_view purpose) {
        if (purpose.empty()) throw std::logic_error("Random purpose is required");
        if (max < min) throw std::logic_error("Random range is invalid");
        const std::uint64_t span = static_cast<std::uint64_t>(static_cast<Int64>(max) - min) + 1u;
        return static_cast<Int32>(static_cast<Int64>(min)
                                  + static_cast<Int64>(source.bounded(span, purpose)));
    }

    Int32 Math::random(Int32 min, Int32 max, std::string_view purpose) {
        if (authoritativeRandom) {
            if (purpose.empty()) throw std::logic_error("Authoritative random purpose is required");
            const std::uint64_t span = static_cast<std::uint64_t>(static_cast<Int64>(max) - min) + 1u;
            return static_cast<Int32>(static_cast<Int64>(min)
                                      + static_cast<Int64>(authoritativeRandom->bounded(span, purpose)));
        }
        if (authoritativeRequired) {
            ++forbiddenRandomAccessCount;
            throw std::logic_error("Authoritative random source is unavailable");
        }
        ensureRandomEngineSeeded();
        std::uniform_int_distribution<> uniformDistribution(min, max);
        return uniformDistribution(randomEngine);
    }

    Float32 Math::random(Float32 min, Float32 max, std::string_view purpose) {
        if (authoritativeRandom) {
            if (purpose.empty()) throw std::logic_error("Authoritative random purpose is required");
            const Float32 unit = static_cast<Float32>(authoritativeRandom->next(purpose) >> 40u)
                                 / static_cast<Float32>(UINT32_C(1) << 24u);
            return min + (max - min) * unit;
        }
        if (authoritativeRequired) {
            ++forbiddenRandomAccessCount;
            throw std::logic_error("Authoritative random source is unavailable");
        }
        ensureRandomEngineSeeded();
        std::uniform_real_distribution<Float32> uniformDistribution(min, max);
        return uniformDistribution(randomEngine);
    }

    Float32 Math::random(Float32 min, Float32 max, RandomSource &source, std::string_view purpose) {
        if (purpose.empty()) throw std::logic_error("Random purpose is required");
        const Float32 unit = static_cast<Float32>(source.next(purpose) >> 40u)
                             / static_cast<Float32>(UINT32_C(1) << 24u);
        return min + (max - min) * unit;
    }

    Float64 Math::random(Float64 min, Float64 max, std::string_view purpose) {
        if (authoritativeRandom) {
            if (purpose.empty()) throw std::logic_error("Authoritative random purpose is required");
            const Float64 unit = static_cast<Float64>(authoritativeRandom->next(purpose) >> 11u)
                                 / static_cast<Float64>(UINT64_C(1) << 53u);
            return min + (max - min) * unit;
        }
        if (authoritativeRequired) {
            ++forbiddenRandomAccessCount;
            throw std::logic_error("Authoritative random source is unavailable");
        }
        ensureRandomEngineSeeded();
        std::uniform_real_distribution<Float64> uniformDistribution(min, max);
        return uniformDistribution(randomEngine);
    }

    Float64 Math::random(Float64 min, Float64 max, RandomSource &source, std::string_view purpose) {
        if (purpose.empty()) throw std::logic_error("Random purpose is required");
        const Float64 unit = static_cast<Float64>(source.next(purpose) >> 11u)
                             / static_cast<Float64>(UINT64_C(1) << 53u);
        return min + (max - min) * unit;
    }

    RandomSource &Math::localRandomSource() {
        ensureRandomEngineSeeded();
        static LocalRandomSource source;
        return source;
    }

    bool Math::isAuthoritative() noexcept {
        return authoritativeRandom != nullptr;
    }

    Float32 Math::quantizeAuthoritative(Float32 value) {
        if (!authoritativeRandom || !std::isfinite(value)) return value;
        constexpr Float32 scale = 65536.0f;
        return std::round(value * scale) / scale;
    }

    std::uint64_t Math::forbiddenAuthoritativeRandomAccesses() noexcept {
        return forbiddenRandomAccessCount;
    }
}
