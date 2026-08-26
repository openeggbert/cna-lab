#include "IronGang/UI/InteractionPrompt.hpp"

#include <algorithm>

namespace IronGang
{
    InteractionPrompt InteractionPromptSelector::Select(const Vector3& playerPosition,
                                                        const std::vector<InteractionTarget>& targets,
                                                        const std::string& actionKeyName)
    {
        const InteractionTarget* best = nullptr;
        float bestDistanceSquared = 0.0F;

        for (const InteractionTarget& target : targets)
        {
            if (!target.available || target.radiusMetres <= 0.0F)
            {
                continue;
            }
            // The target that already has the prompt keeps it out to an enlarged radius, so a
            // player standing between two does not see them trade the prompt every frame.
            const bool sticky = !currentId_.empty() && target.id == currentId_;
            const float radius = target.radiusMetres * (sticky ? kInteractionPromptHysteresis : 1.0F);
            const float distanceSquared = DistanceSquaredXZ(playerPosition, target.position);
            if (distanceSquared > radius * radius)
            {
                continue;
            }
            if (sticky)
            {
                best = &target;
                bestDistanceSquared = -1.0F; // nothing outranks the target already being offered
                continue;
            }
            if (best == nullptr || (bestDistanceSquared >= 0.0F && distanceSquared < bestDistanceSquared))
            {
                best = &target;
                bestDistanceSquared = distanceSquared;
            }
        }

        InteractionPrompt prompt;
        if (best == nullptr)
        {
            currentId_.clear();
            return prompt;
        }

        currentId_ = best->id;
        prompt.targetId = best->id;
        prompt.visible = true;
        // "[E] Enter the sedan". An unbound action still gets a prompt -- the affordance is real
        // even when the player has unbound the key -- but says so rather than showing "[]".
        prompt.text = "[" + (actionKeyName.empty() ? std::string("unbound") : actionKeyName) + "] " +
                      best->label;
        return prompt;
    }
}
