#include "IronGang/Persistence/SaveGame.hpp"

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>
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

        // FNV-1a, 64-bit, over the save's body. This detects a truncated, torn, or bit-rotted
        // file -- the failures a save actually suffers. It is deliberately not a cryptographic
        // hash and not tamper protection: anyone editing a save by hand can recompute it, and this
        // is a single-player prototype where that is fine.
        std::string ChecksumText(std::string_view body)
        {
            std::uint64_t hash = 1469598103934665603ULL;
            for (const char character : body)
            {
                hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
                hash *= 1099511628211ULL;
            }
            std::ostringstream output;
            output << std::hex << std::setw(16) << std::setfill('0') << hash;
            return output.str();
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
            text << "mission_state_id=" << snapshot.missionStateId << "\n";
            text << "player_position=" << VectorToText(snapshot.playerPosition) << "\n";
            text << "player_yaw=" << snapshot.playerYaw << "\n";
            text << "vehicle_position=" << VectorToText(snapshot.vehiclePosition) << "\n";
            text << "vehicle_yaw=" << snapshot.vehicleYaw << "\n";
            text << "vehicle_speed=" << snapshot.vehicleSpeed << "\n";
            text << "player_driving=" << (snapshot.playerDriving ? 1 : 0) << "\n";
            text << "vehicle_integrity=" << snapshot.vehicleIntegrity << "\n";
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
            if (snapshot.missionCheckpointWorld.has_value())
            {
                const WorldStateSnapshot& checkpoint = *snapshot.missionCheckpointWorld;
                text << "checkpoint_player_position=" << VectorToText(checkpoint.playerPosition) << "\n";
                text << "checkpoint_player_yaw=" << checkpoint.playerYaw << "\n";
                text << "checkpoint_vehicle_position=" << VectorToText(checkpoint.vehiclePosition) << "\n";
                text << "checkpoint_vehicle_yaw=" << checkpoint.vehicleYaw << "\n";
                text << "checkpoint_vehicle_speed=" << checkpoint.vehicleSpeed << "\n";
                text << "checkpoint_player_driving=" << (checkpoint.playerDriving ? 1 : 0) << "\n";
                text << "checkpoint_district_id=" << static_cast<int>(checkpoint.districtId) << "\n";
            }

            const std::string body = text.str();
            std::ostringstream document;
            document << "format=iron-gang-save-v" << kCurrentSaveFormatVersion << "\n";
            document << "checksum=" << ChecksumText(body) << "\n";
            document << body;

            // Atomic replace (IG-29-002): the new save lands in a temporary file first, so a crash
            // or a full disk during the write cannot leave a half-written file where the save
            // belongs. Only once that file is complete is the previous save rotated to the backup
            // (IG-29-003) and the temporary renamed into place -- both operations that are atomic
            // within one directory.
            const std::filesystem::path temporaryPath(TemporaryPath(path));
            System::IO::File::WriteAllText(temporaryPath.string(), document.str());
            std::error_code renameError;
            if (std::filesystem::exists(filesystemPath))
            {
                std::filesystem::rename(filesystemPath, std::filesystem::path(BackupPath(path)), renameError);
                if (renameError)
                {
                    std::filesystem::remove(temporaryPath, renameError);
                    errorMessage = "Could not rotate the previous save to a backup: " + renameError.message();
                    return false;
                }
            }
            std::filesystem::rename(temporaryPath, filesystemPath, renameError);
            if (renameError)
            {
                // The previous save is in the backup and the new one in the temporary file, so
                // nothing was lost -- but this path did not end up holding a save.
                errorMessage = "Could not move the new save into place: " + renameError.message();
                return false;
            }
            return true;
        }
        catch (const std::exception& exception)
        {
            std::error_code ignored;
            std::filesystem::remove(std::filesystem::path(TemporaryPath(path)), ignored);
            errorMessage = exception.what();
            return false;
        }
    }

    std::string SaveGame::BackupPath(const std::string& path)
    {
        return path + ".bak";
    }

    std::string SaveGame::TemporaryPath(const std::string& path)
    {
        return path + ".tmp";
    }

    std::string SaveGame::ChooseMostRecent(const std::vector<std::string>& candidates)
    {
        std::string best;
        std::filesystem::file_time_type bestTime{};
        for (const std::string& candidate : candidates)
        {
            std::error_code error;
            const std::filesystem::file_time_type time =
                std::filesystem::last_write_time(std::filesystem::path(candidate), error);
            if (error)
            {
                continue; // missing or unreadable -- not a candidate
            }
            if (best.empty() || time > bestTime)
            {
                best = candidate;
                bestTime = time;
            }
        }
        return best;
    }

    namespace
    {
        // Splits the document into its header lines and the body the checksum covers. Version 1
        // files have no checksum line, so their body is the whole document -- harmless, since the
        // reader only looks at key=value lines and "format" is one of them.
        bool SplitDocument(const std::string& document,
                           int& formatVersion,
                           std::string_view& body,
                           std::string& errorMessage)
        {
            const std::size_t firstLineEnd = document.find('\n');
            if (firstLineEnd == std::string::npos)
            {
                errorMessage = "Save file is empty or truncated";
                return false;
            }
            const std::string firstLine = document.substr(0, firstLineEnd);
            static constexpr std::string_view kFormatPrefix = "format=iron-gang-save-v";
            if (firstLine.compare(0, kFormatPrefix.size(), kFormatPrefix) != 0)
            {
                errorMessage = "Unsupported save format: " + firstLine;
                return false;
            }
            const std::string versionText = firstLine.substr(kFormatPrefix.size());
            try
            {
                formatVersion = std::stoi(versionText);
            }
            catch (const std::exception&)
            {
                errorMessage = "Save file has a malformed format version: " + firstLine;
                return false;
            }
            if (formatVersion < kMinSaveFormatVersion)
            {
                errorMessage = "Save file uses format version " + std::to_string(formatVersion) +
                               ", which this build no longer reads";
                return false;
            }
            if (formatVersion > kCurrentSaveFormatVersion)
            {
                errorMessage = "Save file was written by a newer version of Iron Gang (format v" +
                               std::to_string(formatVersion) + "; this build understands up to v" +
                               std::to_string(kCurrentSaveFormatVersion) + ")";
                return false;
            }

            if (formatVersion < 2)
            {
                body = std::string_view(document);
                return true;
            }

            const std::size_t secondLineEnd = document.find('\n', firstLineEnd + 1);
            if (secondLineEnd == std::string::npos)
            {
                errorMessage = "Save file is truncated: no checksum line";
                return false;
            }
            const std::string checksumLine = document.substr(firstLineEnd + 1, secondLineEnd - firstLineEnd - 1);
            static constexpr std::string_view kChecksumPrefix = "checksum=";
            if (checksumLine.compare(0, kChecksumPrefix.size(), kChecksumPrefix) != 0)
            {
                errorMessage = "Save file is missing its checksum line";
                return false;
            }
            body = std::string_view(document).substr(secondLineEnd + 1);
            const std::string expected = checksumLine.substr(kChecksumPrefix.size());
            const std::string actual = ChecksumText(body);
            if (expected != actual)
            {
                // IG-29-004: this is what a truncated, torn, or edited save looks like. Refusing it
                // is the point -- a partially applied save is worse than no save.
                errorMessage = "Save file is corrupt: checksum " + expected + " does not match " + actual;
                return false;
            }
            return true;
        }

        [[nodiscard]] bool RequireValue(const std::unordered_map<std::string, std::string>& values,
                                        const char* key,
                                        std::string& out,
                                        std::string& errorMessage)
        {
            const auto found = values.find(key);
            if (found == values.end())
            {
                errorMessage = std::string("Save file is missing \"") + key + "\"";
                return false;
            }
            out = found->second;
            return true;
        }

        std::optional<SaveSnapshot> ReadOne(const std::string& path,
                                            std::string& errorMessage,
                                            int& formatVersion)
        {
            if (!System::IO::File::Exists(path))
            {
                errorMessage = "Save file does not exist: " + path;
                return std::nullopt;
            }

            try
            {
                const std::string document = System::IO::File::ReadAllText(path);
                std::string_view body;
                if (!SplitDocument(document, formatVersion, body, errorMessage))
                {
                    return std::nullopt;
                }

                std::unordered_map<std::string, std::string> values;
                std::vector<MissionVariableSnapshot> missionVariables;
                std::vector<MissionVariableSnapshot> checkpointVariables;
                std::size_t lineStart = 0;
                while (lineStart < body.size())
                {
                    const std::size_t lineEnd = std::min(body.find('\n', lineStart), body.size());
                    const std::string line(body.substr(lineStart, lineEnd - lineStart));
                    lineStart = lineEnd + 1;

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

                SaveSnapshot snapshot;
                const auto missionStateIdIt = values.find("mission_state_id");
                if (missionStateIdIt != values.end())
                {
                    snapshot.missionStateId = missionStateIdIt->second;
                }
                else
                {
                    // Migration from the pre-IG-24-018 format, whose mission_state was an index into
                    // a fixed five-state enum. A save from that era can only have been written by
                    // the prologue mission, so the mapping is exact rather than a guess.
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

                std::string playerPosition;
                std::string vehiclePosition;
                std::string playerYaw;
                std::string vehicleYaw;
                std::string vehicleSpeed;
                std::string playerDriving;
                if (!RequireValue(values, "player_position", playerPosition, errorMessage) ||
                    !RequireValue(values, "vehicle_position", vehiclePosition, errorMessage) ||
                    !RequireValue(values, "player_yaw", playerYaw, errorMessage) ||
                    !RequireValue(values, "vehicle_yaw", vehicleYaw, errorMessage) ||
                    !RequireValue(values, "vehicle_speed", vehicleSpeed, errorMessage) ||
                    !RequireValue(values, "player_driving", playerDriving, errorMessage))
                {
                    return std::nullopt;
                }
                if (!ParseVector(playerPosition, snapshot.playerPosition) ||
                    !ParseVector(vehiclePosition, snapshot.vehiclePosition))
                {
                    errorMessage = "Invalid vector in save file";
                    return std::nullopt;
                }
                snapshot.playerYaw = std::stof(playerYaw);
                snapshot.vehicleYaw = std::stof(vehicleYaw);
                snapshot.vehicleSpeed = std::stof(vehicleSpeed);
                snapshot.playerDriving = std::stoi(playerDriving) != 0;
                const auto integrityIt = values.find("vehicle_integrity");
                if (integrityIt != values.end())
                {
                    snapshot.vehicleIntegrity = std::stof(integrityIt->second);
                }
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

                // The world half of the checkpoint is all-or-nothing: a partial one would put the
                // player somewhere and the vehicle nowhere, so anything missing or malformed drops
                // it and leaves a retry to restart the mission instead (plan_29 IG-29-029).
                std::string checkpointPlayerPosition;
                std::string checkpointVehiclePosition;
                std::string checkpointPlayerYaw;
                std::string checkpointVehicleYaw;
                std::string checkpointVehicleSpeed;
                std::string checkpointPlayerDriving;
                std::string dropped;
                if (RequireValue(values, "checkpoint_player_position", checkpointPlayerPosition, dropped) &&
                    RequireValue(values, "checkpoint_vehicle_position", checkpointVehiclePosition, dropped) &&
                    RequireValue(values, "checkpoint_player_yaw", checkpointPlayerYaw, dropped) &&
                    RequireValue(values, "checkpoint_vehicle_yaw", checkpointVehicleYaw, dropped) &&
                    RequireValue(values, "checkpoint_vehicle_speed", checkpointVehicleSpeed, dropped) &&
                    RequireValue(values, "checkpoint_player_driving", checkpointPlayerDriving, dropped))
                {
                    WorldStateSnapshot checkpointWorld;
                    if (ParseVector(checkpointPlayerPosition, checkpointWorld.playerPosition) &&
                        ParseVector(checkpointVehiclePosition, checkpointWorld.vehiclePosition))
                    {
                        checkpointWorld.playerYaw = std::stof(checkpointPlayerYaw);
                        checkpointWorld.vehicleYaw = std::stof(checkpointVehicleYaw);
                        checkpointWorld.vehicleSpeed = std::stof(checkpointVehicleSpeed);
                        checkpointWorld.playerDriving = std::stoi(checkpointPlayerDriving) != 0;
                        const auto checkpointDistrictIt = values.find("checkpoint_district_id");
                        checkpointWorld.districtId =
                            checkpointDistrictIt != values.end()
                                ? static_cast<DistrictId>(std::stoi(checkpointDistrictIt->second))
                                : snapshot.districtId;
                        snapshot.missionCheckpointWorld = checkpointWorld;
                    }
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

    std::optional<SaveSnapshot> SaveGame::Read(const std::string& path,
                                               std::string& errorMessage,
                                               SaveReadDiagnostics* diagnostics)
    {
        SaveReadDiagnostics localDiagnostics;
        std::optional<SaveSnapshot> snapshot = ReadOne(path, errorMessage, localDiagnostics.formatVersion);
        if (!snapshot)
        {
            // IG-29-003: the rolling backup is the previous save, so falling back to it costs at
            // most the progress since that save -- far better than refusing to load at all.
            const std::string primaryError = errorMessage;
            std::string backupError;
            int backupVersion = kCurrentSaveFormatVersion;
            snapshot = ReadOne(BackupPath(path), backupError, backupVersion);
            if (!snapshot)
            {
                errorMessage = primaryError;
                if (diagnostics != nullptr)
                {
                    *diagnostics = localDiagnostics;
                }
                return std::nullopt;
            }
            localDiagnostics.formatVersion = backupVersion;
            localDiagnostics.usedBackup = true;
            localDiagnostics.primaryError = primaryError;
            errorMessage.clear();
        }
        if (diagnostics != nullptr)
        {
            *diagnostics = localDiagnostics;
        }
        return snapshot;
    }
}
