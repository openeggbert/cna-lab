#pragma once

#include <cstdint>

namespace IronGang
{
    // plan_04 IG-04-003/004/005: the time the simulation runs on, which is not the wall clock.
    //
    // Two responsibilities, and only two:
    //
    //   1. **Clamp an extreme frame delta.** A stall -- a breakpoint, a district load, a laptop
    //      lid, a paging storm -- hands the next frame a delta of whole seconds. Feeding that to
    //      physics and movement teleports the player through walls, drives the sedan past its
    //      trigger, and skips a mission condition that was true only in between. Clamping makes
    //      the world advance *slower than wall time* instead, which is the failure everyone
    //      prefers: a visible hitch rather than a broken world.
    //   2. **Be monotonic.** Elapsed simulation time is the sum of the deltas the simulation
    //      actually ran, so it never jumps, never goes backwards, and is unaffected by the system
    //      clock being changed underneath the process.
    //
    // It deliberately does **not** subdivide a long delta into several fixed steps. CNA already
    // drives Update() on a fixed 60 Hz step (see IronGangGame::Initialize's
    // setTargetElapsedTimeProperty), so this clock's caller is already the fixed-step half of the
    // engine; subdividing here would step the world twice for one engine step. Drawing is the
    // variable-step half and reads state, never advances it -- that split is IG-04-005.
    //
    // What that means for the clamp today, stated plainly: while CNA runs fixed-step it hands
    // Update() a constant 16.67 ms and catches up by calling Update() repeatedly, so nothing here
    // ever clamps. The clamp is load-bearing the moment the game runs variable-step, where CNA's
    // own cap is `Game::MaxElapsedTime` -- 500 ms, five times more than this game's movement and
    // physics can absorb in one step -- and it is a guard against that until then.
    class SimulationClock final
    {
    public:
        // The longest single step the simulation will take: 100 ms, i.e. the world may run as slow
        // as 10 Hz but never in bigger jumps than that. Below this, a stall costs smoothness;
        // above it, a stall costs correctness.
        static constexpr float kDefaultMaximumStepSeconds = 0.100F;

        // maximumStepSeconds <= 0 is ignored, since a clock that never advances is not a
        // configuration anyone means to ask for.
        void Configure(float maximumStepSeconds) noexcept;
        [[nodiscard]] float GetMaximumStepSeconds() const noexcept { return maximumStepSeconds_; }

        // Feeds one frame's raw delta and returns what the simulation should use. A negative or
        // non-finite delta -- a broken or wrapped platform timer -- yields 0 rather than running
        // the world backwards.
        [[nodiscard]] float Advance(float rawDeltaSeconds) noexcept;

        // Monotonic simulation time: the sum of every delta actually taken.
        [[nodiscard]] double GetElapsedSeconds() const noexcept { return elapsedSeconds_; }
        // Wall time the simulation refused to take, summed. Non-zero means the game has stalled
        // and the world is behind; it is worth logging once rather than every frame.
        [[nodiscard]] double GetDroppedSeconds() const noexcept { return droppedSeconds_; }
        // How many deltas were clamped, and how many frames have been advanced at all.
        [[nodiscard]] std::uint64_t GetClampedStepCount() const noexcept { return clampedSteps_; }
        [[nodiscard]] std::uint64_t GetFrameCount() const noexcept { return frames_; }

        void Reset() noexcept;

    private:
        float maximumStepSeconds_{kDefaultMaximumStepSeconds};
        double elapsedSeconds_{0.0};
        double droppedSeconds_{0.0};
        std::uint64_t clampedSteps_{0};
        std::uint64_t frames_{0};
    };
}
