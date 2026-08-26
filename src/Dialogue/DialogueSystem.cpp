#include "IronGang/Dialogue/DialogueSystem.hpp"

#include "../Core/JsonDataFileInternal.hpp"

#include "System/Text/Json/JsonProperty.hpp"

#include <sstream>
#include <unordered_set>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    bool DialogueSystem::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        // The previous state is only replaced once the whole file has validated: half a
        // conversation is worse than the built-in fallback.
        std::string conversationId;
        std::vector<DialogueLine> loaded;

        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }

        try
        {
            const JsonElement& root = file.root;
            JsonElement versionElement;
            int version = kDialogueFileVersion;
            if (root.TryGetProperty("version", versionElement))
            {
                if (versionElement.getValueKindProperty() != JsonValueKind::Number)
                {
                    errorMessage = "Dialogue \"version\" must be a number: " + path;
                    return false;
                }
                version = static_cast<int>(versionElement.GetInt32());
            }
            if (version != kDialogueFileVersion)
            {
                errorMessage = "Dialogue file has unsupported \"version\" " + std::to_string(version) +
                               " (expected " + std::to_string(kDialogueFileVersion) + "): " + path;
                return false;
            }

            JsonElement idElement;
            if (root.TryGetProperty("id", idElement) &&
                idElement.getValueKindProperty() == JsonValueKind::String)
            {
                conversationId = idElement.GetString();
            }

            JsonElement linesElement;
            if (!root.TryGetProperty("lines", linesElement) ||
                linesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "Dialogue file is missing a \"lines\" array: " + path;
                return false;
            }

            std::unordered_set<std::string> seenIds;
            for (const JsonElement& entry : linesElement.EnumerateArray())
            {
                if (loaded.size() >= kMaxDialogueLines)
                {
                    errorMessage = "Dialogue file has more than " + std::to_string(kMaxDialogueLines) +
                                   " lines: " + path;
                    return false;
                }
                DialogueLine line;
                for (const auto& property : entry.EnumerateObject())
                {
                    const std::string& name = property.getNameProperty();
                    if (name != "id" && name != "speaker" && name != "text")
                    {
                        errorMessage = "Dialogue line has an unknown field \"" + name + "\": " + path;
                        return false;
                    }
                    if (property.getValueProperty().getValueKindProperty() != JsonValueKind::String)
                    {
                        errorMessage = "Dialogue line field \"" + name + "\" must be a string: " + path;
                        return false;
                    }
                    const std::string value = property.getValueProperty().GetString();
                    if (name == "id") { line.id = value; }
                    else if (name == "speaker") { line.speaker = value; }
                    else { line.text = value; }
                }

                if (line.id.empty() || line.speaker.empty() || line.text.empty())
                {
                    errorMessage = "Every dialogue line needs a non-empty \"id\", \"speaker\", and "
                                   "\"text\": " + path;
                    return false;
                }
                if (!seenIds.insert(line.id).second)
                {
                    // Two lines with one id means every reference to it is ambiguous -- and the
                    // translation of one would silently become the translation of both.
                    errorMessage = "Dialogue file has a duplicate line id \"" + line.id + "\": " + path;
                    return false;
                }
                loaded.push_back(std::move(line));
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }

        if (loaded.empty())
        {
            errorMessage = "Dialogue file contains no lines: " + path;
            return false;
        }

        conversationId_ = std::move(conversationId);
        lines_ = std::move(loaded);
        index_ = 0;
        active_ = false;
        finished_ = false;
        return true;
    }

    bool DialogueSystem::SelectLine(const std::string& lineId)
    {
        for (std::size_t index = 0; index < lines_.size(); ++index)
        {
            if (lines_[index].id == lineId)
            {
                index_ = index;
                active_ = true;
                finished_ = false;
                return true;
            }
        }
        return false;
    }

    const std::string& DialogueSystem::GetLineId(std::size_t index) const noexcept
    {
        static const std::string empty;
        return index < lines_.size() ? lines_[index].id : empty;
    }

    const DialogueLine* DialogueSystem::FindLine(const std::string& lineId) const noexcept
    {
        for (const DialogueLine& line : lines_)
        {
            if (line.id == lineId)
            {
                return &line;
            }
        }
        return nullptr;
    }

    void DialogueSystem::LoadFallbackPrologue()
    {
        // Ids match the committed file, so a fallback line and its shipped counterpart are the
        // same line as far as anything referencing it -- or translating it -- is concerned.
        conversationId_ = "prologue";
        lines_ = {
            {"prologue.mara.quiet_tonight", "Mara",
             "Iron City is quiet tonight. That usually means trouble is already moving."},
            {"prologue.elias.take_the_sedan", "Elias",
             "The sedan is outside. Take it to the warehouse before the river shift changes."},
            {"prologue.mara.no_heroics", "Mara", "No heroics. This city remembers every mistake."}
        };
        index_ = 0;
        active_ = false;
        finished_ = false;
    }

    void DialogueSystem::Start()
    {
        index_ = 0;
        active_ = !lines_.empty();
        finished_ = lines_.empty();
    }

    void DialogueSystem::Advance()
    {
        if (!active_)
        {
            return;
        }
        ++index_;
        if (index_ >= lines_.size())
        {
            active_ = false;
            finished_ = true;
        }
    }

    const DialogueLine* DialogueSystem::GetCurrentLine() const noexcept
    {
        if (!active_ || index_ >= lines_.size())
        {
            return nullptr;
        }
        return &lines_[index_];
    }
}
