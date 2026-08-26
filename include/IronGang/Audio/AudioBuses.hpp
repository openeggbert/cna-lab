#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace IronGang
{
    // plan_27 IG-27-001/026: the buses every sound in the game plays through.
    //
    // Until now there was one number -- `masterVolume` -- multiplied into every call. That cannot
    // express "quieten the engine while someone is talking", or "mute the music but not the
    // sirens", or any per-category setting, because there are no categories.
    //
    // The graph is deliberately **one level**: every bus routes to Master, and the enum is the
    // graph. A tree of arbitrary sub-buses is a mixing-desk feature, and this game has seven
    // categories and one output.
    enum class AudioBus
    {
        Master,
        Music,
        Dialogue,
        Ambience,
        Vehicle,
        Effects,
        Ui,
        Count,
    };

    inline constexpr std::size_t kAudioBusCount = static_cast<std::size_t>(AudioBus::Count);

    // Stable identifiers for settings files and data, so renaming the enum never invalidates a
    // player's saved mix -- the same convention GameActionId uses.
    [[nodiscard]] const char* AudioBusId(AudioBus bus) noexcept;
    [[nodiscard]] bool ParseAudioBusId(const std::string& id, AudioBus& out) noexcept;

    // plan_27 IG-27-004: how far everything else drops while dialogue is playing, and how long it
    // takes to get there and back. Ducking that snaps is more distracting than no ducking at all,
    // and coming back slower than it left is what stops a run of short lines pumping the mix.
    inline constexpr float kDialogueDuckGain = 0.35F;
    inline constexpr float kDialogueDuckAttackSeconds = 0.15F;
    inline constexpr float kDialogueDuckReleaseSeconds = 0.40F;

    // Which buses dialogue does **not** duck: dialogue itself, obviously, and the UI -- a menu
    // click going quiet because someone is talking is a bug, not a mix.
    [[nodiscard]] bool AudioBusIsDuckedByDialogue(AudioBus bus) noexcept;

    class AudioBusGraph final
    {
    public:
        AudioBusGraph();

        // Volumes are clamped to [0,1]. Setting one has no effect on any other bus.
        void SetVolume(AudioBus bus, float volume) noexcept;
        [[nodiscard]] float GetVolume(AudioBus bus) const noexcept;

        void SetMuted(AudioBus bus, bool muted) noexcept;
        [[nodiscard]] bool IsMuted(AudioBus bus) const noexcept;

        // Whether dialogue is currently playing. The duck itself ramps -- see Update().
        void SetDialogueActive(bool active) noexcept { dialogueActive_ = active; }
        [[nodiscard]] bool IsDialogueActive() const noexcept { return dialogueActive_; }
        // The current duck multiplier, 1 (no duck) down to kDialogueDuckGain. Exposed so a test
        // can watch the ramp rather than only its endpoints.
        [[nodiscard]] float GetDuckGain() const noexcept { return duckGain_; }

        // Advances the duck ramp. Called once per simulation update; a negative or zero delta
        // leaves the gain untouched rather than moving it backwards.
        void Update(float deltaSeconds) noexcept;

        // The gain a sound requesting @p requestedVolume on @p bus should actually play at:
        // its own volume, times its bus, times Master, times the duck if the bus is ducked.
        [[nodiscard]] float GetEffectiveVolume(AudioBus bus, float requestedVolume) const noexcept;

    private:
        std::array<float, kAudioBusCount> volumes_{};
        std::array<bool, kAudioBusCount> muted_{};
        bool dialogueActive_{false};
        float duckGain_{1.0F};
    };
}
