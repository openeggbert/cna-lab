#include "CopperBoots/ProceduralAudio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace CopperBoots
{
    namespace
    {
        [[nodiscard]] float Envelope(const int frame, const int frameCount)
        {
            constexpr float attackSeconds = 0.004F;
            const int attackFrames = static_cast<int>(
                ProceduralAudioSampleRate * attackSeconds);
            const float attack = std::min(1.0F,
                static_cast<float>(frame) / std::max(1, attackFrames));
            const float release = std::clamp(
                static_cast<float>(frameCount - 1 - frame) /
                    std::max(1.0F, frameCount * 0.35F),
                0.0F, 1.0F);
            return attack * release;
        }

        [[nodiscard]] float SquareWave(const float phase)
        {
            return std::sin(phase) >= 0.0F ? 1.0F : -1.0F;
        }

        [[nodiscard]] float TriangleWave(const float phase)
        {
            return 2.0F / std::numbers::pi_v<float> *
                   std::asin(std::sin(phase));
        }

        [[nodiscard]] float NoiseSample(const std::uint32_t state)
        {
            return static_cast<float>(state & 0xFFFFU) / 32'767.5F - 1.0F;
        }

        void AppendSample(std::vector<std::uint8_t>& pcm, const float sample)
        {
            const float clamped = std::clamp(sample, -1.0F, 1.0F);
            const auto value = static_cast<std::int16_t>(
                std::round(clamped * 23'000.0F));
            const auto bits = static_cast<std::uint16_t>(value);
            pcm.push_back(static_cast<std::uint8_t>(bits & 0xFFU));
            pcm.push_back(static_cast<std::uint8_t>(bits >> 8U));
        }

        [[nodiscard]] float CueDuration(const AudioCue cue)
        {
            switch (cue) {
            case AudioCue::Jump: return 0.11F;
            case AudioCue::Cog: return 0.10F;
            case AudioCue::Hit: return 0.14F;
            case AudioCue::EnemyDefeat: return 0.12F;
            case AudioCue::Projectile: return 0.08F;
            case AudioCue::Block: return 0.06F;
            case AudioCue::Complete: return 0.42F;
            case AudioCue::Ui: return 0.045F;
            case AudioCue::Count: return 0.0F;
            }
            return 0.0F;
        }
    }

    ProceduralSound GenerateProceduralSound(const AudioCue cue)
    {
        const int frameCount = std::max(1, static_cast<int>(std::round(
            CueDuration(cue) * ProceduralAudioSampleRate)));
        ProceduralSound result;
        result.Pcm.reserve(static_cast<std::size_t>(frameCount) * 2U);

        float phase = 0.0F;
        std::uint32_t noise = 0xC0FFEE11U ^
            static_cast<std::uint32_t>(cue);
        for (int frame = 0; frame < frameCount; ++frame) {
            const float progress = static_cast<float>(frame) /
                                   static_cast<float>(frameCount);
            float frequency = 440.0F;
            float wave = 0.0F;
            switch (cue) {
            case AudioCue::Jump:
                frequency = 280.0F + progress * 520.0F;
                wave = 0.65F * TriangleWave(phase) +
                       0.20F * SquareWave(phase * 0.5F);
                break;
            case AudioCue::Cog:
                frequency = progress < 0.5F ? 880.0F : 1'320.0F;
                wave = 0.75F * TriangleWave(phase);
                break;
            case AudioCue::Hit:
                noise ^= noise << 13U;
                noise ^= noise >> 17U;
                noise ^= noise << 5U;
                frequency = 105.0F;
                wave = NoiseSample(noise) * 0.65F +
                       SquareWave(phase) * 0.25F;
                break;
            case AudioCue::EnemyDefeat:
                frequency = 620.0F - progress * 430.0F;
                wave = 0.70F * SquareWave(phase) +
                       0.18F * TriangleWave(phase * 0.5F);
                break;
            case AudioCue::Projectile:
                frequency = 960.0F - progress * 520.0F;
                wave = 0.72F * TriangleWave(phase);
                break;
            case AudioCue::Block:
                noise ^= noise << 13U;
                noise ^= noise >> 17U;
                noise ^= noise << 5U;
                frequency = 145.0F;
                wave = 0.60F * SquareWave(phase) +
                       NoiseSample(noise) * 0.20F;
                break;
            case AudioCue::Complete: {
                constexpr float notes[]{523.25F, 659.25F, 783.99F, 1'046.50F};
                const int note = std::min(3, static_cast<int>(progress * 4.0F));
                frequency = notes[note];
                wave = 0.65F * TriangleWave(phase) +
                       0.15F * TriangleWave(phase * 2.0F);
                break;
            }
            case AudioCue::Ui:
                frequency = 1'100.0F;
                wave = 0.55F * SquareWave(phase);
                break;
            case AudioCue::Count:
                wave = 0.0F;
                break;
            }
            phase += 2.0F * std::numbers::pi_v<float> * frequency /
                     ProceduralAudioSampleRate;
            AppendSample(result.Pcm, wave * Envelope(frame, frameCount));
        }
        return result;
    }
}
