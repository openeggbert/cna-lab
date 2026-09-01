#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"

#include <optional>
#include <string>
#include <vector>

namespace IronGang
{
    // The part of a snapshot that says where everything is: the player, their vehicle, and which
    // district is loaded. Split out so a mission checkpoint can record the same world state without
    // a second declaration of the same seven fields. SaveSnapshot derives from it, so every
    // existing `snapshot.playerPosition` still compiles and `WorldStateSnapshot world = snapshot;`
    // takes exactly the world half of a save.
    struct WorldStateSnapshot
    {
        Vector3 playerPosition{};
        float playerYaw{0.0F};
        Vector3 vehiclePosition{};
        float vehicleYaw{0.0F};
        float vehicleSpeed{0.0F};
        bool playerDriving{false};
        // plan_17 IG-17-015: 1 is undamaged, 0 is wrecked. Absent in older saves, which load with
        // an intact car -- the friendlier default of the two.
        float vehicleIntegrity{1.0F};
        // Added for gate M5 (district loading, plan_13). Older save files without this field
        // load with a WarehouseBlock default rather than failing.
        DistrictId districtId{DistrictId::WarehouseBlock};
    };

    struct SaveSnapshot : WorldStateSnapshot
    {
        // Which mission is being played (plan_24 IG-24-049). Empty in a save written before
        // campaigns existed, which then keeps whatever mission the campaign starts with.
        std::string missionId;
        // Campaign missions already finished, in the order they were completed. Ids the campaign
        // no longer contains are dropped on restore rather than trusted.
        std::vector<std::string> completedMissions;
        // The mission's current state id (plan_24 IG-24-018). Older saves stored a fixed
        // 0-4 int in a "mission_state" field instead; SaveGame::Read migrates those, so a save
        // written before mission states became free-form still loads.
        std::string missionStateId;
        // Added for plan_24 IG-24-005/029: the mission's own typed variables, written as one
        // "mission_var.<name>=<type>:<value>" line each. Older save files simply have none, and a
        // name/type the current mission file no longer declares is reported and skipped on load
        // (see PrototypeMission::ApplyVariables) rather than failing the load.
        std::vector<MissionVariableSnapshot> missionVariables;
        // Added for plan_24 IG-24-044: the mission's last checkpoint, written as
        // "mission_checkpoint_state_id" plus one "mission_checkpoint_var.<name>" line per variable.
        // An empty stateId means no checkpoint was reached, which is what a save from a mission
        // with no checkpoint states -- and every save written before this field existed -- has.
        MissionCheckpointSnapshot missionCheckpoint;
        // Where the player, the vehicle, and the world stood when that checkpoint was recorded
        // (plan_29 IG-29-009/029). Absent when the mission has no checkpoint, and in every save
        // written before this field existed -- a retry then falls back to restarting the mission.
        std::optional<WorldStateSnapshot> missionCheckpointWorld;
    };

    // Save format versions this build understands (plan_29 IG-29-001). Version 1 is the original
    // plain "format=iron-gang-save-v1" file with no integrity check; version 2 adds a checksum
    // line and is what Write() produces. A file claiming a newer version is refused rather than
    // half-read: it was written by a build that knows fields this one does not.
    inline constexpr int kMinSaveFormatVersion = 1;
    inline constexpr int kCurrentSaveFormatVersion = 2;

    // What Read() had to do to produce a snapshot. Not errors -- facts a caller may want to report.
    struct SaveReadDiagnostics
    {
        // Version of the file the snapshot came from. Below kCurrentSaveFormatVersion means it was
        // migrated on read and will be written back in the current format.
        int formatVersion{kCurrentSaveFormatVersion};
        // True when the primary file was unusable and the rolling backup was read instead.
        bool usedBackup{false};
        // Why the primary file was unusable, when usedBackup is true.
        std::string primaryError;
    };

    class SaveGame final
    {
    public:
        // Writes atomically (plan_29 IG-29-002/003): the snapshot goes to TemporaryPath() first,
        // any existing save is rotated to BackupPath(), and only then is the temporary file
        // renamed into place. A failure at any point leaves either the previous save or its
        // backup intact -- never a half-written file at @p path.
        [[nodiscard]] static bool Write(const std::string& path,
                                        const SaveSnapshot& snapshot,
                                        std::string& errorMessage);

        // Reads @p path, verifying the checksum for format version 2 and migrating older versions.
        // If the primary file is missing, corrupt, or unsupported, the rolling backup is read
        // instead and @p diagnostics (when given) records that plus why (IG-29-003/004).
        [[nodiscard]] static std::optional<SaveSnapshot> Read(const std::string& path,
                                                              std::string& errorMessage,
                                                              SaveReadDiagnostics* diagnostics = nullptr);

        // "<path>.bak" and "<path>.tmp" -- the rolling backup and the write-in-progress file.
        [[nodiscard]] static std::string BackupPath(const std::string& path);
        [[nodiscard]] static std::string TemporaryPath(const std::string& path);

        // The most recently written of @p candidates that exists, or an empty string if none do.
        // "Load" means "resume", and the newest save is what resuming should mean whether it was
        // written by hand or by the autosave (plan_29 IG-29-010).
        [[nodiscard]] static std::string ChooseMostRecent(const std::vector<std::string>& candidates);
    };
}
