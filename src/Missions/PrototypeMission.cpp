#include "IronGang/Missions/PrototypeMission.hpp"

#include <cmath>
#include <iostream>

namespace IronGang
{
    namespace
    {
        // The player is "at" the sedan within this many metres. Gate M7 spelled the same test as a
        // squared-distance literal inside the engine; it is now a fact plus a mission-file number,
        // so a mission can widen or tighten it without a code change.
        constexpr float kPlayerNearVehicleMetres = 3.0F;

        constexpr const char* kFactDialogueFinished = "dialogue_finished";
        constexpr const char* kFactPlayerDriving = "player_driving";
        constexpr const char* kFactPlayerVehicleDistance = "player_vehicle_distance";
        constexpr const char* kFactPlayerNearVehicle = "player_near_vehicle";
        constexpr const char* kFactPlayerInWarehouseGoal = "player_in_warehouse_goal";
        constexpr const char* kFactVehicleInWarehouseGoal = "vehicle_in_warehouse_goal";
        constexpr const char* kFactPlayerDrivingInWarehouseGoal = "player_driving_in_warehouse_goal";

        void LogMission(const std::string& message)
        {
            std::cerr << "[IronGang][mission] " << message << "\n";
        }

        // The built-in fallback's own expressions are fixed source text compiled against the fixed
        // fact set, so a failure here is a programming error in this file, not bad content.
        MissionExpression CompileBuiltIn(const char* source, const MissionContext& context)
        {
            MissionExpression expression;
            std::string error;
            if (!MissionExpression::Compile(source, context, expression, error))
            {
                LogMission(std::string("built-in mission condition \"") + source + "\" failed to compile: " +
                           error);
            }
            return expression;
        }

        MissionDefinition BuildFallbackMissionDefinition()
        {
            MissionDefinition definition;
            definition.id = "prototype_delivery";
            definition.title = "The Quiet Delivery";
            definition.version = kMaxMissionFileVersion;
            definition.initialState = "introduction";
            definition.declaredContext = CreatePrototypeMissionFacts();

            const MissionContext& context = definition.declaredContext;
            MissionStateDefinition introduction;
            introduction.id = "introduction";
            introduction.objective = "Listen to Mara (Enter advances dialogue)";
            introduction.condition = CompileBuiltIn("dialogue_finished", context);
            introduction.next = "reach_vehicle";

            MissionStateDefinition reachVehicle;
            reachVehicle.id = "reach_vehicle";
            reachVehicle.objective = "Walk to the sedan";
            reachVehicle.condition = CompileBuiltIn("player_vehicle_distance <= 3", context);
            reachVehicle.next = "enter_vehicle";

            MissionStateDefinition enterVehicle;
            enterVehicle.id = "enter_vehicle";
            enterVehicle.objective = "Press E to enter the sedan";
            enterVehicle.condition = CompileBuiltIn("player_driving", context);
            enterVehicle.next = "drive_to_warehouse";

            MissionStateDefinition driveToWarehouse;
            driveToWarehouse.id = "drive_to_warehouse";
            driveToWarehouse.objective = "Drive into the green warehouse marker";
            driveToWarehouse.condition = CompileBuiltIn("player_driving && vehicle_in_warehouse_goal", context);
            driveToWarehouse.next = "completed";

            MissionStateDefinition completed;
            completed.id = "completed";
            completed.objective = "Prototype mission complete";
            completed.outcome = MissionOutcome::Completed;

            definition.states.push_back(std::move(introduction));
            definition.states.push_back(std::move(reachVehicle));
            definition.states.push_back(std::move(enterVehicle));
            definition.states.push_back(std::move(driveToWarehouse));
            definition.states.push_back(std::move(completed));
            return definition;
        }

    }

    MissionContext CreatePrototypeMissionFacts()
    {
        MissionContext context;
        std::string error;
        const bool declared =
            context.DeclareFact(kFactDialogueFinished, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPlayerDriving, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPlayerVehicleDistance, MissionValue::Float(0.0F), error) &&
            context.DeclareFact(kFactPlayerNearVehicle, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPlayerInWarehouseGoal, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactVehicleInWarehouseGoal, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPlayerDrivingInWarehouseGoal, MissionValue::Bool(false), error);
        if (!declared)
        {
            LogMission("failed to declare the prototype fact set: " + error);
        }
        return context;
    }

    PrototypeMission::PrototypeMission() : definition_(BuildFallbackMissionDefinition())
    {
        context_ = definition_.declaredContext;
        stateId_ = definition_.initialState;
    }

    bool PrototypeMission::LoadMission(const std::string& path, std::string& errorMessage)
    {
        MissionDefinition loaded;
        if (!LoadMissionDefinition(path, CreatePrototypeMissionFacts(), loaded, errorMessage))
        {
            return false;
        }

        definition_ = std::move(loaded);
        context_ = definition_.declaredContext;
        stateId_ = definition_.initialState;
        return true;
    }

    void PrototypeMission::Reset()
    {
        context_.ResetVariables();
        stateId_ = definition_.initialState;
        EnterState(stateId_);
    }

