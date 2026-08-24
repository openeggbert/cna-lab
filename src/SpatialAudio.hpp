#pragma once

namespace WolfCna
{
    struct SpatialAudioMix final
    {
        float volume = 0.0f;
        float pan = 0.0f;
    };

    [[nodiscard]] SpatialAudioMix CalculateSpatialAudioMix(
        float listenerX,
        float listenerZ,
        float forwardX,
        float forwardZ,
        float sourceX,
        float sourceZ,
        float baseVolume,
        float nearDistance = 1.0f,
        float maximumDistance = 14.0f);
}
