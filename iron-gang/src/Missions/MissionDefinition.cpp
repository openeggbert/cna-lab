#include "IronGang/Missions/MissionDefinition.hpp"

#include "../Core/JsonDataFileInternal.hpp"

#include "System/IO/File.hpp"

#include <unordered_set>

namespace IronGang
{
    using System::Text::Json::JsonDocument;
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    const char* MissionOutcomeName(MissionOutcome outcome) noexcept
    {
        switch (outcome)
        {
            case MissionOutcome::None: return "none";
            case MissionOutcome::Completed: return "completed";
            case MissionOutcome::Failed: return "failed";
        }
        return "none";
    }

    bool ParseMissionOutcome(const std::string& name, MissionOutcome& out)
    {
        if (name == "none") { out = MissionOutcome::None; return true; }
        if (name == "completed") { out = MissionOutcome::Completed; return true; }
        if (name == "failed") { out = MissionOutcome::Failed; return true; }
        return false;
    }

    const char* MissionRetryPolicyName(MissionRetryPolicy policy) noexcept
    {
        switch (policy)
        {
            case MissionRetryPolicy::Checkpoint: return "checkpoint";
            case MissionRetryPolicy::MissionStart: return "mission_start";
        }
        return "checkpoint";
    }

    bool ParseMissionRetryPolicy(const std::string& name, MissionRetryPolicy& out)
    {
        if (name == "checkpoint") { out = MissionRetryPolicy::Checkpoint; return true; }
        if (name == "mission_start") { out = MissionRetryPolicy::MissionStart; return true; }
        return false;
    }

