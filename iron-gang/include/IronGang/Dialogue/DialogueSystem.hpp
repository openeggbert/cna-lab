#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace IronGang
{
    // Schema versions this build understands.
    inline constexpr int kDialogueFileVersion = 1;
    // A bound on how much one conversation can be (plan_36 IG-36-002's reasoning).
    inline constexpr std::size_t kMaxDialogueLines = 256;

    struct DialogueLine
    {
        // plan.md's locked decision 10 and plan_25's own header: **every line has a stable id from
        // day one**, so a second language can be added later without touching this system or
        // rewriting content. The prototype's `speaker|text` format had none, which quietly made
        // that decision untrue for as long as it shipped.
        //
        // The id is the translation key and the reference key: it must not change when the English
        // text is edited, which is why it reads `prologue.mara.no_heroics` rather than the words.
        std::string id;
        std::string speaker;
        std::string text;
    };

    class DialogueSystem final
    {
    public:
        // Loads versioned JSON dialogue data. Validation refuses an unsupported version, an empty
        // or duplicate line id, an empty speaker or text, and a conversation with no lines --
        // every one of which is content that would fail silently at the moment a player reached it.
        [[nodiscard]] bool LoadFromFile(const std::string& path, std::string& errorMessage);
        void LoadFallbackPrologue();
        void Start();
        void Advance();
        // Moves the conversation to @p lineId and makes it the current line, returning false (and
        // changing nothing) for an id the conversation does not contain. This is how a cutscene's
        // dialogue track drives the subtitle (plan_26 IG-26-010): the conversation is left exactly
        // where the track put it, so the player carries on from that line rather than restarting.
        [[nodiscard]] bool SelectLine(const std::string& lineId);

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsFinished() const noexcept { return finished_; }
        [[nodiscard]] const DialogueLine* GetCurrentLine() const noexcept;
        [[nodiscard]] std::size_t GetLineCount() const noexcept { return lines_.size(); }
        // Looks a line up by its stable id -- what a mission, a cutscene, or a subtitle test uses
        // to name a line without depending on its position or its words. Null when there is no
        // such id, which is how a stale reference is caught rather than silently showing line 0.
        [[nodiscard]] const DialogueLine* FindLine(const std::string& lineId) const noexcept;
        // The id of the line at @p index, or an empty string when there is none. Used to hand the
        // whole set of ids to content that references them (see LoadCutsceneSequence).
        [[nodiscard]] const std::string& GetLineId(std::size_t index) const noexcept;
        [[nodiscard]] const std::string& GetConversationId() const noexcept { return conversationId_; }

    private:
        std::string conversationId_;
        std::vector<DialogueLine> lines_;
        std::size_t index_{0};
        bool active_{false};
        bool finished_{false};
    };
}
