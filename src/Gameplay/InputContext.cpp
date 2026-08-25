#include "IronGang/Gameplay/InputContext.hpp"

namespace IronGang
{
    InputContext ResolveInputContext(const GameplaySignals& signals) noexcept
    {
        if (signals.paused)
        {
            return InputContext::Paused;
        }
        if (signals.districtTransitioning)
        {
            return InputContext::DistrictTransition;
        }
        if (signals.cutsceneActive)
        {
            return InputContext::Cutscene;
        }
        if (signals.dialogueActive)
        {
            return InputContext::Dialogue;
        }
        if (signals.vehicleTransitionActive)
        {
            return InputContext::VehicleTransition;
        }
        return signals.driving ? InputContext::Driving : InputContext::OnFoot;
    }

    const char* InputContextName(InputContext context) noexcept
    {
        switch (context)
        {
            case InputContext::Paused: return "paused";
            case InputContext::DistrictTransition: return "district_transition";
            case InputContext::Cutscene: return "cutscene";
            case InputContext::Dialogue: return "dialogue";
            case InputContext::VehicleTransition: return "vehicle_transition";
            case InputContext::Driving: return "driving";
            case InputContext::OnFoot: return "on_foot";
        }
        return "on_foot";
    }

    bool ContextAdvancesWorld(InputContext context) noexcept
    {
        return context != InputContext::Paused;
    }

    bool ContextAllowsMovement(InputContext context) noexcept
    {
        return context == InputContext::OnFoot || context == InputContext::Driving;
    }

    bool ContextAllowsInteraction(InputContext context) noexcept
    {
        return ContextAllowsMovement(context);
    }

    SaveBlockReason SaveBlockReasonForContext(InputContext context) noexcept
    {
        switch (context)
        {
            case InputContext::DistrictTransition: return SaveBlockReason::DistrictTransition;
            case InputContext::Cutscene: return SaveBlockReason::Cutscene;
            case InputContext::Dialogue: return SaveBlockReason::Dialogue;
            case InputContext::VehicleTransition: return SaveBlockReason::VehicleTransition;
            case InputContext::Paused:
            case InputContext::Driving:
            case InputContext::OnFoot:
                break;
        }
        return SaveBlockReason::None;
    }
}
