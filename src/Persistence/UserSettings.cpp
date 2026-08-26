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
                if (name != "version" && name != "masterVolume" && name != "showHud")
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
        text << "  \"showHud\": " << (settings.showHud ? "true" : "false") << "\n";
        text << "}\n";
        return WriteTextFileAtomically(path, text.str(), true, errorMessage);
    }
}
