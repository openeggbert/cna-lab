#pragma once

#include <string>
#include <vector>

namespace IronGang
{
    // plan_24 IG-24-020/021/046: which missions exist, in what order they unlock, and which are
    // done. The campaign is data for the same reason the missions are: a 15-20 mission game whose
    // order lives in a switch statement is a game nobody can reorder.
    inline constexpr int kCampaignFileVersion = 1;
    // A bound on how much a campaign file can describe (plan_36 IG-36-002's reasoning).
    inline constexpr std::size_t kMaxCampaignMissions = 64;

    struct CampaignMission
    {
        std::string id;
        // Path relative to the asset root, e.g. "missions/prologue.mission.json".
        std::string path;
        std::string title;
        // Mission ids that must be completed before this one unlocks. Empty means it is available
        // from the start.
        std::vector<std::string> requires_;
    };

    struct CampaignDefinition
    {
        int version{kCampaignFileVersion};
        std::vector<CampaignMission> missions;

        [[nodiscard]] const CampaignMission* Find(const std::string& missionId) const;
    };

    // Parses and validates a campaign file. Rejected: an unsupported version, a duplicate or empty
    // mission id, an empty path, a prerequisite naming a mission that does not exist, **a
    // dependency cycle**, and a campaign where nothing is available from the start -- each of which
    // produces a campaign that cannot be finished, and none of which a player could diagnose.
    [[nodiscard]] bool LoadCampaignDefinition(const std::string& path,
                                              CampaignDefinition& out,
                                              std::string& errorMessage);

    // Which missions are done, and therefore which are available. Kept separate from the
    // definition because one is content and the other is progress: they have different lifetimes
    // and only one of them goes in a save file.
    class CampaignState final
    {
    public:
        void Reset();
        // Marks a mission complete. Unknown ids are ignored -- a save from an edited campaign must
        // not be able to inject progress for a mission that no longer exists.
        void MarkCompleted(const CampaignDefinition& campaign, const std::string& missionId);
        [[nodiscard]] bool IsCompleted(const std::string& missionId) const;
        // True when every prerequisite is complete and the mission itself is not.
        [[nodiscard]] bool IsAvailable(const CampaignDefinition& campaign,
                                       const std::string& missionId) const;
        // The first available mission in file order, or empty when there is none -- which means
        // either the campaign is finished or its prerequisites are unmet.
        [[nodiscard]] std::string NextAvailable(const CampaignDefinition& campaign) const;
        [[nodiscard]] bool IsFinished(const CampaignDefinition& campaign) const;

        // For the save file, in a stable order.
        [[nodiscard]] const std::vector<std::string>& GetCompleted() const noexcept { return completed_; }
        void SetCompleted(const CampaignDefinition& campaign, const std::vector<std::string>& completed);

    private:
        std::vector<std::string> completed_;
    };
}
