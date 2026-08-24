#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace CopperBoots
{
    enum class InputAction : std::size_t
    {
        MoveLeft,
        MoveRight,
        Run,
        Jump,
        Attack,
        AimUp,
        AimDown,
        Interact,
        Pause,
        Count,
    };

    enum class KeyboardKey
    {
        None,
        A,
        D,
        W,
        S,
        Left,
        Right,
        Up,
        Down,
        LeftShift,
        RightShift,
        Space,
        LeftControl,
        RightControl,
        Escape,
    };

    enum class PresentationStyle
    {
        IntegerScale,
        AspectFit,
    };

    inline constexpr std::size_t InputActionCount =
        static_cast<std::size_t>(InputAction::Count);
    using KeyboardBinding = std::array<KeyboardKey, 2>;

    struct GameSettings
    {
        static constexpr int CurrentVersion = 1;

        float MasterVolume = 0.80F;
        float EffectsVolume = 0.75F;
        bool Fullscreen = false;
        PresentationStyle Presentation = PresentationStyle::IntegerScale;
        std::array<KeyboardBinding, InputActionCount> KeyboardBindings;

        GameSettings();

        [[nodiscard]] const KeyboardBinding& Binding(
            InputAction action) const noexcept;
        [[nodiscard]] KeyboardBinding& Binding(InputAction action) noexcept;
        bool operator==(const GameSettings&) const = default;
    };

    enum class SettingsLoadStatus
    {
        Loaded,
        Migrated,
        DefaultedMissing,
        DefaultedInvalid,
    };

    struct SettingsLoadResult
    {
        GameSettings Settings;
        SettingsLoadStatus Status = SettingsLoadStatus::DefaultedMissing;
    };

    [[nodiscard]] SettingsLoadResult DecodeSettings(
        std::optional<std::string_view> document) noexcept;
    [[nodiscard]] std::string EncodeSettings(const GameSettings& settings);
}
