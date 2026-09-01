#include "IronGang/Audio/AudioListener.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    SpatialFalloff SpatialFalloffFor(SpatialPreset preset) noexcept
    {
        switch (preset)
        {
            // A voice carries across a room, not across a district: it has to fall away fast
            // enough that two conversations in the same street do not overlap.
            case SpatialPreset::Voice: return SpatialFalloff{6.0F, 35.0F};
            // A car is audible a long way off, and the player's own car is effectively at the
            // listener, so its reference distance has to cover the whole camera boom.
            case SpatialPreset::Vehicle: return SpatialFalloff{12.0F, 90.0F};
            case SpatialPreset::Effect: return SpatialFalloff{8.0F, 45.0F};
            // Ambience is a place, not a point: it should still be there when you cross the street.
            case SpatialPreset::Ambience: return SpatialFalloff{20.0F, 140.0F};
        }
        return SpatialFalloff{};
    }

    SpatialGain ComputeSpatialGain(const AudioListener& listener,
                                   const Vector3& emitter,
                                   const SpatialFalloff& falloff) noexcept
    {
        SpatialGain gain;

        const Vector3 offset = emitter - listener.position;
        const float distance = offset.Length();
        const float reference = std::max(0.0F, falloff.referenceMetres);
        const float maximum = std::max(reference, falloff.maximumMetres);

        if (distance <= reference)
        {
            gain.attenuation = 1.0F;
        }
        else if (distance >= maximum || maximum <= reference)
        {
            gain.attenuation = 0.0F;
        }
        else
        {
            // Linear rolloff between the two. Deliberately not inverse-square: this is a stylised
            // city, and a physically correct curve makes everything either deafening up close or
            // inaudible three metres away with the volumes a game actually mixes at.
            gain.attenuation = 1.0F - (distance - reference) / (maximum - reference);
        }

        // Pan in the XZ plane only. A source directly overhead has no left or right, and treating
        // the vertical component as lateral makes sounds swing across the stereo field whenever
        // the camera pitches.
        Vector3 flatForward(listener.forward.X, 0.0F, listener.forward.Z);
        const float forwardLength = flatForward.Length();
        Vector3 flatOffset(offset.X, 0.0F, offset.Z);
        const float flatDistance = flatOffset.Length();
        if (forwardLength > 1e-4F && flatDistance > 1e-4F)
        {
            flatForward = flatForward / forwardLength;
            flatOffset = flatOffset / flatDistance;
            const Vector3 right(-flatForward.Z, 0.0F, flatForward.X);
            gain.pan = std::clamp(Vector3::Dot(flatOffset, right), -1.0F, 1.0F);
        }
        return gain;
    }
}
