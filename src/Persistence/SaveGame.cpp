#include "IronGang/Persistence/SaveGame.hpp"

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"

#include <cstring>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <unordered_map>

namespace IronGang
{
    namespace
    {
        std::string VectorToText(const Vector3& value)
        {
            std::ostringstream output;
            output << value.X << ',' << value.Y << ',' << value.Z;
            return output.str();
        }

        bool ParseVector(const std::string& text, Vector3& value)
        {
            std::istringstream input(text);
            char commaA = '\0';
            char commaB = '\0';
            return static_cast<bool>(input >> value.X >> commaA >> value.Y >> commaB >> value.Z) &&
                   commaA == ',' && commaB == ',';
        }

        constexpr const char* kMissionVariablePrefix = "mission_var.";
        constexpr const char* kMissionCheckpointVariablePrefix = "mission_checkpoint_var.";

        // "mission_var.<name>" / "<type>:<text>". A malformed entry is skipped rather than
        // failing the whole load: the rest of the save is still a valid, resumable game.
        bool ParseMissionVariable(const std::string& key, const char* prefix, const std::string& text,
                                  MissionVariableSnapshot& out)
        {
            const std::string name = key.substr(std::strlen(prefix));
            const std::size_t typeSeparator = text.find(':');
            if (name.empty() || typeSeparator == std::string::npos)
            {
                return false;
            }
            MissionValueType type{};
            if (!ParseMissionValueType(text.substr(0, typeSeparator), type))
            {
                return false;
            }
            MissionValue value;
            if (!MissionValue::Parse(type, text.substr(typeSeparator + 1), value))
            {
                return false;
            }
            out.name = name;
            out.value = std::move(value);
            return true;
        }
    }

    bool SaveGame::Write(const std::string& path,
                         const SaveSnapshot& snapshot,
                         std::string& errorMessage)
    {
        try
        {
            const std::filesystem::path filesystemPath(path);
            const std::filesystem::path parent = filesystemPath.parent_path();
            if (!parent.empty() && !System::IO::Directory::Exists(parent.string()))
            {
                System::IO::Directory::CreateDirectory(parent.string());
            }

            std::ostringstream text;
            text << "format=iron-gang-save-v1\n";
            text << "mission_state_id=" << snapshot.missionStateId << "\n";
            text << "player_position=" << VectorToText(snapshot.playerPosition) << "\n";
            text << "player_yaw=" << snapshot.playerYaw << "\n";
            text << "vehicle_position=" << VectorToText(snapshot.vehiclePosition) << "\n";
            text << "vehicle_yaw=" << snapshot.vehicleYaw << "\n";
            text << "vehicle_speed=" << snapshot.vehicleSpeed << "\n";
            text << "player_driving=" << (snapshot.playerDriving ? 1 : 0) << "\n";
            text << "district_id=" << static_cast<int>(snapshot.districtId) << "\n";
            for (const MissionVariableSnapshot& variable : snapshot.missionVariables)
            {
                // One line per variable so a string value may contain any character except a
                // newline: the reader splits at the first '=', and a variable name is an
                // identifier, so neither side can be ambiguous.
                text << kMissionVariablePrefix << variable.name << "="
                     << MissionValueTypeName(variable.value.GetType()) << ":"
                     << variable.value.ToText() << "\n";
            }
            if (!snapshot.missionCheckpoint.stateId.empty())
            {
                text << "mission_checkpoint_state_id=" << snapshot.missionCheckpoint.stateId << "\n";
                for (const MissionVariableSnapshot& variable : snapshot.missionCheckpoint.variables)
                {
                    text << kMissionCheckpointVariablePrefix << variable.name << "="
                         << MissionValueTypeName(variable.value.GetType()) << ":"
                         << variable.value.ToText() << "\n";
                }
            }
            System::IO::File::WriteAllText(path, text.str());
            return true;
        }
        catch (const std::exception& exception)
        {
            errorMessage = exception.what();
            return false;
        }
    }

