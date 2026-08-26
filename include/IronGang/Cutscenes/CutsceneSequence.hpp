#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // Gate M8 (plan_26-cutscenes-and-cinematic-sequencing.md IG-26-001/002/005): a hand-written
    // timeline (no in-house editor, matching the locked no-bespoke-editor-suite decision) with two
    // tracks -- camera, and since 2026-08-26 dialogue. Animation, audio, event, and fade tracks
    // remain separate future work under IG-26-002.
    struct CutsceneCameraKeyframe
    {
        float time{0.0F};
        Vector3 position{};
        Vector3 lookAt{};
    };

    // plan_26 IG-26-002: one spoken line, named by the **stable dialogue id** (plan_25
    // IG-25-001) rather than by its text -- so editing a line's wording never silently changes
    // what a cutscene says, and a line that is deleted is caught when the cutscene loads instead
    // of showing nothing when it plays.
    struct CutsceneDialogueCue
    {
        float time{0.0F};
        std::string lineId;
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
        // Sorted by ascending time; each entry's line must exist in the conversation the sequence
        // was validated against.
        std::vector<CutsceneDialogueCue> dialogueCues;
    };

    // Parses and validates a cutscene sequence from @p path: at least one keyframe, keyframes
    // sorted by strictly ascending time starting at 0, and duration >= the last keyframe's time.
    // Returns false with errorMessage set on any parse/validation failure -- callers should fall
    // back to a hardcoded default (see IronGangGame's own fallback) rather than run with a
    // partially-invalid sequence.
    // @p knownLineIds is every dialogue line the sequence may cue -- normally
    // DialogueSystem's own ids. A cue naming anything else is a **stale reference** and fails the
    // load, which is the whole reason dialogue has stable ids: the alternative is a cutscene that
    // plays in silence at the point where a line used to be, months after the line was renamed.
    // Pass an empty set to load a sequence with no dialogue track.
    [[nodiscard]] bool LoadCutsceneSequence(const std::string& path,
                                            const std::vector<std::string>& knownLineIds,
                                            CutsceneSequence& out,
                                            std::string& errorMessage);
}
