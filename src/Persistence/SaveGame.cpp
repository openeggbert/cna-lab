#include "IronShadows/Persistence/SaveGame.hpp"

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"

#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace IronShadows
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
            text << "format=iron-shadows-save-v1\n";
            text << "mission_state=" << static_cast<int>(snapshot.missionState) << "\n";
            text << "player_position=" << VectorToText(snapshot.playerPosition) << "\n";
            text << "player_yaw=" << snapshot.playerYaw << "\n";
            text << "vehicle_position=" << VectorToText(snapshot.vehiclePosition) << "\n";
            text << "vehicle_yaw=" << snapshot.vehicleYaw << "\n";
            text << "vehicle_speed=" << snapshot.vehicleSpeed << "\n";
            text << "player_driving=" << (snapshot.playerDriving ? 1 : 0) << "\n";
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
            for (const std::string& line : System::IO::File::ReadAllLines(path))
            {
                const std::size_t separator = line.find('=');
                if (separator != std::string::npos)
                {
                    values[line.substr(0, separator)] = line.substr(separator + 1);
                }
            }

            if (values["format"] != "iron-shadows-save-v1")
            {
                errorMessage = "Unsupported save format";
                return std::nullopt;
            }

            SaveSnapshot snapshot;
            snapshot.missionState = static_cast<PrototypeMissionState>(std::stoi(values.at("mission_state")));
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
            return snapshot;
        }
        catch (const std::exception& exception)
        {
            errorMessage = exception.what();
            return std::nullopt;
        }
    }
}