    std::optional<SaveSnapshot> SaveGame::Read(const std::string& path,
                                               std::string& errorMessage)
    {
        if (!System::IO::File::Exists(path))
        {
            errorMessage = "Save file does not exist: " + path;
            return std::nullopt;
        }

        try
        {
            std::unordered_map<std::string, std::string> values;
            std::vector<MissionVariableSnapshot> missionVariables;
            std::vector<MissionVariableSnapshot> checkpointVariables;
            for (const std::string& line : System::IO::File::ReadAllLines(path))
            {
                const std::size_t separator = line.find('=');
                if (separator == std::string::npos)
                {
                    continue;
                }
                const std::string key = line.substr(0, separator);
                const std::string value = line.substr(separator + 1);
                if (key.compare(0, std::strlen(kMissionVariablePrefix), kMissionVariablePrefix) == 0)
                {
                    MissionVariableSnapshot variable;
                    if (ParseMissionVariable(key, kMissionVariablePrefix, value, variable))
                    {
                        missionVariables.push_back(std::move(variable));
                    }
                    continue;
                }
                if (key.compare(0, std::strlen(kMissionCheckpointVariablePrefix),
                                kMissionCheckpointVariablePrefix) == 0)
                {
                    MissionVariableSnapshot variable;
                    if (ParseMissionVariable(key, kMissionCheckpointVariablePrefix, value, variable))
                    {
                        checkpointVariables.push_back(std::move(variable));
                    }
                    continue;
                }
                values[key] = value;
            }
            // The map above is unordered; mission variables keep the file's own order so a save
            // written from a load is byte-identical to the one it came from.

            if (values["format"] != "iron-gang-save-v1")
            {
                errorMessage = "Unsupported save format";
                return std::nullopt;
            }

            SaveSnapshot snapshot;
            const auto missionStateIdIt = values.find("mission_state_id");
            if (missionStateIdIt != values.end())
            {
                snapshot.missionStateId = missionStateIdIt->second;
            }
            else
            {
                // Migration from the pre-IG-24-018 format, whose mission_state was an index into a
                // fixed five-state enum. A save from that era can only have been written by the
                // prologue mission, so the mapping is exact rather than a guess.
                const auto legacyIt = values.find("mission_state");
                if (legacyIt == values.end())
                {
                    errorMessage = "Save file has neither mission_state_id nor mission_state";
                    return std::nullopt;
                }
                static constexpr const char* kLegacyMissionStateIds[] = {
                    "introduction", "reach_vehicle", "enter_vehicle", "drive_to_warehouse", "completed"};
                const int legacyState = std::stoi(legacyIt->second);
                if (legacyState < 0 ||
                    static_cast<std::size_t>(legacyState) >= std::size(kLegacyMissionStateIds))
                {
                    errorMessage = "Save file has an out-of-range legacy mission_state: " + legacyIt->second;
                    return std::nullopt;
                }
                snapshot.missionStateId = kLegacyMissionStateIds[legacyState];
            }
            if (!ParseVector(values.at("player_position"), snapshot.playerPosition) ||
                !ParseVector(values.at("vehicle_position"), snapshot.vehiclePosition))
            {
                errorMessage = "Invalid vector in save file";
                return std::nullopt;
            }
            snapshot.playerYaw = std::stof(values.at("player_yaw"));
            snapshot.vehicleYaw = std::stof(values.at("vehicle_yaw"));
            snapshot.vehicleSpeed = std::stof(values.at("vehicle_speed"));
            snapshot.playerDriving = std::stoi(values.at("player_driving")) != 0;
            const auto districtIt = values.find("district_id");
            snapshot.districtId = districtIt != values.end()
                                      ? static_cast<DistrictId>(std::stoi(districtIt->second))
                                      : DistrictId::WarehouseBlock;
            snapshot.missionVariables = std::move(missionVariables);
            const auto checkpointStateIt = values.find("mission_checkpoint_state_id");
            if (checkpointStateIt != values.end() && !checkpointStateIt->second.empty())
            {
                snapshot.missionCheckpoint.stateId = checkpointStateIt->second;
                snapshot.missionCheckpoint.variables = std::move(checkpointVariables);
            }
            return snapshot;
        }
        catch (const std::exception& exception)
        {
            errorMessage = exception.what();
            return std::nullopt;
        }
    }
}
