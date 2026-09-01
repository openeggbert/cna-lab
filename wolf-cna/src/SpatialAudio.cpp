#include "SpatialAudio.hpp"

#include <algorithm>
#include <cmath>

namespace WolfCna
{
    SpatialAudioMix CalculateSpatialAudioMix(
        float listenerX,
        float listenerZ,
        float forwardX,
        float forwardZ,
        float sourceX,
        float sourceZ,
        float baseVolume,
        float nearDistance,
        float maximumDistance)
    {
        const float offsetX = sourceX - listenerX;
        const float offsetZ = sourceZ - listenerZ;
        const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        const float boundedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
        const float boundedNearDistance = std::max(0.0f, nearDistance);
        const float boundedMaximumDistance = std::max(
            boundedNearDistance + 0.001f,
            maximumDistance);

        float attenuation = 1.0f;
        if (distance > boundedNearDistance)
        {
            attenuation = 1.0f -
                (distance - boundedNearDistance) /
                (boundedMaximumDistance - boundedNearDistance);
        }
        attenuation = std::clamp(attenuation, 0.0f, 1.0f);

        float pan = 0.0f;
        if (distance > 0.0001f)
        {
            const float forwardLength = std::sqrt(
                forwardX * forwardX + forwardZ * forwardZ);
            if (forwardLength > 0.0001f)
            {
                const float normalizedForwardX = forwardX / forwardLength;
                const float normalizedForwardZ = forwardZ / forwardLength;
                const float rightX = -normalizedForwardZ;
                const float rightZ = normalizedForwardX;
                pan = (offsetX * rightX + offsetZ * rightZ) / distance;
            }
        }

        return {
            boundedBaseVolume * attenuation,
            std::clamp(pan, -1.0f, 1.0f)};
    }
}
