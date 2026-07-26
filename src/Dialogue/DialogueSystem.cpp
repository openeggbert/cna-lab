#include "IronShadows/Dialogue/DialogueSystem.hpp"

#include "System/IO/File.hpp"

#include <sstream>

namespace IronShadows
{
    bool DialogueSystem::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        lines_.clear();
        index_ = 0;
        active_ = false;
        finished_ = false;

        if (!System::IO::File::Exists(path))
        {
            errorMessage = "Dialogue file not found: " + path;
            return false;
        }

        try
        {
            const std::vector<std::string> sourceLines = System::IO::File::ReadAllLines(path);
            for (const std::string& raw : sourceLines)
            {
                if (raw.empty() || raw.starts_with('#'))
                {
                    continue;
                }
                const std::size_t separator = raw.find('|');
                if (separator == std::string::npos)
                {
                    continue;
                }
                DialogueLine line;
                line.speaker = raw.substr(0, separator);
                line.text = raw.substr(separator + 1);
                if (!line.text.empty())
                {
                    lines_.push_back(std::move(line));
                }
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = exception.what();
            return false;
        }

        if (lines_.empty())
        {
            errorMessage = "Dialogue file contains no usable speaker|text lines: " + path;
            return false;
        }
        return true;
    }

    void DialogueSystem::LoadFallbackPrologue()
    {
        lines_ = {
            {"Mara", "Iron City is quiet tonight. That usually means trouble is already moving."},
            {"Elias", "The sedan is outside. Take it to the warehouse before the river shift changes."},
            {"Mara", "No heroics. This city remembers every mistake."}
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
