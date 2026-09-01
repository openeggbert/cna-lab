#include "IronGang/Cutscenes/CutscenePlayer.hpp"

#include <algorithm>

namespace IronGang
{
    void CutscenePlayer::Start(CutsceneSequence sequence)
    {
        sequence_ = std::move(sequence);
        elapsed_ = 0.0F;
        active_ = !sequence_.cameraKeyframes.empty();
    }

    void CutscenePlayer::Update(float deltaSeconds)
    {
        if (!active_)
        {
            return;
        }
        elapsed_ += deltaSeconds;
        if (elapsed_ >= sequence_.duration)
        {
            elapsed_ = sequence_.duration;
            active_ = false;
        }
    }

    void CutscenePlayer::Skip()
    {
        elapsed_ = sequence_.duration;
        active_ = false;
    }

    namespace
    {
        // Finds the two keyframes bracketing elapsed_ and linearly interpolates between the
        // requested field. Clamps to the first/last keyframe outside their time range.
        Vector3 InterpolateField(const std::vector<CutsceneCameraKeyframe>& keyframes, float elapsed,
                                 Vector3 CutsceneCameraKeyframe::*field)
        {
            if (keyframes.empty())
            {
                return Vector3{};
            }
            if (elapsed <= keyframes.front().time)
            {
                return keyframes.front().*field;
            }
            if (elapsed >= keyframes.back().time)
            {
                return keyframes.back().*field;
            }
            for (std::size_t i = 1; i < keyframes.size(); ++i)
            {
                if (elapsed <= keyframes[i].time)
                {
                    const CutsceneCameraKeyframe& a = keyframes[i - 1];
                    const CutsceneCameraKeyframe& b = keyframes[i];
                    const float span = b.time - a.time;
                    const float t = span > 0.0F ? (elapsed - a.time) / span : 0.0F;
                    return Vector3::Lerp(a.*field, b.*field, t);
                }
            }
            return keyframes.back().*field; // unreachable given the checks above
        }
    }

    Vector3 CutscenePlayer::GetCameraPosition() const
    {
        return InterpolateField(sequence_.cameraKeyframes, elapsed_, &CutsceneCameraKeyframe::position);
    }

    Vector3 CutscenePlayer::GetCameraLookAt() const
    {
        return InterpolateField(sequence_.cameraKeyframes, elapsed_, &CutsceneCameraKeyframe::lookAt);
    }

    const std::string& CutscenePlayer::GetActiveCueLineId() const noexcept
    {
        static const std::string none;
        // Cues are validated into ascending time order at load, so the last one that has come due
        // is the one showing. A linear scan over a handful of cues costs nothing and needs no
        // playback cursor that Skip() would then have to keep in sync.
        const std::string* active = &none;
        for (const CutsceneDialogueCue& cue : sequence_.dialogueCues)
        {
            if (cue.time > elapsed_)
            {
                break;
            }
            active = &cue.lineId;
        }
        return *active;
    }
}
