#include "explore2d/QBasicSound.hpp"

#include <algorithm>
#include <cmath>

namespace explore2d {

float toneEffectDurationSeconds(const ToneEffectDefinition& effect) noexcept {
    int ticks = 0;
    for (const ToneStep& step : effect.steps) ticks += std::max(0, step.durationTicks);
    return static_cast<float>(ticks) / qbasicTimerTicksPerSecond;
}

std::vector<std::int16_t> synthesizeToneEffect(const ToneEffectDefinition& effect, const int sampleRate) {
    if (sampleRate <= 0) return {};
    const float volume = std::clamp(effect.volume, 0.0F, 1.0F);
    const auto amplitude = static_cast<std::int16_t>(std::lround(volume * 32767.0F));
    std::vector<std::int16_t> samples;
    samples.reserve(static_cast<std::size_t>(std::ceil(
        toneEffectDurationSeconds(effect) * static_cast<float>(sampleRate))));

    for (const ToneStep& step : effect.steps) {
        const int count = std::max(0, static_cast<int>(std::lround(
            static_cast<float>(step.durationTicks) * static_cast<float>(sampleRate) /
                qbasicTimerTicksPerSecond)));
        if (step.frequencyHz <= 0) {
            samples.insert(samples.end(), static_cast<std::size_t>(count), 0);
            continue;
        }
        double phase = 0.0;
        const double phaseStep = static_cast<double>(step.frequencyHz) / static_cast<double>(sampleRate);
        for (int index = 0; index < count; ++index) {
            samples.push_back(phase < 0.5 ? amplitude : static_cast<std::int16_t>(-amplitude));
            phase += phaseStep;
            phase -= std::floor(phase);
        }
    }
    return samples;
}

} // namespace explore2d
