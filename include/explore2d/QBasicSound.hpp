#pragma once

#include "explore2d/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace explore2d {

// A zero frequency is a PLAY-style rest. Durations use the 18.2 Hz timer ticks
// accepted by QBasic SOUND rather than milliseconds.
struct ToneStep final {
    int frequencyHz{};
    int durationTicks{1};
};

struct ToneEffectDefinition final {
    std::string id;
    std::vector<ToneStep> steps;
    float volume{0.22F};
};

inline constexpr int qbasicSoundSampleRate = 22050;

[[nodiscard]] float toneEffectDurationSeconds(const ToneEffectDefinition& effect) noexcept;
[[nodiscard]] std::vector<std::int16_t> synthesizeToneEffect(
    const ToneEffectDefinition& effect,
    int sampleRate = qbasicSoundSampleRate);

} // namespace explore2d
