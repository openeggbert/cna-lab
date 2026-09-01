#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CopperBoots
{
    enum class AudioCue : std::size_t
    {
        Jump,
        Cog,
        Hit,
        EnemyDefeat,
        Projectile,
        Block,
        Complete,
        Ui,
        Count,
    };

    inline constexpr int ProceduralAudioSampleRate = 22'050;
    inline constexpr std::size_t AudioCueCount =
        static_cast<std::size_t>(AudioCue::Count);

    struct ProceduralSound
    {
        int SampleRate = ProceduralAudioSampleRate;
        std::vector<std::uint8_t> Pcm;
    };

    [[nodiscard]] ProceduralSound GenerateProceduralSound(AudioCue cue);
}
