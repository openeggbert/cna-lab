#include "IronGang/Missions/CampaignDefinition.hpp"

#include "../Core/JsonDataFileInternal.hpp"

#include "System/Text/Json/JsonProperty.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    const CampaignMission* CampaignDefinition::Find(const std::string& missionId) const
    {
        for (const CampaignMission& mission : missions)
        {
            if (mission.id == missionId)
            {
                return &mission;
            }
        }
        return nullptr;
    }

    namespace
    {
        std::string GetOptionalString(const JsonElement& element, const char* name)
        {
            JsonElement value;
            if (!element.TryGetProperty(name, value) ||
                value.getValueKindProperty() != JsonValueKind::String)
            {
                return {};
            }
            return value.GetString();
        }

        // Depth-first search with a colour marking, which reports the cycle rather than only its
        // existence -- "prologue requires itself, eventually" is a far more useful error than
        // "cycle detected".
        bool FindCycle(const CampaignDefinition& campaign,
                       const std::string& missionId,
                       std::unordered_map<std::string, int>& state,
                       std::vector<std::string>& path,
                       std::string& cycleDescription)
        {
            state[missionId] = 1; // in progress
            path.push_back(missionId);
            const CampaignMission* mission = campaign.Find(missionId);
            for (const std::string& prerequisite : mission->requires_)
            {
                const int prerequisiteState = state.count(prerequisite) ? state[prerequisite] : 0;
                if (prerequisiteState == 1)
                {
                    const auto start = std::find(path.begin(), path.end(), prerequisite);
                    for (auto entry = start; entry != path.end(); ++entry)
                    {
                        cycleDescription += *entry + " -> ";
                    }
                    cycleDescription += prerequisite;
                    return true;
                }
                if (prerequisiteState == 0 &&
                    FindCycle(campaign, prerequisite, state, path, cycleDescription))
                {
                    return true;
                }
            }
            path.pop_back();
            state[missionId] = 2; // finished
            return false;
        }
    }

    bool LoadCampaignDefinition(const std::string& path, CampaignDefinition& out, std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        CampaignDefinition campaign;
        try
        {
            const JsonElement& root = file.root;
            JsonElement versionElement;
            if (root.TryGetProperty("version", versionElement))
            {
                if (versionElement.getValueKindProperty() != JsonValueKind::Number)
                {
                    errorMessage = "Campaign \"version\" must be a number: " + path;
                    return false;
                }
                campaign.version = static_cast<int>(versionElement.GetInt32());
            }
            if (campaign.version != kCampaignFileVersion)
            {
                errorMessage = "Campaign file has unsupported \"version\" " +
                               std::to_string(campaign.version) + " (expected " +
                               std::to_string(kCampaignFileVersion) + "): " + path;
                return false;
            }

            JsonElement missions;
            if (!root.TryGetProperty("missions", missions) ||
                missions.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "Campaign file is missing a \"missions\" array: " + path;
                return false;
            }

            for (const JsonElement& entry : missions.EnumerateArray())
            {
                if (campaign.missions.size() >= kMaxCampaignMissions)
                {
                    errorMessage = "Campaign file lists more than " +
                                   std::to_string(kMaxCampaignMissions) + " missions: " + path;
                    return false;
                }
                CampaignMission mission;
                mission.id = GetOptionalString(entry, "id");
                mission.path = GetOptionalString(entry, "path");
                mission.title = GetOptionalString(entry, "title");
                if (mission.id.empty() || mission.path.empty())
                {
                    errorMessage = "Every campaign mission needs a non-empty \"id\" and \"path\": " + path;
                    return false;
                }
                // Not named `requires`: that is a keyword in C++20 and this is a C++23 build.
                JsonElement prerequisites;
                if (entry.TryGetProperty("requires", prerequisites))
                {
                    if (prerequisites.getValueKindProperty() != JsonValueKind::Array)
                    {
                        errorMessage = "Campaign mission \"" + mission.id +
                                       "\" has a \"requires\" that is not an array: " + path;
                        return false;
                    }
                    for (const JsonElement& prerequisite : prerequisites.EnumerateArray())
                    {
                        if (prerequisite.getValueKindProperty() != JsonValueKind::String ||
                            prerequisite.GetString().empty())
                        {
                            errorMessage = "Campaign mission \"" + mission.id +
                                           "\" has a prerequisite that is not a mission id: " + path;
                            return false;
                        }
                        mission.requires_.push_back(prerequisite.GetString());
                    }
                }
                campaign.missions.push_back(std::move(mission));
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        if (campaign.missions.empty())
        {
            errorMessage = "Campaign file lists no missions: " + path;
            return false;
        }

        std::unordered_set<std::string> seen;
        for (const CampaignMission& mission : campaign.missions)
        {
            if (!seen.insert(mission.id).second)
            {
                errorMessage = "Campaign file has a duplicate mission id \"" + mission.id + "\": " + path;
                return false;
            }
        }
        for (const CampaignMission& mission : campaign.missions)
        {
            for (const std::string& prerequisite : mission.requires_)
            {
                if (prerequisite == mission.id)
                {
                    errorMessage = "Campaign mission \"" + mission.id + "\" requires itself: " + path;
                    return false;
                }
                if (campaign.Find(prerequisite) == nullptr)
                {
                    errorMessage = "Campaign mission \"" + mission.id + "\" requires \"" + prerequisite +
                                   "\", which is not in this campaign: " + path;
                    return false;
                }
            }
        }

        std::unordered_map<std::string, int> state;
        for (const CampaignMission& mission : campaign.missions)
        {
            if (state.count(mission.id) && state[mission.id] != 0)
            {
                continue;
            }
            std::vector<std::string> visiting;
            std::string cycle;
            if (FindCycle(campaign, mission.id, state, visiting, cycle))
            {
                errorMessage = "Campaign has a dependency cycle (" + cycle + "): " + path;
                return false;
            }
        }

        // A campaign whose every mission has a prerequisite can never start. That is not a runtime
        // surprise worth discovering on a player's machine.
        const bool hasStart = std::any_of(campaign.missions.begin(), campaign.missions.end(),
                                          [](const CampaignMission& mission)
                                          { return mission.requires_.empty(); });
        if (!hasStart)
        {
            errorMessage = "Campaign has no mission available from the start: " + path;
            return false;
        }

        out = std::move(campaign);
        return true;
    }

    void CampaignState::Reset()
    {
        completed_.clear();
    }

    bool CampaignState::IsCompleted(const std::string& missionId) const
    {
        return std::find(completed_.begin(), completed_.end(), missionId) != completed_.end();
    }

    void CampaignState::MarkCompleted(const CampaignDefinition& campaign, const std::string& missionId)
    {
        if (campaign.Find(missionId) == nullptr || IsCompleted(missionId))
        {
            return;
        }
        completed_.push_back(missionId);
    }

    bool CampaignState::IsAvailable(const CampaignDefinition& campaign, const std::string& missionId) const
    {
        const CampaignMission* mission = campaign.Find(missionId);
        if (mission == nullptr || IsCompleted(missionId))
        {
            return false;
        }
        for (const std::string& prerequisite : mission->requires_)
        {
            if (!IsCompleted(prerequisite))
            {
                return false;
            }
        }
        return true;
    }

    std::string CampaignState::NextAvailable(const CampaignDefinition& campaign) const
    {
        for (const CampaignMission& mission : campaign.missions)
        {
            if (IsAvailable(campaign, mission.id))
            {
                return mission.id;
            }
        }
        return {};
    }

    bool CampaignState::IsFinished(const CampaignDefinition& campaign) const
    {
        return std::all_of(campaign.missions.begin(), campaign.missions.end(),
                           [this](const CampaignMission& mission) { return IsCompleted(mission.id); });
    }

    void CampaignState::SetCompleted(const CampaignDefinition& campaign,
                                     const std::vector<std::string>& completed)
    {
        completed_.clear();
        for (const std::string& missionId : completed)
        {
            MarkCompleted(campaign, missionId);
        }
    }
}
