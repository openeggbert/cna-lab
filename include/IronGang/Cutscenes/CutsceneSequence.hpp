#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // Gate M8 (plan_26-cutscenes-and-cinematic-sequencing.md IG-26-001/002/005): the smallest
    // possible sequence track -- a camera-only timeline, hand-written as data (no in-house
    // timeline editor, matching the project's locked no-bespoke-editor-suite decision). Other
    // track types (animation/dialogue/audio/event, IG-26-002) are real, separate future work;
    // this prototype reuses the existing DialogueSystem independently rather than folding
    // dialogue into the sequence itself.
    struct CutsceneCameraKeyframe
    {
        float time{0.0F};
        Vector3 position{};
        Vector3 lookAt{};
    };

    // A named, versioned, skippable camera sequence. Keyframes must be sorted by ascending time,
    // the first at time 0, and duration must be >= the last keyframe's time -- see
    // LoadCutsceneSequence for the validation that enforces this.
    struct CutsceneSequence
    {
        std::string id;
        int version{1};
        float duration{0.0F};
        std::vector<CutsceneCameraKeyframe> cameraKeyframes;
    };

    // Parses and validates a cutscene sequence from @p path: at least one keyframe, keyframes
    // sorted by strictly ascending time starting at 0, and duration >= the last keyframe's time.
    // Returns false with errorMessage set on any parse/validation failure -- callers should fall
    // back to a hardcoded default (see IronGangGame's own fallback) rather than run with a
    // partially-invalid sequence.
    [[nodiscard]] bool LoadCutsceneSequence(const std::string& path,
                                            CutsceneSequence& out,
                                            std::string& errorMessage);
}
