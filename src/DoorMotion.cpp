#include "DoorMotion.hpp"

#include <algorithm>

namespace WolfCna
{
    DoorPanelOffset CalculateLateralDoorOffset(
        bool blocksAlongX,
        float openAmount,
        int slideDirection)
    {
        const float direction = slideDirection < 0 ? -1.0f : 1.0f;
        const float travel = std::clamp(openAmount, 0.0f, 1.0f) * direction;
        return blocksAlongX
            ? DoorPanelOffset{0.0f, travel}
            : DoorPanelOffset{travel, 0.0f};
    }

    int SelectDoorSlideDirection(
        int cellX,
        int cellZ,
        bool hasNegativePocket,
        bool hasPositivePocket)
    {
        if (hasNegativePocket != hasPositivePocket)
            return hasNegativePocket ? -1 : 1;
        return ((cellX + cellZ) & 1) == 0 ? 1 : -1;
    }
}
