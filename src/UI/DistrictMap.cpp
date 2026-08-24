#include "IronGang/UI/DistrictMap.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace IronGang
{
    using Microsoft::Xna::Framework::Rectangle;
    using Microsoft::Xna::Framework::Vector2;

    namespace
    {
        float Normalize(float value, float minimum, float maximum) noexcept
        {
            return std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
        }
    }

    Vector2 DistrictMapProjection::ProjectPoint(const Vector3& position) const noexcept
    {
        const float usableWidth = static_cast<float>(std::max(0, screenBounds.Width - 1));
        const float usableHeight = static_cast<float>(std::max(0, screenBounds.Height - 1));
        return {
            static_cast<float>(screenBounds.X) + Normalize(position.X, minimumX, maximumX) * usableWidth,
            static_cast<float>(screenBounds.Y) + Normalize(position.Z, minimumZ, maximumZ) * usableHeight,
        };
    }

    Rectangle DistrictMapProjection::ProjectBox(const WorldBox& box) const noexcept
    {
        const float left = Normalize(box.center.X - box.size.X * 0.5F, minimumX, maximumX);
        const float right = Normalize(box.center.X + box.size.X * 0.5F, minimumX, maximumX);
        const float top = Normalize(box.center.Z - box.size.Z * 0.5F, minimumZ, maximumZ);
        const float bottom = Normalize(box.center.Z + box.size.Z * 0.5F, minimumZ, maximumZ);

        const int x = screenBounds.X + static_cast<int>(std::lround(left * static_cast<float>(screenBounds.Width)));
        const int y = screenBounds.Y + static_cast<int>(std::lround(top * static_cast<float>(screenBounds.Height)));
        const int rightPixel = screenBounds.X +
            static_cast<int>(std::lround(right * static_cast<float>(screenBounds.Width)));
        const int bottomPixel = screenBounds.Y +
            static_cast<int>(std::lround(bottom * static_cast<float>(screenBounds.Height)));
        return {x, y, std::max(1, rightPixel - x), std::max(1, bottomPixel - y)};
    }

    DistrictMapProjection BuildDistrictMapProjection(std::span<const WorldBox> boxes,
                                                       const Rectangle& screenBounds) noexcept
    {
        DistrictMapProjection projection;
        projection.screenBounds = screenBounds;
        if (boxes.empty())
        {
            return projection;
        }

        projection.minimumX = std::numeric_limits<float>::max();
        projection.maximumX = std::numeric_limits<float>::lowest();
        projection.minimumZ = std::numeric_limits<float>::max();
        projection.maximumZ = std::numeric_limits<float>::lowest();
        for (const WorldBox& box : boxes)
        {
            projection.minimumX = std::min(projection.minimumX, box.center.X - box.size.X * 0.5F);
            projection.maximumX = std::max(projection.maximumX, box.center.X + box.size.X * 0.5F);
            projection.minimumZ = std::min(projection.minimumZ, box.center.Z - box.size.Z * 0.5F);
            projection.maximumZ = std::max(projection.maximumZ, box.center.Z + box.size.Z * 0.5F);
        }

        if (projection.maximumX - projection.minimumX < 0.001F)
        {
            projection.minimumX -= 1.0F;
            projection.maximumX += 1.0F;
        }
        if (projection.maximumZ - projection.minimumZ < 0.001F)
        {
            projection.minimumZ -= 1.0F;
            projection.maximumZ += 1.0F;
        }
        return projection;
    }
}