    MissionOutcome MissionDefinition::GetOutcome(const std::string& stateId) const
    {
        const MissionStateDefinition* state = FindState(stateId);
        return state != nullptr ? state->outcome : MissionOutcome::None;
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
        std::string GetOptionalString(const JsonElement& element, const char* name)
        {
            JsonElement value;
            if (!element.TryGetProperty(name, value))
            {
                return {};
            }
            return value.GetString();
        }

        bool TryGetArray(const JsonElement& element, const char* name, JsonElement& out, bool& present)
        {
            present = element.TryGetProperty(name, out);
            if (!present)
            {
                return true;
            }
            return out.getValueKindProperty() == JsonValueKind::Array;
        }

        // Reads a declared variable's initial value in the type the file itself declared, so a
        // mismatch ("type": "int" with "value": "hello") is a load error rather than a silent
        // coercion that would then fail every expression compiled against it.
        bool ParseDeclaredValue(const JsonElement& variableElement,
                                MissionValueType type,
                                const std::string& variableId,
                                MissionValue& out,
                                std::string& errorMessage)
        {
            JsonElement valueElement;
            if (!variableElement.TryGetProperty("value", valueElement))
            {
                switch (type)
                {
                    case MissionValueType::Bool: out = MissionValue::Bool(false); break;
                    case MissionValueType::Int: out = MissionValue::Int(0); break;
                    case MissionValueType::Float: out = MissionValue::Float(0.0F); break;
                    case MissionValueType::String: out = MissionValue::String({}); break;
                }
                return true;
            }

            const JsonValueKind kind = valueElement.getValueKindProperty();
            switch (type)
            {
                case MissionValueType::Bool:
                    if (kind != JsonValueKind::True && kind != JsonValueKind::False)
                    {
                        break;
                    }
                    out = MissionValue::Bool(valueElement.GetBoolean());
                    return true;
                case MissionValueType::Int:
                    if (kind != JsonValueKind::Number)
                    {
                        break;
                    }
                    out = MissionValue::Int(static_cast<int>(valueElement.GetInt32()));
                    return true;
                case MissionValueType::Float:
                    if (kind != JsonValueKind::Number)
                    {
                        break;
                    }
                    out = MissionValue::Float(static_cast<float>(valueElement.GetDouble()));
                    return true;
                case MissionValueType::String:
                    if (kind != JsonValueKind::String)
                    {
                        break;
                    }
                    out = MissionValue::String(valueElement.GetString());
                    return true;
            }

            errorMessage = "Mission variable \"" + variableId + "\" is declared " +
                           MissionValueTypeName(type) + " but its \"value\" is not a " +
                           MissionValueTypeName(type) + " literal";
            return false;
        }

        bool ParseVariables(const JsonElement& root,
                            int version,
                            MissionContext& context,
                            std::string& errorMessage)
        {
            bool present = false;
            JsonElement variablesElement;
            if (!TryGetArray(root, "variables", variablesElement, present))
            {
                errorMessage = "Mission file's \"variables\" must be an array";
                return false;
            }
            if (!present)
            {
                return true;
            }
            if (version < 2)
            {
                errorMessage = "Mission file declares \"variables\", which requires \"version\": 2";
                return false;
            }

            for (const JsonElement& variableElement : variablesElement.EnumerateArray())
            {
                const std::string id = GetOptionalString(variableElement, "id");
                if (id.empty())
                {
                    errorMessage = "Mission file has a variable with a missing or empty \"id\"";
                    return false;
                }
                const std::string typeName = GetOptionalString(variableElement, "type");
                MissionValueType type{};
                if (!ParseMissionValueType(typeName, type))
                {
                    errorMessage = "Mission variable \"" + id + "\" has an unknown type \"" + typeName +
                                   "\" (expected bool/int/float/string)";
                    return false;
                }
                MissionValue initial;
                if (!ParseDeclaredValue(variableElement, type, id, initial, errorMessage))
                {
                    return false;
                }
                if (!context.DeclareVariable(id, std::move(initial), errorMessage))
                {
                    return false;
                }
            }
            return true;
        }

        // Compiles one bool expression used as a transition condition.
        bool CompileCondition(const std::string& source,
                              const std::string& stateId,
                              const MissionContext& context,
                              MissionExpression& out,
                              std::string& errorMessage)
        {
            if (!MissionExpression::Compile(source, context, out, errorMessage))
            {
                errorMessage = "Mission state \"" + stateId + "\" condition \"" + source + "\": " + errorMessage;
                return false;
            }
            if (out.GetResultType() != MissionValueType::Bool)
            {
                errorMessage = "Mission state \"" + stateId + "\" condition \"" + source + "\" evaluates to " +
                               std::string(MissionValueTypeName(out.GetResultType())) + ", not bool";
                return false;
            }
            return true;
        }

        // A state's ways out. Three spellings, checked to be mutually exclusive:
        //   * "transitions": [ { "when": ..., "next": ... }, ... ] -- several, first match wins.
        //   * "when" + "next"      -- the single-transition shorthand.
        //   * "condition" + "next" -- the version-1 spelling of that shorthand; kept because a
        //     version-1 condition name is still a declared bool fact, i.e. a valid expression.
        // No spelling at all means a terminal state.
        bool ParseStateTransitions(const JsonElement& stateElement,
                                   const std::string& stateId,
                                   int version,
                                   const MissionContext& context,
                                   std::vector<MissionTransition>& out,
                                   std::string& errorMessage)
        {
            JsonElement whenElement;
            JsonElement conditionElement;
            JsonElement transitionsElement;
            const bool hasWhen = stateElement.TryGetProperty("when", whenElement);
            const bool hasCondition = stateElement.TryGetProperty("condition", conditionElement);
            const bool hasTransitions = stateElement.TryGetProperty("transitions", transitionsElement);
            if ((hasWhen && hasCondition) || (hasTransitions && (hasWhen || hasCondition)))
            {
                errorMessage = "Mission state \"" + stateId +
                               "\" declares more than one of \"transitions\"/\"when\"/\"condition\"; "
                               "use \"transitions\" for several ways out and \"when\" for one";
                return false;
            }

            const std::string next = GetOptionalString(stateElement, "next");
            if (hasWhen || hasCondition)
            {
                const std::string source = hasWhen ? whenElement.GetString() : conditionElement.GetString();
                if (source.empty() && next.empty())
                {
                    return true; // explicitly empty condition on a terminal state
                }
                if (source.empty() || next.empty())
                {
                    errorMessage = "Mission state \"" + stateId +
                                   "\" needs both a condition and a \"next\" state, or neither";
                    return false;
                }
                MissionTransition transition;
                transition.next = next;
                if (!CompileCondition(source, stateId, context, transition.condition, errorMessage))
                {
                    return false;
                }
                out.push_back(std::move(transition));
                return true;
            }

            if (!next.empty())
            {
                errorMessage = "Mission state \"" + stateId +
                               "\" declares a \"next\" state with no condition to reach it";
                return false;
            }
            if (!hasTransitions)
            {
                return true; // terminal state
            }
            if (version < 2)
            {
                errorMessage = "Mission state \"" + stateId +
                               "\" declares \"transitions\", which requires \"version\": 2";
                return false;
            }
            if (transitionsElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "Mission state \"" + stateId + "\" has a \"transitions\" that is not an array";
                return false;
            }

            for (const JsonElement& entry : transitionsElement.EnumerateArray())
            {
                if (out.size() >= kMaxMissionStateTransitions)
                {
                    errorMessage = "Mission state \"" + stateId + "\" declares more than " +
                                   std::to_string(kMaxMissionStateTransitions) + " transitions";
                    return false;
                }
                MissionTransition transition;
                transition.next = GetOptionalString(entry, "next");
                const std::string source = GetOptionalString(entry, "when");
                if (source.empty() || transition.next.empty())
                {
                    errorMessage = "Mission state \"" + stateId +
                                   "\" has a transition without both a \"when\" and a \"next\"";
                    return false;
                }
                if (!CompileCondition(source, stateId, context, transition.condition, errorMessage))
                {
                    return false;
                }
                out.push_back(std::move(transition));
            }
            if (out.empty())
            {
                errorMessage = "Mission state \"" + stateId + "\" has an empty \"transitions\" array";
                return false;
            }
            return true;
        }

        bool ParseStateActions(const JsonElement& stateElement,
                               const std::string& stateId,
                               int version,
                               const MissionContext& context,
                               std::vector<MissionAction>& out,
                               std::string& errorMessage)
        {
            bool present = false;
            JsonElement actionsElement;
            if (!TryGetArray(stateElement, "onEnter", actionsElement, present))
            {
                errorMessage = "Mission state \"" + stateId + "\" has an \"onEnter\" that is not an array";
                return false;
            }
            if (!present)
            {
                return true;
            }
            if (version < 2)
            {
                errorMessage = "Mission state \"" + stateId +
                               "\" declares \"onEnter\", which requires \"version\": 2";
                return false;
            }

            for (const JsonElement& actionElement : actionsElement.EnumerateArray())
            {
                if (out.size() >= kMaxMissionStateActions)
                {
                    errorMessage = "Mission state \"" + stateId + "\" declares more than " +
                                   std::to_string(kMaxMissionStateActions) + " entry actions";
                    return false;
                }

                const std::string kind = GetOptionalString(actionElement, "action");
                MissionAction action;
                if (kind == "set")
                {
                    action.kind = MissionAction::Kind::Set;
                    action.variable = GetOptionalString(actionElement, "variable");
                    if (!context.IsVariable(action.variable))
                    {
                        errorMessage = "Mission state \"" + stateId + "\" sets \"" + action.variable +
                                       "\", which is not a variable this mission declares";
                        return false;
                    }
                    const std::string source = GetOptionalString(actionElement, "value");
                    if (!MissionExpression::Compile(source, context, action.value, errorMessage))
                    {
                        errorMessage = "Mission state \"" + stateId + "\" set \"" + action.variable +
                                       "\" = \"" + source + "\": " + errorMessage;
                        return false;
                    }
                    MissionValueType variableType{};
                    if (!context.TryGetType(action.variable, variableType) ||
                        variableType != action.value.GetResultType())
                    {
                        errorMessage = "Mission state \"" + stateId + "\" assigns a " +
                                       std::string(MissionValueTypeName(action.value.GetResultType())) +
                                       " expression to " + MissionValueTypeName(variableType) +
                                       " variable \"" + action.variable + "\"";
                        return false;
                    }
                }
                else if (kind == "log")
                {
                    action.kind = MissionAction::Kind::Log;
                    action.message = GetOptionalString(actionElement, "message");
                    if (action.message.empty())
                    {
                        errorMessage = "Mission state \"" + stateId + "\" has a log action with no \"message\"";
                        return false;
                    }
                }
                else
                {
                    errorMessage = "Mission state \"" + stateId + "\" has an unknown action \"" + kind +
                                   "\" (expected set/log)";
                    return false;
                }
                out.push_back(std::move(action));
            }
            return true;
        }
    }

