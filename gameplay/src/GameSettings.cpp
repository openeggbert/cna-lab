#include "CopperBoots/GameSettings.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace CopperBoots
{
    namespace
    {
        constexpr std::array<std::string_view, InputActionCount> ActionNames{
            "left", "right", "run", "jump", "attack",
            "aim-up", "aim-down", "interact", "pause"};

        constexpr std::array<std::pair<std::string_view, KeyboardKey>, 15>
            KeyNames{{
                {"None", KeyboardKey::None}, {"A", KeyboardKey::A},
                {"D", KeyboardKey::D}, {"W", KeyboardKey::W},
                {"S", KeyboardKey::S}, {"Left", KeyboardKey::Left},
                {"Right", KeyboardKey::Right}, {"Up", KeyboardKey::Up},
                {"Down", KeyboardKey::Down},
                {"LeftShift", KeyboardKey::LeftShift},
                {"RightShift", KeyboardKey::RightShift},
                {"Space", KeyboardKey::Space},
                {"LeftControl", KeyboardKey::LeftControl},
                {"RightControl", KeyboardKey::RightControl},
                {"Escape", KeyboardKey::Escape},
            }};

        [[nodiscard]] std::vector<std::string_view> SplitLines(
            std::string_view text)
        {
            std::vector<std::string_view> lines;
            while (!text.empty()) {
                const std::size_t end = text.find('\n');
                std::string_view line = text.substr(0, end);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                lines.push_back(line);
                if (end == std::string_view::npos)
                    break;
                text.remove_prefix(end + 1);
            }
            if (!lines.empty() && lines.back().empty())
                lines.pop_back();
            return lines;
        }

        [[nodiscard]] std::string_view After(
            const std::string_view line, const std::string_view prefix)
        {
            if (!line.starts_with(prefix))
                throw std::runtime_error("unexpected settings directive");
            const std::string_view value = line.substr(prefix.size());
            if (value.empty())
                throw std::runtime_error("missing settings value");
            return value;
        }

        [[nodiscard]] float ParseVolume(const std::string_view text)
        {
            float value = 0.0F;
            const auto result = std::from_chars(
                text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} ||
                result.ptr != text.data() + text.size() ||
                !std::isfinite(value) || value < 0.0F || value > 1.0F) {
                throw std::runtime_error("invalid settings volume");
            }
            return value;
        }

        [[nodiscard]] bool ParseBool(const std::string_view text)
        {
            if (text == "0")
                return false;
            if (text == "1")
                return true;
            throw std::runtime_error("invalid settings boolean");
        }

        [[nodiscard]] KeyboardKey ParseKey(const std::string_view text)
        {
            for (const auto& [name, key] : KeyNames) {
                if (text == name)
                    return key;
            }
            throw std::runtime_error("unknown settings key");
        }

        [[nodiscard]] std::string_view KeyName(const KeyboardKey key)
        {
            for (const auto& [name, candidate] : KeyNames) {
                if (key == candidate)
                    return name;
            }
            throw std::runtime_error("unserializable settings key");
        }

        [[nodiscard]] KeyboardBinding ParseBinding(const std::string_view text)
        {
            const std::size_t separator = text.find(' ');
            if (separator == std::string_view::npos ||
                text.find(' ', separator + 1) != std::string_view::npos) {
                throw std::runtime_error("invalid settings binding");
            }
            KeyboardBinding result{
                ParseKey(text.substr(0, separator)),
                ParseKey(text.substr(separator + 1))};
            if (result[0] == KeyboardKey::None &&
                result[1] == KeyboardKey::None) {
                throw std::runtime_error("settings action cannot be unbound");
            }
            return result;
        }

        [[nodiscard]] std::string FormatVolume(const float value)
        {
            std::array<char, 32> buffer{};
            const auto result = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value,
                std::chars_format::fixed, 3);
            if (result.ec != std::errc{})
                throw std::runtime_error("cannot serialize settings volume");
            return std::string(buffer.data(), result.ptr);
        }

        [[nodiscard]] GameSettings DecodeVersionOne(
            const std::vector<std::string_view>& lines)
        {
            if (lines.size() != 5U + InputActionCount)
                throw std::runtime_error("wrong settings field count");
            GameSettings settings;
            settings.MasterVolume = ParseVolume(
                After(lines[1], "master-volume "));
            settings.EffectsVolume = ParseVolume(
                After(lines[2], "effects-volume "));
            settings.Fullscreen = ParseBool(After(lines[3], "fullscreen "));
            const std::string_view presentation = After(
                lines[4], "presentation ");
            if (presentation == "integer")
                settings.Presentation = PresentationStyle::IntegerScale;
            else if (presentation == "fit")
                settings.Presentation = PresentationStyle::AspectFit;
            else
                throw std::runtime_error("unknown presentation mode");

            for (std::size_t index = 0; index < InputActionCount; ++index) {
                settings.KeyboardBindings[index] = ParseBinding(After(
                    lines[5 + index],
                    "bind-" + std::string(ActionNames[index]) + ' '));
            }
            return settings;
        }

        [[nodiscard]] GameSettings DecodeVersionZero(
            const std::vector<std::string_view>& lines)
        {
            if (lines.size() != 3U)
                throw std::runtime_error("wrong legacy settings field count");
            GameSettings settings;
            const float volume = ParseVolume(After(lines[1], "sound-volume "));
            settings.MasterVolume = volume;
            settings.EffectsVolume = 1.0F;
            settings.Fullscreen = ParseBool(After(lines[2], "fullscreen "));
            return settings;
        }
    }

    GameSettings::GameSettings()
        : KeyboardBindings{{
            KeyboardBinding{KeyboardKey::A, KeyboardKey::Left},
            KeyboardBinding{KeyboardKey::D, KeyboardKey::Right},
            KeyboardBinding{KeyboardKey::LeftShift, KeyboardKey::RightShift},
            KeyboardBinding{KeyboardKey::Space, KeyboardKey::None},
            KeyboardBinding{KeyboardKey::LeftControl, KeyboardKey::RightControl},
            KeyboardBinding{KeyboardKey::W, KeyboardKey::Up},
            KeyboardBinding{KeyboardKey::S, KeyboardKey::Down},
            KeyboardBinding{KeyboardKey::S, KeyboardKey::Down},
            KeyboardBinding{KeyboardKey::Escape, KeyboardKey::None},
        }}
    {
    }

    const KeyboardBinding& GameSettings::Binding(
        const InputAction action) const noexcept
    {
        return KeyboardBindings[static_cast<std::size_t>(action)];
    }

    KeyboardBinding& GameSettings::Binding(const InputAction action) noexcept
    {
        return KeyboardBindings[static_cast<std::size_t>(action)];
    }

    SettingsLoadResult DecodeSettings(
        const std::optional<std::string_view> document) noexcept
    {
        if (!document.has_value())
            return {GameSettings{}, SettingsLoadStatus::DefaultedMissing};
        try {
            const std::vector<std::string_view> lines = SplitLines(*document);
            if (lines.empty())
                throw std::runtime_error("empty settings document");
            if (lines[0] == "copper-boots-settings 1") {
                return {DecodeVersionOne(lines), SettingsLoadStatus::Loaded};
            }
            if (lines[0] == "copper-boots-settings 0") {
                return {DecodeVersionZero(lines), SettingsLoadStatus::Migrated};
            }
        }
        catch (...) {
        }
        return {GameSettings{}, SettingsLoadStatus::DefaultedInvalid};
    }

    std::string EncodeSettings(const GameSettings& settings)
    {
        std::string result =
            "copper-boots-settings 1\n"
            "master-volume " + FormatVolume(settings.MasterVolume) + "\n" +
            "effects-volume " + FormatVolume(settings.EffectsVolume) + "\n" +
            "fullscreen " + std::string(settings.Fullscreen ? "1\n" : "0\n") +
            "presentation " +
                std::string(settings.Presentation == PresentationStyle::IntegerScale
                    ? "integer\n" : "fit\n");
        for (std::size_t index = 0; index < InputActionCount; ++index) {
            const KeyboardBinding& binding = settings.KeyboardBindings[index];
            result += "bind-" + std::string(ActionNames[index]) + ' ' +
                      std::string(KeyName(binding[0])) + ' ' +
                      std::string(KeyName(binding[1])) + '\n';
        }
        return result;
    }
}
