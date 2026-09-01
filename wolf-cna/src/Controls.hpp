#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

namespace WolfCna
{
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    enum class ControlAction : int
    {
        MoveForward,
        MoveBackward,
        TurnLeft,
        TurnRight,
        StrafeLeft,
        StrafeRight,
        Run,
        Action,
        Attack,
        Map
    };

    inline constexpr std::array BindableControlActions{
        ControlAction::MoveForward,
        ControlAction::MoveBackward,
        ControlAction::TurnLeft,
        ControlAction::TurnRight,
        ControlAction::StrafeLeft,
        ControlAction::StrafeRight,
        ControlAction::Run,
        ControlAction::Action,
        ControlAction::Attack,
        ControlAction::Map};
    inline constexpr std::size_t ControlActionCount = BindableControlActions.size();

    // What a physical mouse button does. These mirror the original's bt_attack, bt_strafe,
    // bt_use, bt_run and bt_nobutton, which is what its buttonmouse[] array selected from.
    enum class MouseButtonAction : int
    {
        None,
        Attack,
        StrafeModifier,
        Action,
        Run
    };

    inline constexpr std::array AssignableMouseButtonActions{
        MouseButtonAction::None,
        MouseButtonAction::Attack,
        MouseButtonAction::StrafeModifier,
        MouseButtonAction::Action,
        MouseButtonAction::Run};

    // Index order is physical: left, middle, right.
    inline constexpr std::size_t MouseButtonCount = 3;
    inline constexpr int DefaultTurnSensitivityStep = 2;
    inline constexpr int MaximumTurnSensitivityStep = 4;
    inline constexpr int DefaultMouseSensitivityStep = 2;
    inline constexpr int MaximumMouseSensitivityStep = 4;

    // Yaw applied by one mouse count at 100%. A full turn takes roughly 2,850 counts,
    // which matches the travel a 1992-style mouse needed for a 360-degree spin.
    inline constexpr float BaseMouseYawRadiansPerCount = 0.0022f;

    // A single frame never yaws by more than half a turn, so a pathological displacement
    // spike cannot spin the player through several revolutions or leave the direction it
    // turned ambiguous. The bound is deliberately expressed in radians rather than counts:
    // a count bound would depend on mouse DPI and, because it truncates per frame rather
    // than carrying the remainder, would silently shrink fast flicks and halve the maximum
    // turn rate whenever the frame rate dropped.
    inline constexpr float MaximumMouseYawRadiansPerFrame = 3.14159265f;

    struct ControlSettings final
    {
        std::array<Keys, ControlActionCount> bindings{
            Keys::Up,
            Keys::Down,
            Keys::Left,
            Keys::Right,
            Keys::A,
            Keys::D,
            Keys::LeftShift,
            Keys::Space,
            Keys::LeftControl,
            Keys::Tab};

        // Secondary keys, so WASD works without giving up the classic arrows. Keys::None
        // means an action has no secondary key. A/D already strafe, so only forward and
        // backward need one to complete the modern layout.
        std::array<Keys, ControlActionCount> alternateBindings{
            Keys::W,
            Keys::S,
            Keys::None,
            Keys::None,
            Keys::None,
            Keys::None,
            Keys::None,
            Keys::None,
            Keys::None,
            Keys::None};
        int turnSensitivityStep = DefaultTurnSensitivityStep;
        bool mouseEnabled = true;
        int mouseSensitivityStep = DefaultMouseSensitivityStep;

        // The original moved the player forward and backward with vertical mouse motion,
        // since it had no vertical look to spend the axis on. That is preserved as an
        // option but defaults off: it is the one 1992 mouse behaviour players actively
        // worked around at the time.
        bool mouseYMovesForward = false;

        // Classic defaults. The original's buttonmouse[] held {bt_attack, bt_strafe,
        // bt_use} against an INT 33h button mask whose bits are left, RIGHT, middle in
        // that order, so the original assignment is left attack, right strafe, middle use
        // -- not the left/middle/right reading the array order suggests at a glance.
        std::array<MouseButtonAction, MouseButtonCount> mouseButtons{
            MouseButtonAction::Attack,
            MouseButtonAction::Action,
            MouseButtonAction::StrafeModifier};