    bool LoadMissionDefinition(const std::string& path,
                               const MissionContext& factSchema,
                               MissionDefinition& out,
                               std::string& errorMessage)
    {
        // A mission file is the most content-shaped input the game has -- and the one a tool or a
        // package is most likely to generate -- so it gets the same bounded read as everything
        // else (plan_36 IG-36-002/006/009): size, UTF-8, nesting depth, and an object root, all
        // checked before the parser runs.
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        MissionDefinition definition;
        definition.declaredContext = factSchema;
        try
        {
            const JsonElement& root = file.root;

            definition.id = GetOptionalString(root, "id");
            definition.title = GetOptionalString(root, "title");
            JsonElement versionElement;
            definition.version =
                root.TryGetProperty("version", versionElement) ? static_cast<int>(versionElement.GetInt32()) : 1;
            if (definition.version < kMinMissionFileVersion || definition.version > kMaxMissionFileVersion)
            {
                errorMessage = "Mission file has unsupported \"version\" " +
                               std::to_string(definition.version) + " (supported: " +
                               std::to_string(kMinMissionFileVersion) + "-" +
                               std::to_string(kMaxMissionFileVersion) + "): " + path;
                return false;
            }
            definition.initialState = GetOptionalString(root, "initialState");
            const std::string retryPolicyName = GetOptionalString(root, "retry");
            if (!retryPolicyName.empty())
            {
                if (definition.version < 2)
                {
                    errorMessage = "Mission file declares \"retry\", which requires \"version\": 2: " + path;
                    return false;
                }
                if (!ParseMissionRetryPolicy(retryPolicyName, definition.retryPolicy))
                {
                    errorMessage = "Mission file has an unknown \"retry\" policy \"" + retryPolicyName +
                                   "\" (expected checkpoint/mission_start): " + path;
                    return false;
                }
            }

            if (!ParseVariables(root, definition.version, definition.declaredContext, errorMessage))
            {
                errorMessage += " (" + path + ")";
                return false;
            }

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
                const std::string outcomeName = GetOptionalString(stateElement, "outcome");
                if (!outcomeName.empty() && !ParseMissionOutcome(outcomeName, state.outcome))
                {
                    errorMessage = "Mission state \"" + state.id + "\" has an unknown outcome \"" +
                                   outcomeName + "\" (expected completed/failed): " + path;
                    return false;
                }
                if (!outcomeName.empty() && definition.version < 2)
                {
                    errorMessage = "Mission state \"" + state.id +
                                   "\" declares \"outcome\", which requires \"version\": 2: " + path;
                    return false;
                }
                state.reason = GetOptionalString(stateElement, "reason");
                if (!state.reason.empty() && definition.version < 2)
                {
                    errorMessage = "Mission state \"" + state.id +
                                   "\" declares \"reason\", which requires \"version\": 2: " + path;
                    return false;
                }
                JsonElement checkpointElement;
                if (stateElement.TryGetProperty("checkpoint", checkpointElement))
                {
                    const JsonValueKind checkpointKind = checkpointElement.getValueKindProperty();
                    if (checkpointKind != JsonValueKind::True && checkpointKind != JsonValueKind::False)
                    {
                        errorMessage = "Mission state \"" + state.id +
                                       "\" has a \"checkpoint\" that is not true or false: " + path;
                        return false;
                    }
                    if (definition.version < 2)
                    {
                        errorMessage = "Mission state \"" + state.id +
                                       "\" declares \"checkpoint\", which requires \"version\": 2: " + path;
                        return false;
                    }
                    state.checkpoint = checkpointElement.GetBoolean();
                }
                if (!ParseStateTransitions(stateElement, state.id, definition.version,
                                           definition.declaredContext, state.transitions, errorMessage) ||
                    !ParseStateActions(stateElement, state.id, definition.version, definition.declaredContext,
                                       state.onEnter, errorMessage))
                {
                    errorMessage += " (" + path + ")";
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

        // Graph validation (IG-24-003's smallest form: inline checks, not a separate tool): unique
        // ids, initialState and every non-empty `next` must refer to a real state, and a terminal
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
            for (const MissionTransition& transition : state.transitions)
            {
                if (definition.FindState(transition.next) == nullptr)
                {
                    errorMessage = "Mission state \"" + state.id + "\" has a \"next\" (\"" + transition.next +
                                   "\") that does not match any state id: " + path;
                    return false;
                }
            }
            if (!state.reason.empty() && state.outcome != MissionOutcome::Failed)
            {
                errorMessage = "Mission state \"" + state.id +
                               "\" declares a failure \"reason\" without \"outcome\": \"failed\", so "
                               "nothing would ever show it: " + path;
                return false;
            }
            if (state.checkpoint && state.outcome != MissionOutcome::None)
            {
                errorMessage = "Mission state \"" + state.id + "\" is both a checkpoint and an outcome "
                               "(\"" + MissionOutcomeName(state.outcome) +
                               "\"); a state that ends the mission cannot be retried from: " + path;
                return false;
            }
            if (state.outcome != MissionOutcome::None && !state.transitions.empty())
            {
                errorMessage = "Mission state \"" + state.id + "\" declares outcome \"" +
                               MissionOutcomeName(state.outcome) +
                               "\" and a way out; an outcome ends the mission: " + path;
                return false;
            }
        }

        // Compatibility with schema version 1 and with version-2 files that never say which state
        // ends the mission: a terminal state literally named "completed" is treated as the success
        // outcome, which is what every mission file written before "outcome" existed relies on.
        // The rule applies only when the file declares no outcome of its own, so a file that opts
        // in keeps full control.
        bool declaresOutcome = false;
        for (const MissionStateDefinition& state : definition.states)
        {
            declaresOutcome = declaresOutcome || state.outcome != MissionOutcome::None;
        }
        if (!declaresOutcome)
        {
            for (MissionStateDefinition& state : definition.states)
            {
                if (state.id == "completed" && state.transitions.empty())
                {
                    state.outcome = MissionOutcome::Completed;
                    declaresOutcome = true;
                }
            }
        }
        if (!declaresOutcome)
        {
            errorMessage = "Mission file has no state that ends the mission: give one state "
                           "\"outcome\": \"completed\" (or name a terminal state \"completed\"): " + path;
            return false;
        }

        out = std::move(definition);
        return true;
    }
}
