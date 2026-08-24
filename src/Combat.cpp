#include "Combat.hpp"

#include <algorithm>
#include <cstdint>

namespace WolfCna
{
    namespace
    {
        [[nodiscard]] std::uint64_t Mix(std::uint64_t value)
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27u)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31u);
        }
    }

    FirearmShot ResolveFirearmShot(
        PlayerWeapon weapon,
        int ammunition,
        std::uint32_t seed,
        std::uint32_t sequence,
        bool moving)
    {
        if (weapon == PlayerWeapon::Knife || ammunition <= 0)
            return {.ammunitionAfter = std::max(0, ammunition), .sequenceAfter = sequence};

        const WeaponSpec spec = GetWeaponSpec(weapon);
        const std::uint64_t input =
            (static_cast<std::uint64_t>(seed) << 32u) ^
            static_cast<std::uint64_t>(sequence) ^
            (static_cast<std::uint64_t>(static_cast<int>(weapon)) << 60u);
        const std::uint64_t randomBits = Mix(input);
        constexpr double Inverse53Bits = 1.0 / 9007199254740992.0;
        const double unit = static_cast<double>(randomBits >> 11u) * Inverse53Bits;
        const float signedUnit = static_cast<float>(unit * 2.0 - 1.0);
        const float maximumSpread = moving
            ? spec.movingSpreadRadians
            : spec.standingSpreadRadians;
        return {
            .emitted = true,
            .ammunitionAfter = ammunition - 1,
            .sequenceAfter = sequence + 1u,
            .yawOffsetRadians = signedUnit * maximumSpread};
    }
}