        bool operator==(const ControlSettings&) const = default;
    };

    struct RebindResult final
    {
        bool accepted = false;
        std::optional<ControlAction> swappedAction;
    };

    struct MovementInput final
    {
        float forward = 0.0f;
        float strafe = 0.0f;
    };

    [[nodiscard]] constexpr std::size_t ControlIndex(ControlAction action)
    {
        return static_cast<std::size_t>(action);
    }

    [[nodiscard]] constexpr Keys NormalizeControlKey(Keys key)
    {
        if (key == Keys::RightShift)
            return Keys::LeftShift;
        if (key == Keys::RightControl)
            return Keys::LeftControl;
        if (key == Keys::RightAlt)
            return Keys::LeftAlt;
        return key;
    }

    [[nodiscard]] constexpr bool IsBindableControlKey(Keys key)
    {
        key = NormalizeControlKey(key);
        const int value = static_cast<int>(key);
        if (value >= static_cast<int>(Keys::A) && value <= static_cast<int>(Keys::Z))
            return key != Keys::P;
        return key == Keys::Up || key == Keys::Down || key == Keys::Left ||
            key == Keys::Right || key == Keys::Tab || key == Keys::Space ||
            key == Keys::LeftShift || key == Keys::LeftControl || key == Keys::LeftAlt;
    }

