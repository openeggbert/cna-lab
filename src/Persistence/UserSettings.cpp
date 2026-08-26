#include "IronGang/Persistence/UserSettings.hpp"

#include "../Core/JsonDataFileInternal.hpp"
#include "IronGang/Core/AtomicFile.hpp"

#include "System/IO/File.hpp"
#include "System/Text/Json/JsonProperty.hpp"

#include <array>
#include <charconv>
#include <sstream>
#include <system_error>

namespace IronGang
{
    using Microsoft::Xna::Framework::Input::Keys;
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    namespace
    {
        void Warn(std::vector<std::string>* warnings, std::string message)
        {
            if (warnings != nullptr)
            {
                warnings->push_back(std::move(message));
            }
        }

        std::string FloatToText(float value)
        {
            std::array<char, 48> buffer{};
            const std::to_chars_result result =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (result.ec != std::errc())
            {
                return "0";
            }
            return std::string(buffer.data(), result.ptr);
        }
    }

    bool LoadUserSettings(const std::string& path,
                          UserSettings& out,
                          std::string& errorMessage,
                          std::vector<std::string>* warnings)
    {
        UserSettings settings;
        if (!System::IO::File::Exists(path))
        {
            // Silent: a player who has never opened settings has no file, and saying so every
            // launch would be noise rather than information.
            out = settings;
            return true;
        }

        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        try
        {
            const JsonElement& root = file.root;
            JsonElement versionElement;
            if (root.TryGetProperty("version", versionElement))
            {
                if (versionElement.getValueKindProperty() != JsonValueKind::Number)
                {
                    errorMessage = "Settings \"version\" must be a number: " + path;
                    return false;
                }
                settings.version = static_cast<int>(versionElement.GetInt32());
            }
            if (settings.version != kUserSettingsVersion)
            {
                errorMessage = "Settings file has unsupported \"version\" " +
                               std::to_string(settings.version) + " (expected " +
                               std::to_string(kUserSettingsVersion) + "): " + path;
                return false;
            }

            for (const auto& property : root.EnumerateObject())
            {
                const std::string& name = property.getNameProperty();
                if (name != "version" && name != "masterVolume" && name != "showHud" &&
                    name != "bindings")
                {
                    Warn(warnings, "Unknown setting \"" + name + "\"; ignored");
                }
            }

            JsonElement volume;
            if (root.TryGetProperty("masterVolume", volume))
            {
                if (volume.getValueKindProperty() != JsonValueKind::Number)
                {
                    Warn(warnings, "\"masterVolume\" must be a number; keeping the default");
                }
                else
                {
                    const float value = static_cast<float>(volume.GetDouble());
                    if (value < 0.0F || value > 1.0F)
                    {
                        Warn(warnings, "\"masterVolume\" must be between 0 and 1; keeping the default");
                    }
                    else
                    {
                        settings.masterVolume = value;
                    }
                }
            }

            JsonElement showHud;
            if (root.TryGetProperty("showHud", showHud))
            {
                const JsonValueKind kind = showHud.getValueKindProperty();
                if (kind != JsonValueKind::True && kind != JsonValueKind::False)
                {
                    Warn(warnings, "\"showHud\" must be true or false; keeping the default");
                }
                else
                {
                    settings.showHud = showHud.GetBoolean();
                }
            }
            JsonElement bindings;
            if (root.TryGetProperty("bindings", bindings) &&
                bindings.getValueKindProperty() == JsonValueKind::Object)
            {
                for (const auto& property : bindings.EnumerateObject())
                {
                    GameAction action{};
                    if (!ParseGameActionId(property.getNameProperty(), action))
                    {
                        Warn(warnings, "Unknown bindable action \"" + property.getNameProperty() +
                                           "\"; ignored");
                        continue;
                    }
                    const JsonElement& keys = property.getValueProperty();
                    if (keys.getValueKindProperty() != JsonValueKind::Array)
                    {
                        Warn(warnings, "Binding for \"" + property.getNameProperty() +
                                           "\" must be a list of key names; keeping the default");
                        continue;
                    }

                    // All or nothing per action: half-applying a binding would leave the player
                    // with an action that works on one key and not the other it can see listed.
                    ActionBinding parsed;
                    bool ok = true;
                    std::size_t index = 0;
                    for (const JsonElement& entry : keys.EnumerateArray())
                    {
                        Keys key{};
                        if (index >= 2 || entry.getValueKindProperty() != JsonValueKind::String ||
                            !ParseKeyName(entry.GetString(), key))
                        {
                            ok = false;
                            break;
                        }
                        (index == 0 ? parsed.primary : parsed.secondary) = key;
                        ++index;
                    }
                    if (!ok)
                    {
                        Warn(warnings, "Binding for \"" + property.getNameProperty() +
                                           "\" names at most two known keys; keeping the default");
                        continue;
                    }
                    settings.bindings.Set(action, parsed);
                }
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        out = settings;
        return true;
    }

    bool SaveUserSettings(const std::string& path, const UserSettings& settings, std::string& errorMessage)
    {
        std::ostringstream text;
        text << "{\n";
        text << "  \"version\": " << kUserSettingsVersion << ",\n";
        text << "  \"masterVolume\": " << FloatToText(settings.masterVolume) << ",\n";
        text << "  \"showHud\": " << (settings.showHud ? "true" : "false") << ",\n";
        text << "  \"bindings\": {\n";
        for (std::size_t index = 0; index < static_cast<std::size_t>(GameAction::Count); ++index)
        {
            const auto action = static_cast<GameAction>(index);
            const ActionBinding& binding = settings.bindings.Get(action);
            text << "    \"" << GameActionId(action) << "\": [";
            const std::string primary = KeyName(binding.primary);
            const std::string secondary = KeyName(binding.secondary);
            if (!primary.empty())
            {
                text << "\"" << primary << "\"";
            }
            if (!secondary.empty())
            {
                text << (primary.empty() ? "" : ", ") << "\"" << secondary << "\"";
            }
            text << "]" << (index + 1 < static_cast<std::size_t>(GameAction::Count) ? "," : "") << "\n";
        }
        text << "  }\n";
        text << "}\n";
        return WriteTextFileAtomically(path, text.str(), true, errorMessage);
    }
}
