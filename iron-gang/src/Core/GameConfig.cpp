#include "IronGang/Core/GameConfig.hpp"

#include "JsonDataFileInternal.hpp"

#include "System/IO/File.hpp"
#include "System/Text/Json/JsonProperty.hpp"

namespace IronGang
{
    using System::Text::Json::JsonDocument;
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

        // Every key the loader knows. Anything else in the file is a typo or a leftover, and
        // silently ignoring it is how a mistuned build goes unnoticed for a week.
        bool IsKnownKey(const std::string& key)
        {
            return key == "projectName" || key == "cityName" || key == "prototypeYear" ||
                   key == "autosaveIntervalSeconds" || key == "autosaveMinimumSpacingSeconds" ||
                   key == "logSeverity" || key == "maxSkinnedPedestrians" ||
                   key == "notes"; // accepted and ignored: the file may carry a comment
        }

        void ReadString(const JsonElement& root,
                        const char* key,
                        std::string& out,
                        std::vector<std::string>* warnings)
        {
            JsonElement element;
            if (!root.TryGetProperty(key, element))
            {
                return;
            }
            if (element.getValueKindProperty() != JsonValueKind::String)
            {
                Warn(warnings, std::string("\"") + key + "\" must be a string; keeping the default");
                return;
            }
            const std::string value = element.GetString();
            if (value.empty())
            {
                Warn(warnings, std::string("\"") + key + "\" must not be empty; keeping the default");
                return;
            }
            out = value;
        }

        void ReadInt(const JsonElement& root,
                     const char* key,
                     int minimum,
                     int maximum,
                     int& out,
                     std::vector<std::string>* warnings)
        {
            JsonElement element;
            if (!root.TryGetProperty(key, element))
            {
                return;
            }
            if (element.getValueKindProperty() != JsonValueKind::Number)
            {
                Warn(warnings, std::string("\"") + key + "\" must be a number; keeping the default");
                return;
            }
            const int value = static_cast<int>(element.GetInt32());
            if (value < minimum || value > maximum)
            {
                Warn(warnings, std::string("\"") + key + "\" is outside " + std::to_string(minimum) + "-" +
                                   std::to_string(maximum) + "; keeping the default");
                return;
            }
            out = value;
        }

        // Negative seconds are the one case worth clamping rather than rejecting: the author
        // clearly meant "off", and 0 is exactly that for both of these values.
        void ReadSeconds(const JsonElement& root,
                         const char* key,
                         float& out,
                         std::vector<std::string>* warnings)
        {
            JsonElement element;
            if (!root.TryGetProperty(key, element))
            {
                return;
            }
            if (element.getValueKindProperty() != JsonValueKind::Number)
            {
                Warn(warnings, std::string("\"") + key + "\" must be a number of seconds; keeping the default");
                return;
            }
            const float value = static_cast<float>(element.GetDouble());
            if (value < 0.0F)
            {
                Warn(warnings, std::string("\"") + key + "\" cannot be negative; using 0");
                out = 0.0F;
                return;
            }
            out = value;
        }
    }

    bool LoadGameConfig(const std::string& path,
                        GameConfig& out,
                        std::string& errorMessage,
                        std::vector<std::string>* warnings)
    {
        GameConfig config;
        if (!System::IO::File::Exists(path))
        {
            // Not a failure: the defaults above are a complete, playable configuration.
            Warn(warnings, "Configuration file not found (" + path + "); using defaults");
            out = config;
            return true;
        }

        // plan_36 IG-36-002/006/009: size, UTF-8, nesting depth, and the object root are all
        // checked before the parser sees the text -- see LoadJsonDataFile.
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        try
        {
            const JsonElement& root = file.root;

            for (const auto& property : root.EnumerateObject())
            {
                if (!IsKnownKey(property.getNameProperty()))
                {
                    Warn(warnings,
                         "Unknown configuration key \"" + property.getNameProperty() + "\"; ignored");
                }
            }

            ReadString(root, "projectName", config.projectName, warnings);
            ReadString(root, "cityName", config.cityName, warnings);
            ReadInt(root, "prototypeYear", 1800, 2200, config.prototypeYear, warnings);
            ReadSeconds(root, "autosaveIntervalSeconds", config.autosaveIntervalSeconds, warnings);
            ReadSeconds(root, "autosaveMinimumSpacingSeconds", config.autosaveMinimumSpacingSeconds, warnings);

            ReadInt(root, "maxSkinnedPedestrians", 0, 64, config.maxSkinnedPedestrians, warnings);

            std::string severityName;
            ReadString(root, "logSeverity", severityName, warnings);
            if (!severityName.empty() && !ParseLogSeverity(severityName, config.logSeverity))
            {
                Warn(warnings, "\"logSeverity\" must be debug/info/warning/error, not \"" + severityName +
                                   "\"; keeping the default");
            }

            if (config.autosaveIntervalSeconds > 0.0F &&
                config.autosaveMinimumSpacingSeconds > config.autosaveIntervalSeconds)
            {
                // Not an error -- the spacing simply wins -- but it means the interval never fires
                // when it says it will, which is almost always a mistake rather than an intent.
                Warn(warnings,
                     "\"autosaveMinimumSpacingSeconds\" is longer than \"autosaveIntervalSeconds\", so "
                     "periodic autosaves happen at the spacing instead");
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        out = config;
        return true;
    }
}
