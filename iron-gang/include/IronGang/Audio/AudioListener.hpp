#pragma once

#include "IronGang/Core/WorldTypes.hpp"

namespace IronGang
{
    // plan_27 IG-27-003/007: where the game is listening from, and how a sound at a point in the
    // world reaches it.
    //
    // Every sound so far played at full volume dead centre no matter where its source was: the
    // horn was as loud from across the district as from inside the car. Volume and pan are the two
    // things CNA's `SoundEffect::Play(volume, pitch, pan)` actually takes, so those are the two
    // things this computes -- not a full HRTF, and not a doppler model.
    //
    // The listener is the **active camera**, which is the same thing XNA's AudioListener means and
    // the right answer for a third-person game: the player hears what the shot shows. That
    // includes the camera being pulled in to a wall (plan_16 IG-16-003) -- the listener follows the
    // camera it is attached to, rather than silently diverging from it.
    struct AudioListener
    {
        Vector3 position{};
        // Unit vector the listener faces. Its right vector is derived, so a caller only has to
        // supply what it already has.
        Vector3 forward{0.0F, 0.0F, -1.0F};
    };

    // What a point source's distance and direction do to it.
    struct SpatialGain
    {
        // 0 (inaudible) to 1 (as loud as it was asked to be), multiplied into the bus volume.
        float attenuation{1.0F};
        // -1 fully left, 0 centre, +1 fully right.
        float pan{0.0F};
    };

    // plan_27 IG-27-007: rather than a falloff pair per call site, a small set of named presets.
    // Sounds of the same kind should carry the same distance, and the moment those numbers are
    // written at the call site they stop agreeing with each other.
    enum class SpatialPreset
    {
        Voice,
        Vehicle,
        Effect,
        Ambience,
    };

    struct SpatialFalloff
    {
        // Full volume within this distance -- a source is not quieter for being two metres away
        // instead of one.
        float referenceMetres{8.0F};
        // Silent at and beyond this distance.
        float maximumMetres{45.0F};
    };

    [[nodiscard]] SpatialFalloff SpatialFalloffFor(SpatialPreset preset) noexcept;

    // Distance is measured in 3D, but panning is computed in the XZ plane: a sound directly
    // overhead has no left or right, and pretending otherwise makes it swing across the stereo
    // field as the camera pitches.
    [[nodiscard]] SpatialGain ComputeSpatialGain(const AudioListener& listener,
                                                 const Vector3& emitter,
                                                 const SpatialFalloff& falloff) noexcept;

    [[nodiscard]] inline SpatialGain ComputeSpatialGain(const AudioListener& listener,
                                                        const Vector3& emitter,
                                                        SpatialPreset preset) noexcept
    {
        return ComputeSpatialGain(listener, emitter, SpatialFalloffFor(preset));
    }
}
