#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace IronShadows
{
    struct DialogueLine
    {
        std::string speaker;
        std::string text;
    };

    class DialogueSystem final
    {
    public:
        bool LoadFromFile(const std::string& path, std::string& errorMessage);
        void LoadFallbackPrologue();
        void Start();
        void Advance();

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsFinished() const noexcept { return finished_; }
        [[nodiscard]] const DialogueLine* GetCurrentLine() const noexcept;
        [[nodiscard]] std::size_t GetLineCount() const noexcept { return lines_.size(); }

    private:
        std::vector<DialogueLine> lines_;
        std::size_t index_{0};
        bool active_{false};
        bool finished_{false};
    };
}
