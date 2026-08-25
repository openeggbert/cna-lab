#include "IronGang/Core/RandomSource.hpp"

namespace IronGang
{
    namespace
    {
        // splitmix64 (Steele, Lea, Flood 2014), the finalizer used by the SplitMix/xoshiro family.
        std::uint64_t SplitMix64(std::uint64_t& state) noexcept
        {
            state += 0x9E3779B97F4A7C15ULL;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
    }

    RandomSource RandomSource::Derive(std::uint64_t label) const noexcept
    {
        // Mix the label into a copy of the state without advancing this generator, so deriving a
        // stream never changes what the parent produces next.
        std::uint64_t mixed = state_ ^ (label + 0x9E3779B97F4A7C15ULL + (state_ << 6) + (state_ >> 2));
        return RandomSource(SplitMix64(mixed));
    }

    std::uint64_t RandomSource::NextUInt64() noexcept
    {
        return SplitMix64(state_);
    }

    std::uint32_t RandomSource::NextIndex(std::uint32_t bound) noexcept
    {
        if (bound == 0)
        {
            return 0;
        }
        // Rejection sampling: taking the modulo directly would favour the first
        // (2^32 % bound) values, which is exactly the bias that shows up as "the same pedestrian
        // path keeps being picked".
        const std::uint32_t limit = std::uint32_t{0xFFFFFFFFU} - (std::uint32_t{0xFFFFFFFFU} % bound);
        std::uint32_t draw = 0;
        do
        {
            draw = static_cast<std::uint32_t>(NextUInt64() >> 32);
        } while (draw >= limit);
        return draw % bound;
    }

    float RandomSource::NextUnitFloat() noexcept
    {
        // 24 bits: every result is exactly representable as a float, so the distribution has no
        // gaps or repeated values near the top of the range.
        const std::uint32_t bits = static_cast<std::uint32_t>(NextUInt64() >> 40);
        return static_cast<float>(bits) * (1.0F / 16777216.0F);
    }

    float RandomSource::NextFloatInRange(float minimum, float maximum) noexcept
    {
        if (!(maximum > minimum))
        {
            return minimum;
        }
        return minimum + (maximum - minimum) * NextUnitFloat();
    }

    bool RandomSource::NextBool() noexcept
    {
        return (NextUInt64() >> 63) != 0;
    }
}
