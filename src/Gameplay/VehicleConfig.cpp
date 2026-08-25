#include "IronGang/Gameplay/VehicleConfig.hpp"

#include "../Core/JsonDataFileInternal.hpp"

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

        bool IsKnownRootKey(const std::string& key)
        {
            return key == "id" || key == "version" || key == "chassis" || key == "wheels" ||
                   key == "performance" || key == "notes";
        }

        bool IsKnownKey(const std::string& section, const std::string& key)
        {
            if (section == "chassis")
            {
                return key == "mass" || key == "halfExtents";
            }
            if (section == "wheels")
            {
                return key == "radius" || key == "width" || key == "positions";
            }
            return key == "maxForwardSpeed" || key == "maxReverseSpeed";
        }

        void WarnUnknownKeys(const JsonElement& element,
                             const std::string& section,
                             std::vector<std::string>* warnings)
        {
            for (const auto& property : element.EnumerateObject())
            {
                if (!IsKnownKey(section, property.getNameProperty()))
                {
                    Warn(warnings, "Unknown vehicle key \"" + section + "." + property.getNameProperty() +
                                       "\"; ignored");
                }
            }
        }

        // A positive, finite number inside [minimum, maximum]. Anything else keeps the default and
        // says which value was refused -- a vehicle with zero mass or a zero-radius wheel does not
        // fail gracefully in a physics engine, it explodes.
        void ReadPositive(const JsonElement& section,
                          const char* sectionName,
                          const char* key,
                          float minimum,
                          float maximum,
                          float& out,
                          std::vector<std::string>* warnings)
        {
            JsonElement element;
            if (!section.TryGetProperty(key, element))
            {
                return;
            }
            const std::string label = std::string(sectionName) + "." + key;
            if (element.getValueKindProperty() != JsonValueKind::Number)
            {
                Warn(warnings, "\"" + label + "\" must be a number; keeping the default");
                return;
            }
            const float value = static_cast<float>(element.GetDouble());
            if (!(value >= minimum) || !(value <= maximum))
            {
                Warn(warnings, "\"" + label + "\" must be between " + std::to_string(minimum) + " and " +
                                   std::to_string(maximum) + "; keeping the default");
                return;
            }
            out = value;
        }

        bool ReadVector(const JsonElement& element, Vector3& out)
        {
            if (element.getValueKindProperty() != JsonValueKind::Array)
            {
                return false;
            }
            const std::vector<JsonElement> components = element.EnumerateArray();
            if (components.size() != 3)
            {
                return false;
            }
            for (const JsonElement& component : components)
            {
                if (component.getValueKindProperty() != JsonValueKind::Number)
                {
                    return false;
                }
            }
            out = Vector3(static_cast<float>(components[0].GetDouble()),
                          static_cast<float>(components[1].GetDouble()),
                          static_cast<float>(components[2].GetDouble()));
            return true;
        }
    }

    bool LoadVehicleConfig(const std::string& path,
                           VehicleConfig& out,
                           std::string& errorMessage,
                           std::vector<std::string>* warnings)
    {
        VehicleConfig config;
        if (!System::IO::File::Exists(path))
        {
            Warn(warnings, "Vehicle file not found (" + path + "); using the built-in sedan");
            out = config;
            return true;
        }

        // Same bounded read every data file gets (plan_36 IG-36-002/006/009).
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
                    errorMessage = "Vehicle file's \"version\" must be a number: " + path;
                    return false;
                }
                config.version = static_cast<int>(versionElement.GetInt32());
            }
            if (config.version < kMinVehicleConfigVersion || config.version > kMaxVehicleConfigVersion)
            {
                errorMessage = "Vehicle file has unsupported \"version\" " + std::to_string(config.version) +
                               " (supported: " + std::to_string(kMinVehicleConfigVersion) + "-" +
                               std::to_string(kMaxVehicleConfigVersion) + "): " + path;
                return false;
            }

            for (const auto& property : root.EnumerateObject())
            {
                if (!IsKnownRootKey(property.getNameProperty()))
                {
                    Warn(warnings, "Unknown vehicle key \"" + property.getNameProperty() + "\"; ignored");
                }
            }

            JsonElement idElement;
            if (root.TryGetProperty("id", idElement))
            {
                if (idElement.getValueKindProperty() == JsonValueKind::String && !idElement.GetString().empty())
                {
                    config.id = idElement.GetString();
                }
                else
                {
                    Warn(warnings, "\"id\" must be a non-empty string; keeping the default");
                }
            }

            JsonElement chassis;
            if (root.TryGetProperty("chassis", chassis) &&
                chassis.getValueKindProperty() == JsonValueKind::Object)
            {
                WarnUnknownKeys(chassis, "chassis", warnings);
                ReadPositive(chassis, "chassis", "mass", 50.0F, 20000.0F, config.chassisMass, warnings);
                JsonElement halfExtents;
                if (chassis.TryGetProperty("halfExtents", halfExtents))
                {
                    Vector3 parsed;
                    if (!ReadVector(halfExtents, parsed) || parsed.X <= 0.0F || parsed.Y <= 0.0F ||
                        parsed.Z <= 0.0F)
                    {
                        Warn(warnings,
                             "\"chassis.halfExtents\" must be three positive numbers; keeping the default");
                    }
                    else
                    {
                        config.chassisHalfExtents = parsed;
                    }
                }
            }

            JsonElement wheels;
            if (root.TryGetProperty("wheels", wheels) && wheels.getValueKindProperty() == JsonValueKind::Object)
            {
                WarnUnknownKeys(wheels, "wheels", warnings);
                ReadPositive(wheels, "wheels", "radius", 0.05F, 2.0F, config.wheelRadius, warnings);
                ReadPositive(wheels, "wheels", "width", 0.02F, 1.0F, config.wheelWidth, warnings);
                JsonElement positions;
                if (wheels.TryGetProperty("positions", positions))
                {
                    const bool isArray = positions.getValueKindProperty() == JsonValueKind::Array;
                    const std::vector<JsonElement> entries =
                        isArray ? positions.EnumerateArray() : std::vector<JsonElement>{};
                    if (!isArray || entries.size() != config.wheelPositions.size())
                    {
                        Warn(warnings, "\"wheels.positions\" must list exactly " +
                                           std::to_string(config.wheelPositions.size()) +
                                           " positions; keeping the defaults");
                    }
                    else
                    {
                        std::array<Vector3, 4> parsed = config.wheelPositions;
                        bool ok = true;
                        for (std::size_t index = 0; index < entries.size(); ++index)
                        {
                            ok = ReadVector(entries[index], parsed[index]) && ok;
                        }
                        if (!ok)
                        {
                            Warn(warnings,
                                 "\"wheels.positions\" entries must each be three numbers; keeping the defaults");
                        }
                        else
                        {
                            config.wheelPositions = parsed;
                        }
                    }
                }
            }

            JsonElement performance;
            if (root.TryGetProperty("performance", performance) &&
                performance.getValueKindProperty() == JsonValueKind::Object)
            {
                WarnUnknownKeys(performance, "performance", warnings);
                ReadPositive(performance, "performance", "maxForwardSpeed", 1.0F, 200.0F,
                             config.maxForwardSpeed, warnings);
                ReadPositive(performance, "performance", "maxReverseSpeed", 0.5F, 200.0F,
                             config.maxReverseSpeed, warnings);
            }

            if (config.maxReverseSpeed > config.maxForwardSpeed)
            {
                // Legal, and the physics will honour it -- but a car that reverses faster than it
                // drives forward is a typo far more often than a design.
                Warn(warnings, "\"performance.maxReverseSpeed\" is higher than \"maxForwardSpeed\"");
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
