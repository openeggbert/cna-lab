#pragma once

#include <cstdint>

namespace IronGang
{
    // plan_04 IG-04-011: the game's one source of gameplay randomness.
    //
    // Deterministic by construction: the same seed produces the same sequence, on every platform
    // and every compiler. That is why this is a hand-written generator rather than `<random>` --
    // `std::mt19937` is specified exactly, but `std::uniform_int_distribution` and
    // `std::uniform_real_distribution` are **not**, so the same seed gives different numbers on
    // different standard libraries. A save, a replay, or a bug report that says "seed 42" has to
    // mean the same thing everywhere, so the range mapping is written out here.
    //
    // This is splitmix64: small, fast, well-distributed, and trivially reproducible. It is not a
    // cryptographic generator and must not be used as one.
    //
    // There is deliberately no multi-stream framework. When two systems must not disturb each
    // other's sequence, give each its own RandomSource with its own seed -- that is what
    // Derive() is for.
    class RandomSource final
    {
    public:
        static constexpr std::uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ULL;

        RandomSource() noexcept = default;
        explicit RandomSource(std::uint64_t seed) noexcept : state_(seed) {}

        void Seed(std::uint64_t seed) noexcept { state_ = seed; }
        [[nodiscard]] std::uint64_t GetState() const noexcept { return state_; }

        // A fresh generator whose sequence is independent of this one's, without consuming from
        // it: mixing the seed with a caller-chosen label keeps "the traffic stream" and "the
        // pedestrian stream" from shifting each other when one of them changes.
        [[nodiscard]] RandomSource Derive(std::uint64_t label) const noexcept;

        [[nodiscard]] std::uint64_t NextUInt64() noexcept;
        // Uniform in [0, bound); returns 0 for bound == 0. Rejection-sampled, so the result is
        // genuinely uniform rather than modulo-biased toward small values.
        [[nodiscard]] std::uint32_t NextIndex(std::uint32_t bound) noexcept;
        // Uniform in [0, 1) with 24 bits of mantissa -- every value exactly representable.
        [[nodiscard]] float NextUnitFloat() noexcept;
        // Uniform in [minimum, maximum]; returns minimum when the range is empty or reversed.
        [[nodiscard]] float NextFloatInRange(float minimum, float maximum) noexcept;
        [[nodiscard]] bool NextBool() noexcept;

    private:
        std::uint64_t state_{kDefaultSeed};
    };
}
