#pragma once

#include "IronGang/Cutscenes/CutsceneSequence.hpp"

namespace IronGang
{
    // Gate M8: the minimal in-engine sequence player (IG-26-001) -- plays a CutsceneSequence's
    // camera track, supports skipping straight to the terminal (last-keyframe) state
    // (IG-26-004), and reports when it has finished so the caller can hand control back to
    // gameplay. Deliberately holds no player-control/freeze logic itself (IronGangGame decides
    // what "control" means and when to restore it, the same way it already does for dialogue).
    class CutscenePlayer final
    {
    public:
        // Begins playback of @p sequence from time 0. A sequence with no keyframes never
        // becomes active (nothing to play).
        void Start(CutsceneSequence sequence);

        // Advances playback by deltaSeconds. A no-op if not active. Finishes (IsActive() becomes
        // false) the instant elapsed time reaches the sequence's duration.
        void Update(float deltaSeconds);

        // Jumps straight to the terminal state (as if elapsed time had reached duration) and
        // stops playback immediately -- the caller should treat this exactly like a natural
        // finish (IG-26-004: skip must apply the same required terminal state as a full play-through).
        void Skip();

        [[nodiscard]] bool IsActive() const noexcept { return active_; }

        // Interpolated camera position/look-at target for the current elapsed time. Only
        // meaningful while IsActive() (or on the exact frame it just became false via a natural
        // finish/Skip() -- both leave elapsed_ at the sequence's terminal keyframe).
        [[nodiscard]] Vector3 GetCameraPosition() const;
        [[nodiscard]] Vector3 GetCameraLookAt() const;

        // plan_26 IG-26-010: the dialogue line the track has reached -- the last cue at or before
        // the current time -- or an empty string before the first cue. The player deliberately
        // only *names* the line; resolving it to a speaker and text is the caller's job, which is
        // what keeps CutscenePlayer independent of DialogueSystem. Skip() leaves this at the last
        // cue, so a skipped cutscene ends on the same line a full play-through would (IG-26-004).
        [[nodiscard]] const std::string& GetActiveCueLineId() const noexcept;

    private:
        CutsceneSequence sequence_;
        float elapsed_{0.0F};
        bool active_{false};
    };
}
