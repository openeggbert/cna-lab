#pragma once

namespace WolfCna
{
    struct DoorPanelOffset final
    {
        float x = 0.0f;
        float z = 0.0f;
    };

    [[nodiscard]] DoorPanelOffset CalculateLateralDoorOffset(
        bool blocksAlongX,
        float openAmount,
        int slideDirection);

    [[nodiscard]] int SelectDoorSlideDirection(
        int cellX,
        int cellZ,
        bool hasNegativePocket,
        bool hasPositivePocket);
}