    [[nodiscard]] inline bool AreValidControlSettings(const ControlSettings& settings)
    {
        if (settings.turnSensitivityStep < 0 ||
            settings.turnSensitivityStep > MaximumTurnSensitivityStep)
        {
            return false;
        }
        if (settings.mouseSensitivityStep < 0 ||
            settings.mouseSensitivityStep > MaximumMouseSensitivityStep)
        {
            return false;
        }
        for (const MouseButtonAction action : settings.mouseButtons)
        {
            if (std::find(
                    AssignableMouseButtonActions.begin(),
                    AssignableMouseButtonActions.end(),
                    action) == AssignableMouseButtonActions.end())
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < settings.bindings.size(); ++index)
        {
            const Keys key = settings.bindings[index];
            if (!IsBindableControlKey(key) || NormalizeControlKey(key) != key)
                return false;
            if (std::find(settings.bindings.begin(), settings.bindings.begin() +
                    static_cast<std::ptrdiff_t>(index), key) !=
                settings.bindings.begin() + static_cast<std::ptrdiff_t>(index))
            {
                return false;
            }
        }

        // A secondary key may be absent, but a present one has to obey the same rules and
        // stay unique across both sets: one key must never drive two actions at once.
        for (std::size_t index = 0; index < settings.alternateBindings.size(); ++index)
        {
            const Keys key = settings.alternateBindings[index];
            if (key == Keys::None)
                continue;
            if (!IsBindableControlKey(key) || NormalizeControlKey(key) != key)
                return false;
            if (std::find(settings.bindings.begin(), settings.bindings.end(), key) !=
                settings.bindings.end())
            {
                return false;
            }
            if (std::find(settings.alternateBindings.begin(),
                    settings.alternateBindings.begin() +
                        static_cast<std::ptrdiff_t>(index),
                    key) !=
                settings.alternateBindings.begin() + static_cast<std::ptrdiff_t>(index))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline RebindResult RebindControl(
        ControlSettings& settings,
        ControlAction action,
        Keys requestedKey)
    {
        requestedKey = NormalizeControlKey(requestedKey);
        if (!IsBindableControlKey(requestedKey))
            return {};

        const std::size_t actionIndex = ControlIndex(action);
        const Keys previousKey = settings.bindings[actionIndex];

        // Claiming a key that is some action's secondary must release it there, otherwise
        // one press would drive two actions and the settings would fail validation.
        for (Keys& alternate : settings.alternateBindings)
        {
            if (alternate == requestedKey)
                alternate = Keys::None;
        }

        const auto conflict = std::find(
            settings.bindings.begin(),
            settings.bindings.end(),
            requestedKey);
        if (conflict == settings.bindings.end())
        {
            settings.bindings[actionIndex] = requestedKey;
            return {true, std::nullopt};
        }

        const std::size_t conflictIndex = static_cast<std::size_t>(
            std::distance(settings.bindings.begin(), conflict));
        if (conflictIndex == actionIndex)
            return {true, std::nullopt};

        settings.bindings[actionIndex] = requestedKey;
        settings.bindings[conflictIndex] = previousKey;
        return {true, BindableControlActions[conflictIndex]};
    }

    [[nodiscard]] inline bool IsBoundKeyDown(const KeyboardState& keyboard, Keys key)
    {
        if (key == Keys::None)
            return false;
        if (key == Keys::LeftShift)
            return keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
        if (key == Keys::LeftControl)
            return keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        if (key == Keys::LeftAlt)
            return keyboard.IsKeyDown(Keys::LeftAlt) || keyboard.IsKeyDown(Keys::RightAlt);
        return keyboard.IsKeyDown(key);
    }

    [[nodiscard]] inline bool IsControlDown(
        const KeyboardState& keyboard,
        const ControlSettings& settings,
        ControlAction action)
    {
        const std::size_t index = ControlIndex(action);
        return IsBoundKeyDown(keyboard, settings.bindings[index]) ||
            IsBoundKeyDown(keyboard, settings.alternateBindings[index]);
    }

    [[nodiscard]] inline MovementInput NormalizeMovementInput(MovementInput input)
    {
        input.forward = std::clamp(input.forward, -1.0f, 1.0f);
        input.strafe = std::clamp(input.strafe, -1.0f, 1.0f);
        const float lengthSquared = input.forward * input.forward + input.strafe * input.strafe;
        if (lengthSquared > 1.0f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            input.forward *= inverseLength;
            input.strafe *= inverseLength;
        }
        return input;
    }

    [[nodiscard]] constexpr int TurnSensitivityPercent(int step)
    {
        return 70 + std::clamp(step, 0, MaximumTurnSensitivityStep) * 15;
    }

    [[nodiscard]] constexpr float TurnSensitivityMultiplier(int step)
    {
        return static_cast<float>(TurnSensitivityPercent(step)) / 100.0f;
    }

    [[nodiscard]] constexpr int MouseSensitivityPercent(int step)
    {
        return 40 + std::clamp(step, 0, MaximumMouseSensitivityStep) * 30;
    }

    // Mouse yaw is frame-rate independent for every movement below the half-turn bound:
    // relative counts already describe distance moved, so unlike keyboard turning this
    // must not be scaled by elapsed time. Equal hand travel therefore produces equal
    // rotation whether it arrives in one frame or spread across ten.
    [[nodiscard]] constexpr float MouseYawDeltaRadians(
        int counts,
        const ControlSettings& settings)
    {
        if (!settings.mouseEnabled)
            return 0.0f;
        const float yaw = static_cast<float>(counts) *
            BaseMouseYawRadiansPerCount *
            (static_cast<float>(MouseSensitivityPercent(settings.mouseSensitivityStep)) /
                100.0f);
        return std::clamp(
            yaw,
            -MaximumMouseYawRadiansPerFrame,
            MaximumMouseYawRadiansPerFrame);
    }

    // The original drove forward motion from vertical mouse travel at twice the gain of
    // the turning axis (controly took *20 where controlx took *10). That ratio is kept,
    // but the result is a movement axis rather than a raw displacement, so it feeds the
    // same normalization, speed and collision path as the movement keys.
    inline constexpr float BaseMouseForwardAxisPerCount = 0.025f;

    // Counts grow downward on screen, and pushing the mouse away from you must move the
    // player forward, so the axis is negated.
    [[nodiscard]] constexpr float MouseForwardAxis(
        int counts,
        const ControlSettings& settings)
    {
        if (!settings.mouseEnabled || !settings.mouseYMovesForward)
            return 0.0f;
        const float axis = static_cast<float>(-counts) *
            BaseMouseForwardAxisPerCount *
            (static_cast<float>(MouseSensitivityPercent(settings.mouseSensitivityStep)) /
                100.0f);
        return std::clamp(axis, -1.0f, 1.0f);
    }

    // Half the forward gain, mirroring the original's *10 horizontal against *20 vertical.
    // Unlike the forward axis this is not opt-in: it only ever applies while the strafe
    // modifier is held, which is already an explicit request to sidestep.
    [[nodiscard]] constexpr float MouseStrafeAxis(
        int counts,
        const ControlSettings& settings)
    {
        if (!settings.mouseEnabled)
            return 0.0f;
        const float axis = static_cast<float>(counts) *
            (BaseMouseForwardAxisPerCount / 2.0f) *
            (static_cast<float>(MouseSensitivityPercent(settings.mouseSensitivityStep)) /
                100.0f);
        return std::clamp(axis, -1.0f, 1.0f);
    }

    [[nodiscard]] constexpr std::string_view MouseButtonName(std::size_t button)
    {
        switch (button)
        {
        case 0: return "LEFT BUTTON";
        case 1: return "MIDDLE BUTTON";
        case 2: return "RIGHT BUTTON";
        }
        return "BUTTON";
    }

    [[nodiscard]] constexpr std::string_view MouseButtonActionName(MouseButtonAction action)
    {
        switch (action)
        {
        case MouseButtonAction::None: return "NONE";
        case MouseButtonAction::Attack: return "ATTACK";
        case MouseButtonAction::StrafeModifier: return "STRAFE";
        case MouseButtonAction::Action: return "USE";
        case MouseButtonAction::Run: return "RUN";
        }
        return "NONE";
    }

    // Yaw accumulates every frame and mouse look raises its growth rate by more than an
    // order of magnitude over keyboard turning, so it is wrapped to keep float precision
    // constant instead of letting aim quantize over a long session.
    [[nodiscard]] inline float WrapYawRadians(float yaw)
    {
        constexpr float turn = 2.0f * MaximumMouseYawRadiansPerFrame;
        yaw = std::fmod(yaw, turn);
        if (yaw <= -MaximumMouseYawRadiansPerFrame)
            yaw += turn;
        else if (yaw > MaximumMouseYawRadiansPerFrame)
            yaw -= turn;
        return yaw;
    }

    [[nodiscard]] constexpr std::string_view ControlActionName(ControlAction action)
    {
        switch (action)
        {
        case ControlAction::MoveForward: return "FORWARD";
        case ControlAction::MoveBackward: return "BACKWARD";
        case ControlAction::TurnLeft: return "TURN LEFT";
        case ControlAction::TurnRight: return "TURN RIGHT";
        case ControlAction::StrafeLeft: return "STRAFE LEFT";
        case ControlAction::StrafeRight: return "STRAFE RIGHT";
        case ControlAction::Run: return "RUN";
        case ControlAction::Action: return "ACTION";
        case ControlAction::Attack: return "ATTACK";
        case ControlAction::Map: return "MAP";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] inline std::string ControlKeyName(Keys key)
    {
        key = NormalizeControlKey(key);
        const int value = static_cast<int>(key);
        if (value >= static_cast<int>(Keys::A) && value <= static_cast<int>(Keys::Z))
            return std::string(1, static_cast<char>('A' + value - static_cast<int>(Keys::A)));
        switch (key)
        {
        case Keys::Up: return "UP";
        case Keys::Down: return "DOWN";
        case Keys::Left: return "LEFT";
        case Keys::Right: return "RIGHT";
        case Keys::Tab: return "TAB";
        case Keys::Space: return "SPACE";
        case Keys::LeftShift: return "SHIFT";
        case Keys::LeftControl: return "CTRL";
        case Keys::LeftAlt: return "ALT";
        default: return "RESERVED";
        }
    }
}