    bool PrototypeMission::SetStateId(const std::string& stateId)
    {
        if (definition_.FindState(stateId) == nullptr)
        {
            return false;
        }
        stateId_ = stateId;
        conditionFaultLogged_ = false;
        return true;
    }

    MissionOutcome PrototypeMission::GetOutcome() const
    {
        return definition_.GetOutcome(stateId_);
    }

    void PrototypeMission::EnterState(const std::string& stateId)
    {
        conditionFaultLogged_ = false;
        const MissionStateDefinition* definition = definition_.FindState(stateId);
        if (definition == nullptr)
        {
            return;
        }

        for (const MissionAction& action : definition->onEnter)
        {
            std::string error;
            switch (action.kind)
            {
                case MissionAction::Kind::Set:
                {
                    MissionValue value;
                    if (!action.value.Evaluate(context_, value, error) ||
                        !context_.SetVariable(action.variable, value, error))
                    {
                        // Both the expression's type and the variable's existence were checked at
                        // load time, so this only reports a genuine runtime fault (divide by zero,
                        // step limit) -- the mission keeps running with the previous value.
                        LogMission("state \"" + definition->id + "\" could not set \"" + action.variable +
                                   "\": " + error);
                        break;
                    }
                    break;
                }
                case MissionAction::Kind::Log:
                    LogMission(definition_.id + ": " + action.message);
                    break;
            }
        }
    }

    void PrototypeMission::RefreshFacts(bool dialogueFinished,
                                        const Vector3& playerPosition,
                                        const Vector3& vehiclePosition,
                                        bool playerDriving,
                                        const TriggerZone& warehouseGoal)
    {
        const float distance = std::sqrt(DistanceSquaredXZ(playerPosition, vehiclePosition));
        const bool vehicleInGoal = warehouseGoal.bounds.ContainsXZ(vehiclePosition);
        const bool playerInGoal = warehouseGoal.bounds.ContainsXZ(playerPosition);

        std::string error;
        const bool applied =
            context_.SetFact(kFactDialogueFinished, MissionValue::Bool(dialogueFinished), error) &&
            context_.SetFact(kFactPlayerDriving, MissionValue::Bool(playerDriving), error) &&
            context_.SetFact(kFactPlayerVehicleDistance, MissionValue::Float(distance), error) &&
            context_.SetFact(kFactPlayerNearVehicle,
                             MissionValue::Bool(distance <= kPlayerNearVehicleMetres), error) &&
            context_.SetFact(kFactPlayerInWarehouseGoal, MissionValue::Bool(playerInGoal), error) &&
            context_.SetFact(kFactVehicleInWarehouseGoal, MissionValue::Bool(vehicleInGoal), error) &&
            context_.SetFact(kFactPlayerDrivingInWarehouseGoal,
                             MissionValue::Bool(playerDriving && vehicleInGoal), error);
        if (!applied)
        {
            LogMission("could not refresh mission facts: " + error);
        }
    }

    void PrototypeMission::Update(bool dialogueFinished,
                                  const Vector3& playerPosition,
                                  const Vector3& vehiclePosition,
                                  bool playerDriving,
                                  const TriggerZone& warehouseGoal)
    {
        RefreshFacts(dialogueFinished, playerPosition, vehiclePosition, playerDriving, warehouseGoal);

        const MissionStateDefinition* current = definition_.FindState(stateId_);
        if (current == nullptr || current->condition.IsEmpty() || current->next.empty())
        {
            return;
        }

        bool conditionMet = false;
        std::string error;
        if (!current->condition.EvaluateBool(context_, conditionMet, error))
        {
            // A condition that cannot be evaluated must not silently behave like "false forever":
            // report it and leave the mission where it is. Once per state entry, not once per
            // frame -- a faulting condition faults on every one of them.
            if (!conditionFaultLogged_)
            {
                conditionFaultLogged_ = true;
                LogMission("state \"" + current->id + "\" condition failed: " + error);
            }
            return;
        }
        if (!conditionMet)
        {
            return;
        }

        // IG-24-016: every transition goes through the game's existing logging path, naming the
        // condition that fired, so a mission that advances unexpectedly can be traced from a log.
        const std::string nextStateId = current->next;
        LogMission(definition_.id + ": " + current->id + " -> " + nextStateId + " (" +
                   current->condition.GetSource() + ")");
        stateId_ = nextStateId;
        EnterState(stateId_);
    }

    std::string PrototypeMission::GetObjectiveText() const
    {
        const MissionStateDefinition* current = definition_.FindState(stateId_);
        return current != nullptr ? current->objective : "Unknown objective";
    }

    std::vector<MissionVariableSnapshot> PrototypeMission::CaptureVariables() const
    {
        return context_.CaptureVariables();
    }

    void PrototypeMission::ApplyVariables(const std::vector<MissionVariableSnapshot>& variables,
                                          std::vector<std::string>* warnings)
    {
        for (const MissionVariableSnapshot& variable : variables)
        {
            std::string error;
            if (!context_.SetVariable(variable.name, variable.value, error) && warnings != nullptr)
            {
                warnings->push_back(error);
            }
        }
    }

    bool PrototypeMission::TryGetVariable(const std::string& name, MissionValue& out) const
    {
        return context_.IsVariable(name) && context_.TryGetValue(name, out);
    }
}
