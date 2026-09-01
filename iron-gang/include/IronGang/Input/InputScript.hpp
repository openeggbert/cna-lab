#pragma once

#include "IronGang/Input/InputBindings.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace IronGang
{
    inline constexpr std::size_t kGameActionCount = static_cast<std::size_t>(GameAction::Count);
    // What a single update's input looks like: which actions are held, by action rather than by
    // key, so a script survives someone rebinding a key (plan_28 IG-28-007's whole point).
    using HeldActions = std::array<bool, kGameActionCount>;

    // plan_30 IG-30-012: a recorded QA repro case. Sparse -- one step per update where the held
    // set *changes*, since almost every update in a real session is identical to the one before.
    struct InputScriptStep
    {
        int update{0};
        HeldActions held{};
    };

    inline constexpr int kInputScriptVersion = 1;
    // A bound on how long one repro case can be, in the same spirit as kMaxDialogueLines: a QA
    // script is a minute or two of deliberate input, not a session recording.
    inline constexpr std::size_t kMaxInputScriptSteps = 4096;

    // Deterministic playback of a recorded input sequence.
    //
    // Keyed on the **update index**, not the frame or the wall clock. The game's simulation runs at
    // a fixed 60 Hz step, so "at update 320, hold Confirm" means the same thing on a fast machine
    // and a slow one, while "at draw frame 40" does not -- how many draw frames a run produces
    // depends entirely on how fast it renders.
    class InputScript final
    {
    public:
        // Validation refuses an unsupported version, an empty or duplicate step list, steps out of
        // ascending update order, a negative update index, an unknown action id, and unknown
        // fields -- each of which is a repro case that would silently do the wrong thing.
        [[nodiscard]] bool LoadFromFile(const std::string& path, std::string& errorMessage);

        [[nodiscard]] const std::string& GetId() const noexcept { return id_; }
        [[nodiscard]] std::size_t GetStepCount() const noexcept { return steps_.size(); }
        [[nodiscard]] const std::vector<InputScriptStep>& GetSteps() const noexcept { return steps_; }
        // The update index of the last step; -1 for an empty script.
        [[nodiscard]] int GetLastUpdate() const noexcept;

        // Playback starts *before* update 0, so the first Advance() moves to it. That keeps
        // "pressed" meaningful on update 0: nothing was held before playback began.
        void Rewind() noexcept;
        void Advance() noexcept;
        [[nodiscard]] int GetUpdateIndex() const noexcept { return updateIndex_; }
        // True once playback has moved past the last step -- the repro case is over.
        [[nodiscard]] bool IsFinished() const noexcept;

        [[nodiscard]] bool IsDown(GameAction action) const noexcept;
        // Held on this update and not on the previous one, matching the game's own edge-triggered
        // WasPressed(). A step that holds an action for many updates presses it exactly once.
        [[nodiscard]] bool WasPressed(GameAction action) const noexcept;

    private:
        [[nodiscard]] const HeldActions& StateAt(int update) const noexcept;

        std::string id_;
        std::vector<InputScriptStep> steps_;
        int updateIndex_{-1};
    };

    // Records what actually happened, emitting a step only when the held set changes -- so an
    // idle minute costs one step, not 3600.
    class InputScriptRecorder final
    {
    public:
        explicit InputScriptRecorder(std::string id) : id_(std::move(id)) {}

        // Call once per simulation update, in order. Updates are numbered from 0 by the recorder
        // itself so a caller cannot desynchronise them from the script it produces.
        void Record(const HeldActions& held);

        [[nodiscard]] const std::vector<InputScriptStep>& GetSteps() const noexcept { return steps_; }
        [[nodiscard]] int GetUpdateCount() const noexcept { return updateCount_; }

        // Writes the JSON a QA repro case is stored as. An unrecorded (empty) script is refused
        // rather than written: a file that plays back nothing is worse than no file.
        [[nodiscard]] bool Save(const std::string& path, std::string& errorMessage) const;

    private:
        std::string id_;
        std::vector<InputScriptStep> steps_;
        int updateCount_{0};
        bool started_{false};
        HeldActions previous_{};
    };
}
