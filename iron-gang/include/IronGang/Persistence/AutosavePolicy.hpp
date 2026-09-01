#pragma once

namespace IronGang
{
    // plan_29 IG-29-010/011: why saving must not happen right now. Each of these is a moment where
    // the game holds state the save format does not carry, so a save taken here would come back
    // wrong: mid-cutscene the camera is not the gameplay camera, mid-dialogue the line index is
    // lost, mid-district-transition the world being written is the one being unloaded, and
    // mid-enter/exit-vehicle the player is neither on foot nor driving.
    enum class SaveBlockReason
    {
        None,
        Cutscene,
        Dialogue,
        DistrictTransition,
        VehicleTransition,
    };

    // Short player-facing text: "a cutscene is playing", etc.
    [[nodiscard]] const char* DescribeSaveBlockReason(SaveBlockReason reason) noexcept;

    // Which of these applies to the game right now is answered by
    // SaveBlockReasonForContext (Gameplay/InputContext.hpp): the input context already knows
    // whether a cutscene, a conversation, a district load, or a vehicle clip is in progress, and
    // two places deciding that independently is how they come to disagree.

    // What asked for an autosave. Ordered by priority: when several are pending at once the
    // highest one is what gets reported, since the file written is identical either way.
    enum class AutosaveTrigger
    {
        None,
        Interval,
        DistrictArrival,
        Checkpoint,
    };

    [[nodiscard]] const char* DescribeAutosaveTrigger(AutosaveTrigger trigger) noexcept;

    // Decides *when* an autosave happens; the caller decides what to write and where.
    //
    // Two things it guarantees. A request made at an unsafe moment is **held, not dropped** -- an
    // autosave asked for during a cutscene happens the instant the cutscene ends, so the player
    // does not silently lose a checkpoint to bad timing. And two triggers landing close together
    // produce one save, not two: a minimum spacing keeps a checkpoint reached moments after a
    // periodic autosave from writing the same state again.
    class AutosaveScheduler final
    {
    public:
        // Long enough not to interrupt play, short enough that losing the interval hurts less than
        // replaying it. Event triggers (checkpoints, district arrivals) are what actually matter;
        // the interval is the backstop between them.
        static constexpr float kDefaultIntervalSeconds = 180.0F;
        static constexpr float kDefaultMinimumSpacingSeconds = 20.0F;

        // intervalSeconds <= 0 disables periodic autosaves; event triggers still fire.
        void Configure(float intervalSeconds, float minimumSpacingSeconds = kDefaultMinimumSpacingSeconds);
        // Forgets the pending request and starts the interval afresh -- what a load or a reset
        // wants, since the state that was worth saving is gone.
        void Reset();
        // Asks for an autosave. Safe to call every frame for a condition that stays true; the
        // request is idempotent until it is served.
        void Request(AutosaveTrigger trigger);

        // Advances time and returns the trigger to save for this frame, or None. Returning a
        // trigger consumes the request and restarts both timers, so the caller must actually
        // perform the save (or accept that this one is lost).
        [[nodiscard]] AutosaveTrigger Update(float deltaSeconds, SaveBlockReason blockReason);

        [[nodiscard]] AutosaveTrigger GetPendingTrigger() const noexcept { return pending_; }
        [[nodiscard]] float GetSecondsSinceLastSave() const noexcept { return secondsSinceSave_; }
        [[nodiscard]] float GetIntervalSeconds() const noexcept { return intervalSeconds_; }

    private:
        float intervalSeconds_{kDefaultIntervalSeconds};
        float minimumSpacingSeconds_{kDefaultMinimumSpacingSeconds};
        float secondsSinceSave_{0.0F};
        AutosaveTrigger pending_{AutosaveTrigger::None};
    };
}
