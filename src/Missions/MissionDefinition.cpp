#include "IronGang/Missions/MissionDefinition.hpp"

#include "System/IO/File.hpp"
#include "System/Text/Json/JsonDocument.hpp"

#include <unordered_set>

namespace IronGang
{
    using System::Text::Json::JsonDocument;
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    bool ParseMissionCondition(const std::string& name, MissionCondition& out)
    {
        if (name == "dialogue_finished") { out = MissionCondition::DialogueFinished; return true; }
        if (name == "player_near_vehicle") { out = MissionCondition::PlayerNearVehicle; return true; }
        if (name == "player_driving") { out = MissionCondition::PlayerDriving; return true; }
        if (name == "player_driving_in_warehouse_goal") { out = MissionCondition::PlayerDrivingInWarehouseGoal; return true; }
        return false;
    }

    const MissionStateDefinition* MissionDefinition::FindState(const std::string& stateId) const
    {
        for (const MissionStateDefinition& state : states)
        {
            if (state.id == stateId)
            {
                return &state;
            }
        }
        return nullptr;
    }

    namespace
    {
        // "" (absent/null/empty) is a valid value here -- it means "terminal state, no condition".
        bool ParseOptionalCondition(const JsonElement& stateElement, MissionCondition& out, std::string& errorMessage)
        {
            JsonElement conditionElement;
            if (!stateElement.TryGetProperty("condition", conditionElement))
            {
                out = MissionCondition::None;
                return true;
            }
            const std::string text = conditionElement.GetString();
            if (text.empty())
            {
                out = MissionCondition::None;
                return true;
            }
            if (!ParseMissionCondition(text, out))
            {
                errorMessage = "Unknown mission condition: " + text;
                return false;
            }
            return true;
        }

        std::string GetOptionalString(const JsonElement& element, const char* name)
        {
            JsonElement value;
            if (!element.TryGetProperty(name, value))
            {
                return {};
            }
            return value.GetString();
        }
    }

    bool LoadMissionDefinition(const std::string& path, MissionDefinition& out, std::string& errorMessage)
    {
        if (!System::IO::File::Exists(path))
        {
            errorMessage = "Mission file not found: " + path;
            return false;
        }

        MissionDefinition definition;
        try
        {
            const std::string text = System::IO::File::ReadAllText(path);
            const std::shared_ptr<JsonDocument> document = JsonDocument::Parse(text);
            const JsonElement root = document->getRootElementProperty();
            if (root.getValueKindProperty() != JsonValueKind::Object)
            {
                errorMessage = "Mission file root must be a JSON object: " + path;
                return false;
            }

            definition.id = GetOptionalString(root, "id");
            JsonElement versionElement;
            definition.version = root.TryGetProperty("version", versionElement) ? versionElement.GetInt32() : 1;
            definition.initialState = GetOptionalString(root, "initialState");

            JsonElement statesElement;
            if (!root.TryGetProperty("states", statesElement) ||
                statesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "Mission file is missing a \"states\" array: " + path;
                return false;
            }

            for (const JsonElement& stateElement : statesElement.EnumerateArray())
            {
                MissionStateDefinition state;
                state.id = GetOptionalString(stateElement, "id");
                if (state.id.empty())
                {
                    errorMessage = "Mission file has a state with a missing or empty \"id\": " + path;
                    return false;
                }
                state.objective = GetOptionalString(stateElement, "objective");
                state.next = GetOptionalString(stateElement, "next");
                if (!ParseOptionalCondition(stateElement, state.condition, errorMessage))
                {
                    errorMessage += " (state \"" + state.id + "\" in " + path + ")";
                    return false;
                }
                definition.states.push_back(std::move(state));
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        // Validation (IG-24-003's smallest form: inline checks, not a separate tool): unique ids,
        // initialState and every non-empty `next` must refer to a real state, and a terminal
        // state (empty `next`) must not also claim a condition (it would never be evaluated).
        std::unordered_set<std::string> seenIds;
        for (const MissionStateDefinition& state : definition.states)
        {
            if (!seenIds.insert(state.id).second)
            {
                errorMessage = "Mission file has a duplicate state id \"" + state.id + "\": " + path;
                return false;
            }
        }
        if (definition.states.empty())
        {
            errorMessage = "Mission file defines no states: " + path;
            return false;
        }
        if (definition.FindState(definition.initialState) == nullptr)
        {
            errorMessage = "Mission file's initialState \"" + definition.initialState +
                           "\" does not match any state id: " + path;
            return false;
        }
        for (const MissionStateDefinition& state : definition.states)
        {
            if (!state.next.empty() && definition.FindState(state.next) == nullptr)
            {
                errorMessage = "Mission state \"" + state.id + "\" has a \"next\" (\"" + state.next +
                               "\") that does not match any state id: " + path;
                return false;
            }
        }

        out = std::move(definition);
        return true;
    }
}
