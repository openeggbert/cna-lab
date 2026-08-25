#include "IronGang/Missions/PrototypeMission.hpp"

#include "IronGang/Core/Log.hpp"

#include <cmath>

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
        constexpr const char* kFactPoliceAlerted = "police_alerted";
        constexpr const char* kFactPoliceChasing = "police_chasing";
        constexpr const char* kFactPoliceChaseSeconds = "police_chase_seconds";

        void LogMission(const std::string& message)
        {
            Log::Info(LogCategory::Mission, message);
        }

        void LogMissionFault(const std::string& message)
        {
            Log::Error(LogCategory::Mission, message);
        }

        // The built-in fallback's own expressions are fixed source text compiled against the fixed
        // fact set, so a failure here is a programming error in this file, not bad content.
        MissionExpression CompileBuiltIn(const char* source, const MissionContext& context)
        {
            MissionExpression expression;
            std::string error;
            if (!MissionExpression::Compile(source, context, expression, error))
            {
                LogMissionFault(std::string("built-in mission condition \"") + source + "\" failed to compile: " +
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
            introduction.transitions.push_back(
                {CompileBuiltIn("dialogue_finished", context), "reach_vehicle"});

            MissionStateDefinition reachVehicle;
            reachVehicle.id = "reach_vehicle";
            reachVehicle.objective = "Walk to the sedan";
            reachVehicle.transitions.push_back(
                {CompileBuiltIn("player_vehicle_distance <= 3", context), "enter_vehicle"});

            MissionStateDefinition enterVehicle;
            enterVehicle.id = "enter_vehicle";
            enterVehicle.objective = "Press E to enter the sedan";
            enterVehicle.transitions.push_back(
                {CompileBuiltIn("player_driving", context), "drive_to_warehouse"});

            MissionStateDefinition driveToWarehouse;
            driveToWarehouse.id = "drive_to_warehouse";
            driveToWarehouse.objective = "Drive into the green warehouse marker";
            driveToWarehouse.transitions.push_back(
                {CompileBuiltIn("player_driving && vehicle_in_warehouse_goal", context), "completed"});

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
            context.DeclareFact(kFactPlayerDrivingInWarehouseGoal, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPoliceAlerted, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPoliceChasing, MissionValue::Bool(false), error) &&
            context.DeclareFact(kFactPoliceChaseSeconds, MissionValue::Float(0.0F), error);
        if (!declared)
        {
            LogMissionFault("failed to declare the prototype fact set: " + error);
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
        checkpoint_ = {};
        stateId_ = definition_.initialState;
        EnterState(stateId_);
    }

    void PrototypeMission::Retry()
    {
        const bool restoresCheckpoint =
            definition_.retryPolicy == MissionRetryPolicy::Checkpoint && HasCheckpoint();
        if (!restoresCheckpoint)
        {
            LogMission(definition_.id + ": retry from the beginning (" +
                       MissionRetryPolicyName(definition_.retryPolicy) +
                       (HasCheckpoint() ? " policy)" : " policy, no checkpoint reached yet)"));
            Reset();
            return;
        }

        // Restore the recorded variables on top of their declared values, so a variable that did
        // not exist when the checkpoint was taken still starts from a defined value.
        context_.ResetVariables();
        for (const MissionVariableSnapshot& variable : checkpoint_.variables)
        {
            std::string error;
            if (!context_.SetVariable(variable.name, variable.value, error))
            {
                Log::Warning(LogCategory::Mission, "checkpoint variable ignored: " + error);
            }
        }
        stateId_ = checkpoint_.stateId;
        conditionFaultLogged_ = false;
        // Entry actions are deliberately not re-run: the checkpoint was recorded after they ran,
        // so their effects are already in the variables just restored.
        LogMission(definition_.id + ": retry from checkpoint \"" + stateId_ + "\"");
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
                        LogMissionFault("state \"" + definition->id + "\" could not set \"" + action.variable +
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

        // Recorded after the entry actions have run, so a retry that restores it does not need to
        // re-run them (IG-24-010/042).
        if (definition->checkpoint)
        {
            checkpoint_.stateId = definition->id;
            checkpoint_.variables = context_.CaptureVariables();
            LogMission(definition_.id + ": checkpoint \"" + definition->id + "\"");
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
            LogMissionFault("could not refresh mission facts: " + error);
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
        if (current == nullptr || current->transitions.empty())
        {
            return;
        }

        // File order decides: the first transition whose condition holds this frame wins, so a
        // mission puts its failure branch above its success branch when both could be true
        // (IG-24-024).
        for (const MissionTransition& transition : current->transitions)
        {
            bool conditionMet = false;
            std::string error;
            if (!transition.condition.EvaluateBool(context_, conditionMet, error))
            {
                // A condition that cannot be evaluated must not silently behave like "false
                // forever": report it and leave the mission where it is. Once per state entry, not
                // once per frame -- a faulting condition faults on every one of them.
                if (!conditionFaultLogged_)
                {
                    conditionFaultLogged_ = true;
                    LogMissionFault("state \"" + current->id + "\" condition failed: " + error);
                }
                return;
            }
            if (!conditionMet)
            {
                continue;
            }

            // IG-24-016: every transition goes through the game's existing logging path, naming
            // the condition that fired, so a mission that advances unexpectedly can be traced
            // from a log.
            const std::string nextStateId = transition.next;
            LogMission(definition_.id + ": " + current->id + " -> " + nextStateId + " (" +
                       transition.condition.GetSource() + ")");
            stateId_ = nextStateId;
            EnterState(stateId_);
            return;
        }
    }

    std::string PrototypeMission::GetObjectiveText() const
    {
        const MissionStateDefinition* current = definition_.FindState(stateId_);
        return current != nullptr ? current->objective : "Unknown objective";
    }

    std::string PrototypeMission::GetFailureReason() const
    {
        const MissionStateDefinition* current = definition_.FindState(stateId_);
        if (current == nullptr || current->outcome != MissionOutcome::Failed)
        {
            return {};
        }
        return current->reason;
    }

    void PrototypeMission::ApplyCheckpoint(const MissionCheckpointSnapshot& checkpoint,
                                           std::vector<std::string>* warnings)
    {
        if (checkpoint.stateId.empty())
        {
            checkpoint_ = {};
            return;
        }
        if (definition_.FindState(checkpoint.stateId) == nullptr)
        {
            // A checkpoint into a state this mission no longer defines would send a retry nowhere,
            // so drop the whole checkpoint and fall back to a restart (IG-24-019).
            checkpoint_ = {};
            if (warnings != nullptr)
            {
                warnings->push_back("Checkpoint state \"" + checkpoint.stateId +
                                    "\" is not defined by the loaded mission");
            }
            return;
        }

        MissionCheckpointSnapshot accepted;
        accepted.stateId = checkpoint.stateId;
        for (const MissionVariableSnapshot& variable : checkpoint.variables)
        {
            MissionValueType declaredType{};
            if (!context_.IsVariable(variable.name) ||
                !context_.TryGetType(variable.name, declaredType) ||
                declaredType != variable.value.GetType())
            {
                if (warnings != nullptr)
                {
                    warnings->push_back("Checkpoint variable \"" + variable.name +
                                        "\" no longer matches this mission's declaration");
                }
                continue;
            }
            accepted.variables.push_back(variable);
        }
        checkpoint_ = std::move(accepted);
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

    bool PrototypeMission::SetFact(const std::string& name, const MissionValue& value,
                                   std::string& errorMessage)
    {
        return context_.SetFact(name, value, errorMessage);
    }

    bool PrototypeMission::TryGetVariable(const std::string& name, MissionValue& out) const
    {
        return context_.IsVariable(name) && context_.TryGetValue(name, out);
    }
}
