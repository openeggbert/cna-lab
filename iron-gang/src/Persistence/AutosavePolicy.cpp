#include "IronGang/Persistence/AutosavePolicy.hpp"

namespace IronGang
{
    const char* DescribeSaveBlockReason(SaveBlockReason reason) noexcept
    {
        switch (reason)
        {
            case SaveBlockReason::None: return "";
            case SaveBlockReason::Cutscene: return "a cutscene is playing";
            case SaveBlockReason::Dialogue: return "a conversation is in progress";
            case SaveBlockReason::DistrictTransition: return "the district is still loading";
            case SaveBlockReason::VehicleTransition: return "you are getting in or out of the car";
        }
        return "";
    }

    const char* DescribeAutosaveTrigger(AutosaveTrigger trigger) noexcept
    {
        switch (trigger)
        {
            case AutosaveTrigger::None: return "";
            case AutosaveTrigger::Interval: return "periodic";
            case AutosaveTrigger::DistrictArrival: return "district arrival";
            case AutosaveTrigger::Checkpoint: return "checkpoint";
        }
        return "";
    }

    void AutosaveScheduler::Configure(float intervalSeconds, float minimumSpacingSeconds)
    {
        intervalSeconds_ = intervalSeconds;
        minimumSpacingSeconds_ = minimumSpacingSeconds > 0.0F ? minimumSpacingSeconds : 0.0F;
    }

    void AutosaveScheduler::Reset()
    {
        secondsSinceSave_ = 0.0F;
        pending_ = AutosaveTrigger::None;
    }

    void AutosaveScheduler::Request(AutosaveTrigger trigger)
    {
        if (trigger == AutosaveTrigger::None)
        {
            return;
        }
        // Higher-priority triggers win the *label*; the save itself is the same either way.
        if (static_cast<int>(trigger) > static_cast<int>(pending_))
        {
            pending_ = trigger;
        }
    }

    AutosaveTrigger AutosaveScheduler::Update(float deltaSeconds, SaveBlockReason blockReason)
    {
        secondsSinceSave_ += deltaSeconds;
        if (intervalSeconds_ > 0.0F && secondsSinceSave_ >= intervalSeconds_)
        {
            Request(AutosaveTrigger::Interval);
        }

        if (pending_ == AutosaveTrigger::None)
        {
            return AutosaveTrigger::None;
        }
        if (blockReason != SaveBlockReason::None)
        {
            // Held, not dropped: this is the whole point of scheduling autosaves rather than
            // writing them wherever the trigger happened to fire.
            return AutosaveTrigger::None;
        }
        if (secondsSinceSave_ < minimumSpacingSeconds_)
        {
            return AutosaveTrigger::None;
        }

        const AutosaveTrigger trigger = pending_;
        pending_ = AutosaveTrigger::None;
        secondsSinceSave_ = 0.0F;
        return trigger;
    }
}
