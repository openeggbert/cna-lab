#include "IronShadows/World/PrototypeWorld.hpp"

#include <utility>

namespace IronShadows
{
    PrototypeWorld::PrototypeWorld()
    {
        BuildCityBlock();
    }

    void PrototypeWorld::AddBox(std::string name,
                                const Vector3& center,
                                const Vector3& size,
                                const Color& color,
                                bool collidable)
    {
        boxes_.push_back(WorldBox{std::move(name), center, size, color, collidable});
        if (collidable)
        {
            colliders_.push_back(Aabb{center, Vector3(size.X * 0.5F, size.Y * 0.5F, size.Z * 0.5F)});
        }
    }

    void PrototypeWorld::BuildCityBlock()
    {
        // Ground and crossing roads.
        AddBox("ground", {0.0F, -0.30F, 0.0F}, {100.0F, 0.50F, 100.0F}, Color(67, 103, 61, 255), false);
        AddBox("road_north_south", {0.0F, -0.02F, 0.0F}, {12.0F, 0.10F, 90.0F}, Color(50, 52, 57, 255), false);
        AddBox("road_east_west", {0.0F, -0.01F, 0.0F}, {90.0F, 0.10F, 12.0F}, Color(50, 52, 57, 255), false);

        // Sidewalks along the main road.
        AddBox("sidewalk_west", {-7.5F, 0.10F, 0.0F}, {3.0F, 0.25F, 90.0F}, Color(150, 145, 135, 255), false);
        AddBox("sidewalk_east", {7.5F, 0.10F, 0.0F}, {3.0F, 0.25F, 90.0F}, Color(150, 145, 135, 255), false);
        AddBox("sidewalk_north", {0.0F, 0.11F, -7.5F}, {90.0F, 0.25F, 3.0F}, Color(150, 145, 135, 255), false);
        AddBox("sidewalk_south", {0.0F, 0.11F, 7.5F}, {90.0F, 0.25F, 3.0F}, Color(150, 145, 135, 255), false);

        // Buildings: intentionally simple debug geometry that will later be replaced by CNJ assets.
        AddBox("hotel", {-18.0F, 6.0F, 18.0F}, {16.0F, 12.0F, 16.0F}, Color(151, 108, 79, 255));
        AddBox("apartments", {18.0F, 7.5F, 19.0F}, {18.0F, 15.0F, 14.0F}, Color(179, 161, 132, 255));
        AddBox("workshop", {-20.0F, 3.5F, -18.0F}, {20.0F, 7.0F, 15.0F}, Color(109, 116, 121, 255));
        AddBox("warehouse", {18.0F, 4.5F, -27.0F}, {20.0F, 9.0F, 22.0F}, Color(115, 93, 76, 255));
        AddBox("warehouse_annex", {-17.0F, 3.0F, -35.0F}, {18.0F, 6.0F, 12.0F}, Color(125, 105, 87, 255));

        // A gate that leaves the target zone reachable from the street.
        AddBox("warehouse_gate_left", {-6.0F, 1.6F, -36.0F}, {1.0F, 3.2F, 8.0F}, Color(82, 82, 86, 255));
        AddBox("warehouse_gate_right", {6.0F, 1.6F, -36.0F}, {1.0F, 3.2F, 8.0F}, Color(82, 82, 86, 255));

        // Street furniture and lamp posts.
        for (int i = -3; i <= 3; ++i)
        {
            const float z = static_cast<float>(i) * 11.0F;
            AddBox("lamp_west", {-9.0F, 2.0F, z}, {0.25F, 4.0F, 0.25F}, Color(53, 55, 59, 255), false);
            AddBox("lamp_east", {9.0F, 2.0F, z}, {0.25F, 4.0F, 0.25F}, Color(53, 55, 59, 255), false);
            AddBox("lamp_glow_west", {-9.0F, 4.15F, z}, {0.65F, 0.30F, 0.65F}, Color(255, 220, 130, 255), false);
            AddBox("lamp_glow_east", {9.0F, 4.15F, z}, {0.65F, 0.30F, 0.65F}, Color(255, 220, 130, 255), false);
        }

        // Lane markings and mission target marker.
        for (int i = -8; i <= 8; ++i)
        {
            AddBox("lane_marking", {0.0F, 0.045F, static_cast<float>(i) * 5.0F},
                   {0.18F, 0.03F, 2.2F}, Color(225, 211, 173, 255), false);
        }
        AddBox("warehouse_target", warehouseGoal_.bounds.center + Vector3(0.0F, 0.08F, 0.0F),
               {8.0F, 0.12F, 8.0F}, Color(63, 190, 95, 255), false);
    }

    bool PrototypeWorld::CanOccupy(const Vector3& position, float radius) const
    {
        for (const Aabb& collider : colliders_)
        {
            if (collider.IntersectsCircleXZ(position, radius))
            {
                return false;
            }
        }
        return true;
    }

    Vector3 PrototypeWorld::ResolveHorizontalMotion(const Vector3& position,
                                                    const Vector3& delta,
                                                    float radius) const
    {
        Vector3 result = position;

        Vector3 candidate = result;
        candidate.X += delta.X;
        if (CanOccupy(candidate, radius))
        {
            result.X = candidate.X;
        }

        candidate = result;
        candidate.Z += delta.Z;
        if (CanOccupy(candidate, radius))
        {
            result.Z = candidate.Z;
        }

        return result;
    }
}
