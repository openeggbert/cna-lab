#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_16 IG-16-004: "press E" told through the mission objective line, if at all.
    //
    // The objective says what the mission wants; a prompt says what *this spot* affords, and
    // disappears when it stops affording it. Until now the only hint that the sedan could be
    // entered was the objective text, and the only feedback for being too far away was a status
    // line that appeared **after** pressing a key that did nothing.

    struct InteractionTarget
    {
        // Stable id, used to keep the prompt on one target while the player stands between two.
        std::string id;
        // What the player would be doing: "Enter the sedan". The key is added by the selector, so
        // a rebind changes every prompt without touching content.
        std::string label;
        Vector3 position{};
        float radiusMetres{3.0F};
        // False when the target exists but cannot be used right now -- the sedan while already
        // driving it, a locked door, a mission object before its mission starts.
        bool available{true};
    };

    struct InteractionPrompt
    {
        std::string targetId;
        std::string text;
        bool visible{false};
    };

    // Once a target has the prompt it keeps it until the player leaves this multiple of its
    // radius. Without it, standing on the boundary between two interactables makes the prompt
    // flicker between them every frame, which reads as a bug even though each frame is correct.
    inline constexpr float kInteractionPromptHysteresis = 1.35F;

    class InteractionPromptSelector final
    {
    public:
        // @p actionKeyName is the key currently bound to Interact, so the prompt shows the key the
        // player would actually press (plan_28's rebinding is what makes that matter).
        // Distances are measured in the XZ plane: a target on a balcony overhead is not closer
        // than one at your feet just because the straight-line distance says so.
        [[nodiscard]] InteractionPrompt Select(const Vector3& playerPosition,
                                               const std::vector<InteractionTarget>& targets,
                                               const std::string& actionKeyName);

        // Drops the sticky target: call when interaction is not being offered at all (dialogue, a
        // cutscene, a district transition), so the prompt does not resume on a stale target.
        void Clear() noexcept { currentId_.clear(); }
        [[nodiscard]] const std::string& GetCurrentTargetId() const noexcept { return currentId_; }

    private:
        std::string currentId_;
    };
}
