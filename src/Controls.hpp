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
    inline constexpr int DefaultTurnSensitivityStep = 2;
    inline constexpr int MaximumTurnSensitivityStep = 4;

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
        int turnSensitivityStep = DefaultTurnSensitivityStep;

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

    [[nodiscard]] inline bool IsControlDown(
        const KeyboardState& keyboard,
        const ControlSettings& settings,
        ControlAction action)
    {
        const Keys key = settings.bindings[ControlIndex(action)];
        if (key == Keys::LeftShift)
            return keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
        if (key == Keys::LeftControl)
            return keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        if (key == Keys::LeftAlt)
            return keyboard.IsKeyDown(Keys::LeftAlt) || keyboard.IsKeyDown(Keys::RightAlt);
        return keyboard.IsKeyDown(key);
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
