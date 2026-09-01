#include "IronGang/Gameplay/PedestrianItinerary.hpp"

#include <algorithm>

namespace IronGang
{
    void PedestrianItinerary::BeginWalk(std::string destinationNodeId)
    {
        destinationNodeId_ = std::move(destinationNodeId);
        activity_ = PedestrianActivity::Walking;
        waitSecondsRemaining_ = 0.0F;
    }

    void PedestrianItinerary::BeginWait(float waitSeconds) noexcept
    {
        activity_ = PedestrianActivity::Waiting;
        waitSecondsRemaining_ = std::max(0.0F, waitSeconds);
    }

    void PedestrianItinerary::Update(float deltaSeconds) noexcept
    {
        if (activity_ != PedestrianActivity::Waiting || !(deltaSeconds > 0.0F))
        {
            return;
        }
        waitSecondsRemaining_ = std::max(0.0F, waitSecondsRemaining_ - deltaSeconds);
    }

    bool PedestrianItinerary::WantsNewDestination() const noexcept
    {
        return activity_ == PedestrianActivity::Waiting && waitSecondsRemaining_ <= 0.0F;
    }

    std::string ChoosePedestrianDestination(const std::vector<std::string>& candidates,
                                            const std::string& currentNodeId,
                                            RandomSource& random)
    {
        // Filtering first and drawing once keeps this a single draw from the shared stream: a
        // reject-and-retry loop would consume a variable number of values and make the whole
        // ambient population's sequence depend on where each pedestrian happened to be standing.
        std::vector<const std::string*> options;
        options.reserve(candidates.size());
        for (const std::string& candidate : candidates)
        {
            if (candidate != currentNodeId && !candidate.empty())
            {
                options.push_back(&candidate);
            }
        }
        if (options.empty())
        {
            return {};
        }
        return *options[random.NextIndex(options.size())];
    }
}
