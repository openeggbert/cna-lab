#include "IronGang/Cutscenes/CutsceneSequence.hpp"

#include "../Core/JsonDataFileInternal.hpp"

#include <algorithm>

#include "System/IO/File.hpp"

namespace IronGang
{
    using System::Text::Json::JsonDocument;
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    namespace
    {
        bool GetVector3(const JsonElement& parent, const char* name, Vector3& out, std::string& errorMessage)
        {
            JsonElement array;
            if (!parent.TryGetProperty(name, array) || array.getValueKindProperty() != JsonValueKind::Array ||
                array.GetArrayLength() != 3)
            {
                errorMessage = std::string("Missing or malformed \"") + name + "\" (expected a 3-element array)";
                return false;
            }
            out.X = static_cast<float>(array[0].GetDouble());
            out.Y = static_cast<float>(array[1].GetDouble());
            out.Z = static_cast<float>(array[2].GetDouble());
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

    bool LoadCutsceneSequence(const std::string& path,
                              const std::vector<std::string>& knownLineIds,
                              CutsceneSequence& out,
                              std::string& errorMessage)
    {
        // The same bounded read every data file gets (plan_36 IG-36-002/009).
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        CutsceneSequence sequence;
        try
        {
            const JsonElement& root = file.root;

            sequence.id = GetOptionalString(root, "id");
            JsonElement versionElement;
            sequence.version = root.TryGetProperty("version", versionElement) ? versionElement.GetInt32() : 1;
            JsonElement durationElement;
            if (!root.TryGetProperty("duration", durationElement))
            {
                errorMessage = "Cutscene file is missing \"duration\": " + path;
                return false;
            }
            sequence.duration = static_cast<float>(durationElement.GetDouble());

            JsonElement keyframesElement;
            if (!root.TryGetProperty("cameraKeyframes", keyframesElement) ||
                keyframesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "Cutscene file is missing a \"cameraKeyframes\" array: " + path;
                return false;
            }

            for (const JsonElement& keyframeElement : keyframesElement.EnumerateArray())
            {
                CutsceneCameraKeyframe keyframe;
                JsonElement timeElement;
                if (!keyframeElement.TryGetProperty("time", timeElement))
                {
                    errorMessage = "A cutscene camera keyframe is missing \"time\": " + path;
                    return false;
                }
                keyframe.time = static_cast<float>(timeElement.GetDouble());

                std::string vectorError;
                if (!GetVector3(keyframeElement, "position", keyframe.position, vectorError) ||
                    !GetVector3(keyframeElement, "lookAt", keyframe.lookAt, vectorError))
                {
                    errorMessage = vectorError + " (cutscene keyframe at time " + std::to_string(keyframe.time) +
                                   " in " + path + ")";
                    return false;
                }
                sequence.cameraKeyframes.push_back(keyframe);
            }

            // plan_26 IG-26-002: the dialogue track. Optional -- a camera-only cutscene stays
            // exactly what it was.
            JsonElement cuesElement;
            if (root.TryGetProperty("dialogue", cuesElement))
            {
                if (cuesElement.getValueKindProperty() != JsonValueKind::Array)
                {
                    errorMessage = "Cutscene \"dialogue\" must be an array: " + path;
                    return false;
                }
                for (const JsonElement& cueElement : cuesElement.EnumerateArray())
                {
                    CutsceneDialogueCue cue;
                    JsonElement timeElement;
                    JsonElement lineElement;
                    if (!cueElement.TryGetProperty("time", timeElement) ||
                        timeElement.getValueKindProperty() != JsonValueKind::Number ||
                        !cueElement.TryGetProperty("lineId", lineElement) ||
                        lineElement.getValueKindProperty() != JsonValueKind::String)
                    {
                        errorMessage = "Every cutscene dialogue cue needs a numeric \"time\" and a "
                                       "\"lineId\": " + path;
                        return false;
                    }
                    cue.time = static_cast<float>(timeElement.GetDouble());
                    cue.lineId = lineElement.GetString();
                    sequence.dialogueCues.push_back(std::move(cue));
                }
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        // Validation: at least one keyframe, sorted by strictly ascending time starting at 0,
        // and duration long enough to reach the last keyframe.
        if (sequence.cameraKeyframes.empty())
        {
            errorMessage = "Cutscene file defines no camera keyframes: " + path;
            return false;
        }
        if (sequence.cameraKeyframes.front().time != 0.0F)
        {
            errorMessage = "Cutscene file's first camera keyframe must be at time 0: " + path;
            return false;
        }
        for (std::size_t i = 1; i < sequence.cameraKeyframes.size(); ++i)
        {
            if (sequence.cameraKeyframes[i].time <= sequence.cameraKeyframes[i - 1].time)
            {
                errorMessage = "Cutscene file's camera keyframes must be sorted by strictly ascending time: " + path;
                return false;
            }
        }
        if (sequence.duration < sequence.cameraKeyframes.back().time)
        {
            errorMessage = "Cutscene file's duration is shorter than its last camera keyframe's time: " + path;
            return false;
        }

        // The dialogue track's own rules: in order, inside the sequence, and every line must
        // still exist. The last is the point of the whole id scheme -- a renamed or deleted line
        // is caught here, at load, rather than as silence at the moment the cutscene plays.
        float previousCueTime = -1.0F;
        for (const CutsceneDialogueCue& cue : sequence.dialogueCues)
        {
            if (!(cue.time >= 0.0F) || cue.time > sequence.duration)
            {
                errorMessage = "Cutscene dialogue cue \"" + cue.lineId + "\" is at " +
                               std::to_string(cue.time) + "s, outside the sequence's " +
                               std::to_string(sequence.duration) + "s: " + path;
                return false;
            }
            if (cue.time <= previousCueTime)
            {
                errorMessage = "Cutscene dialogue cues must be in ascending time order (\"" +
                               cue.lineId + "\"): " + path;
                return false;
            }
            previousCueTime = cue.time;

            if (std::find(knownLineIds.begin(), knownLineIds.end(), cue.lineId) == knownLineIds.end())
            {
                errorMessage = "Cutscene cues dialogue line \"" + cue.lineId +
                               "\", which the conversation does not contain: " + path;
                return false;
            }
        }

        out = std::move(sequence);
        return true;
    }
}
