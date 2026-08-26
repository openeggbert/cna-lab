#include "IronGang/Input/InputScript.hpp"

#include "../Core/JsonDataFileInternal.hpp"
#include "IronGang/Core/AtomicFile.hpp"

#include "System/Text/Json/JsonProperty.hpp"

#include <algorithm>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    namespace
    {
        const HeldActions& NothingHeld() noexcept
        {
            static const HeldActions none{};
            return none;
        }

        [[nodiscard]] bool ReadHeldArray(const JsonElement& element, HeldActions& out, std::string& error)
        {
            out = HeldActions{};
            if (element.getValueKindProperty() != JsonValueKind::Array)
            {
                error = "a step's \"held\" must be an array of action ids";
                return false;
            }
            for (const JsonElement& entry : element.EnumerateArray())
            {
                if (entry.getValueKindProperty() != JsonValueKind::String)
                {
                    error = "a step's \"held\" entries must be action id strings";
                    return false;
                }
                const std::string id = entry.GetString();
                GameAction action{};
                if (!ParseGameActionId(id, action))
                {
                    // The whole point of naming actions rather than keys: an id that no longer
                    // exists is a script that would silently do nothing at that moment.
                    error = "unknown action id \"" + id + "\" in an input script";
                    return false;
                }
                out[static_cast<std::size_t>(action)] = true;
            }
            return true;
        }
    }

    int InputScript::GetLastUpdate() const noexcept
    {
        return steps_.empty() ? -1 : steps_.back().update;
    }

    void InputScript::Rewind() noexcept
    {
        updateIndex_ = -1;
    }

    void InputScript::Advance() noexcept
    {
        ++updateIndex_;
    }

    bool InputScript::IsFinished() const noexcept
    {
        return updateIndex_ > GetLastUpdate();
    }

    const HeldActions& InputScript::StateAt(int update) const noexcept
    {
        if (update < 0 || steps_.empty() || update < steps_.front().update)
        {
            return NothingHeld();
        }
        // Steps are ascending and sparse: the step in effect is the last one at or before this
        // update, so a held set persists until something changes it.
        const auto found = std::upper_bound(steps_.begin(), steps_.end(), update,
                                            [](int value, const InputScriptStep& step) {
                                                return value < step.update;
                                            });
        return (found - 1)->held;
    }

    bool InputScript::IsDown(GameAction action) const noexcept
    {
        if (action >= GameAction::Count)
        {
            return false;
        }
        return StateAt(updateIndex_)[static_cast<std::size_t>(action)];
    }

    bool InputScript::WasPressed(GameAction action) const noexcept
    {
        if (action >= GameAction::Count)
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(action);
        return StateAt(updateIndex_)[index] && !StateAt(updateIndex_ - 1)[index];
    }

    bool InputScript::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }
        const JsonElement& root = file.root;

        std::string id;
        std::vector<InputScriptStep> steps;
        try
        {
            for (const auto& property : root.EnumerateObject())
            {
                const std::string name = property.getNameProperty();
                if (name != "id" && name != "version" && name != "steps")
                {
                    errorMessage = "unknown field \"" + name + "\" in input script: " + path;
                    return false;
                }
            }

            JsonElement versionElement;
            if (!root.TryGetProperty("version", versionElement) ||
                versionElement.getValueKindProperty() != JsonValueKind::Number)
            {
                errorMessage = "input script has no numeric \"version\": " + path;
                return false;
            }
            const int version = versionElement.GetInt32();
            if (version != kInputScriptVersion)
            {
                errorMessage = "input script version " + std::to_string(version) + " is not supported (expected " +
                               std::to_string(kInputScriptVersion) + "): " + path;
                return false;
            }

            JsonElement idElement;
            if (!root.TryGetProperty("id", idElement) ||
                idElement.getValueKindProperty() != JsonValueKind::String ||
                idElement.GetString().empty())
            {
                errorMessage = "input script has no non-empty \"id\": " + path;
                return false;
            }
            id = idElement.GetString();

            JsonElement stepsElement;
            if (!root.TryGetProperty("steps", stepsElement) ||
                stepsElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "input script has no \"steps\" array: " + path;
                return false;
            }

            int previousUpdate = -1;
            for (const JsonElement& stepElement : stepsElement.EnumerateArray())
            {
                if (steps.size() >= kMaxInputScriptSteps)
                {
                    errorMessage = "input script has more than " + std::to_string(kMaxInputScriptSteps) +
                                   " steps: " + path;
                    return false;
                }
                for (const auto& property : stepElement.EnumerateObject())
                {
                    const std::string name = property.getNameProperty();
                    if (name != "update" && name != "held")
                    {
                        errorMessage = "unknown field \"" + name + "\" in an input script step: " + path;
                        return false;
                    }
                }

                InputScriptStep step;
                JsonElement updateElement;
                if (!stepElement.TryGetProperty("update", updateElement) ||
                    updateElement.getValueKindProperty() != JsonValueKind::Number)
                {
                    errorMessage = "every input script step needs a numeric \"update\": " + path;
                    return false;
                }
                step.update = updateElement.GetInt32();
                if (step.update < 0)
                {
                    errorMessage = "an input script step has a negative update index: " + path;
                    return false;
                }
                if (step.update <= previousUpdate)
                {
                    errorMessage = "input script steps must be in ascending update order (update " +
                                   std::to_string(step.update) + " follows " + std::to_string(previousUpdate) +
                                   "): " + path;
                    return false;
                }
                previousUpdate = step.update;

                JsonElement heldElement;
                if (!stepElement.TryGetProperty("held", heldElement))
                {
                    errorMessage = "every input script step needs a \"held\" array: " + path;
                    return false;
                }
                std::string heldError;
                if (!ReadHeldArray(heldElement, step.held, heldError))
                {
                    errorMessage = heldError + ": " + path;
                    return false;
                }
                steps.push_back(step);
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string("failed to read input script ") + path + ": " + exception.what();
            return false;
        }

        if (steps.empty())
        {
            errorMessage = "input script has no steps, so it would play back nothing: " + path;
            return false;
        }

        id_ = std::move(id);
        steps_ = std::move(steps);
        Rewind();
        return true;
    }

    void InputScriptRecorder::Record(const HeldActions& held)
    {
        if (!started_ || held != previous_)
        {
            if (steps_.size() < kMaxInputScriptSteps)
            {
                steps_.push_back(InputScriptStep{updateCount_, held});
            }
            previous_ = held;
            started_ = true;
        }
        ++updateCount_;
    }

    bool InputScriptRecorder::Save(const std::string& path, std::string& errorMessage) const
    {
        if (steps_.empty())
        {
            errorMessage = "nothing was recorded, so no input script was written: " + path;
            return false;
        }

        std::string text;
        text += "{\n";
        text += "  \"id\": \"" + id_ + "\",\n";
        text += "  \"version\": " + std::to_string(kInputScriptVersion) + ",\n";
        text += "  \"steps\": [\n";
        for (std::size_t index = 0; index < steps_.size(); ++index)
        {
            const InputScriptStep& step = steps_[index];
            text += "    { \"update\": " + std::to_string(step.update) + ", \"held\": [";
            bool first = true;
            for (std::size_t action = 0; action < kGameActionCount; ++action)
            {
                if (!step.held[action])
                {
                    continue;
                }
                if (!first)
                {
                    text += ", ";
                }
                text += "\"";
                text += GameActionId(static_cast<GameAction>(action));
                text += "\"";
                first = false;
            }
            text += "] }";
            text += index + 1 < steps_.size() ? ",\n" : "\n";
        }
        text += "  ]\n";
        text += "}\n";
        return WriteTextFileAtomically(path, text, false, errorMessage);
    }
}
