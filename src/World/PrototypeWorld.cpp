#include "IronGang/World/PrototypeWorld.hpp"

#include <numbers>

#include "IronGang/Physics/PhysicsWorld.hpp"

#include <utility>

namespace IronGang
{
    PrototypeWorld::PrototypeWorld(DistrictId id) : id_(id)
    {
        switch (id_)
        {
        case DistrictId::WarehouseBlock:
            BuildWarehouseBlock();
            break;
        case DistrictId::Countryside:
            BuildCountryside();
            break;
        }
    }

    std::vector<Physics::RigidBodyHandle> PrototypeWorld::BuildPhysicsStaticBodies(Physics::PhysicsWorld& physics) const
    {
        std::vector<Physics::RigidBodyHandle> handles;
        handles.reserve(colliders_.size() + 1);
        handles.push_back(
            physics.CreateStaticBody(Physics::ShapeDesc::Box(groundCollider_.halfExtents), groundCollider_.center));
        for (const Aabb& collider : colliders_)
        {
            handles.push_back(physics.CreateStaticBody(Physics::ShapeDesc::Box(collider.halfExtents), collider.center));
        }
        return handles;
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

    void PrototypeWorld::SetGround(const Vector3& center, const Vector3& size, const Color& color)
    {
        AddBox("ground", center, size, color, false);
        groundCollider_ = Aabb{center, Vector3(size.X * 0.5F, size.Y * 0.5F, size.Z * 0.5F)};
    }

    void PrototypeWorld::BuildWarehouseBlock()
    {
        // Ground and crossing roads.
        SetGround({0.0F, -0.30F, 0.0F}, {100.0F, 0.50F, 100.0F}, Color(67, 103, 61, 255));
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

        playerSpawn_ = {0.0F, 1.70F, 20.0F};
        vehicleSpawn_ = {0.0F, 0.65F, 11.0F};
        vehicleSpawnYaw_ = 0.0F;

        // Gate M9 (plan_19-navigation-and-pathfinding.md IG-19-001/002): a traffic oval using
        // both lanes of road_north_south (X in [-6,6], so lanes at X=+-3 stay clear of the
        // parked sedan at X=0), and two sidewalk back-and-forth paths (X=+-7.5, matching
        // sidewalk_west/sidewalk_east exactly). Kept well inside the road's own Z range
        // ([-45,45]) to stay clear of the district exit trigger at Z=-47 and the warehouse gate
        // at Z=-36.
        trafficLoop_.points = {
            {3.0F, 0.4F, 38.0F},
            {3.0F, 0.4F, -38.0F},
            {-3.0F, 0.4F, -38.0F},
            {-3.0F, 0.4F, 38.0F},
        };
        trafficLoop_.loop = true;

        // plan_21 IG-21-003/007: one signalled crossing where the loop's two straights meet the
        // east-west road. Two stop lines, one per direction of travel, sharing a single signal --
        // the second reads its opposing phase, so the two can never show green together.
        // ForwardFromYaw(0) points down -Z, so the northbound lane (x=+3, travelling -Z) has an
        // approach yaw of 0 and the southbound lane (x=-3) has pi.
        trafficStopLines_ = {
            TrafficStopLine{{3.0F, 0.4F, 8.0F}, 0.0F, {5.5F, 2.6F, 8.0F}, false},
            TrafficStopLine{{-3.0F, 0.4F, -8.0F}, std::numbers::pi_v<float>, {-5.5F, 2.6F, -8.0F}, true},
        };
        for (const TrafficStopLine& stopLine : trafficStopLines_)
        {
            AddBox("stop_line", stopLine.position + Vector3(0.0F, -0.35F, 0.0F), {5.0F, 0.08F, 0.5F},
                   Color(225, 225, 215, 255), false);
        }
        sidewalkPaths_ = {
            WaypointPath{{{-7.5F, 0.9F, -38.0F}, {-7.5F, 0.9F, 38.0F}}, true},
            WaypointPath{{{7.5F, 0.9F, -38.0F}, {7.5F, 0.9F, 38.0F}}, true},
        };

        // The road continues north past the warehouse gate to the district border; a district
        // exit sits there, well clear of the mission's own warehouse-delivery trigger.
        districtExit_.trigger = TriggerZone{"exit_to_countryside", {{0.0F, 0.5F, -47.0F}, {6.0F, 1.5F, 3.0F}}};
        districtExit_.targetDistrict = DistrictId::Countryside;
        districtExit_.targetEntryPosition = {0.0F, 1.70F, 40.0F};
        districtExit_.targetEntryYaw = 0.0F;
        AddBox("district_exit_marker", districtExit_.trigger.bounds.center + Vector3(0.0F, 0.05F, 0.0F),
               {12.0F, 0.10F, 6.0F}, Color(120, 170, 230, 255), false);
    }

    void PrototypeWorld::BuildCountryside()
    {
        // A small farmland crossroads: proves the district-transition mechanism (gate M5) with
        // real, distinct content, not just a copy of the warehouse block. Full countryside
        // content production is a much larger, separate content-authoring task (see plan_31);
        // this is deliberately minimal.
        SetGround({0.0F, -0.30F, 0.0F}, {120.0F, 0.50F, 120.0F}, Color(92, 110, 58, 255));
        AddBox("dirt_road", {0.0F, -0.02F, 0.0F}, {8.0F, 0.10F, 110.0F}, Color(107, 88, 63, 255), false);

        AddBox("barn", {-16.0F, 4.0F, -10.0F}, {14.0F, 8.0F, 10.0F}, Color(122, 46, 40, 255));
        AddBox("barn_roof_trim", {-16.0F, 8.2F, -10.0F}, {15.0F, 0.4F, 11.0F}, Color(70, 26, 22, 255));
        AddBox("farmhouse", {15.0F, 3.5F, -15.0F}, {10.0F, 7.0F, 8.0F}, Color(214, 202, 180, 255));
        AddBox("silo", {-14.0F, 6.0F, 4.0F}, {5.0F, 12.0F, 5.0F}, Color(168, 168, 160, 255));

        // Fence line along the road, decorative (matches the lamp-post pattern in the warehouse
        // block: small, non-collidable posts).
        for (int i = -4; i <= 4; ++i)
        {
            const float z = static_cast<float>(i) * 12.0F;
            AddBox("fence_west", {-6.0F, 0.6F, z}, {0.2F, 1.2F, 0.2F}, Color(90, 70, 50, 255), false);
            AddBox("fence_east", {6.0F, 0.6F, z}, {0.2F, 1.2F, 0.2F}, Color(90, 70, 50, 255), false);
        }

        playerSpawn_ = {0.0F, 1.70F, 40.0F};
        vehicleSpawn_ = {0.0F, 0.65F, 35.0F};
        vehicleSpawnYaw_ = 0.0F;

        // No mission-relevant trigger in this district yet; keep it far outside the playable
        // area so it can never accidentally fire (IronGangGame only evaluates the mission's
        // warehouse-delivery goal while in the WarehouseBlock district, but this keeps the value
        // itself harmless regardless).
        // A real delivery target, so a mission can be set here rather than only in the warehouse
        // block: the yard in front of the farmhouse.
        warehouseGoal_ = TriggerZone{"farmhouse_delivery", {{15.0F, 0.5F, -6.0F}, {5.0F, 2.0F, 4.5F}}};
        AddBox("farmhouse_delivery_marker", warehouseGoal_.bounds.center + Vector3(0.0F, -0.45F, 0.0F),
               {10.0F, 0.10F, 9.0F}, Color(75, 230, 115, 255), false);

        districtExit_.trigger = TriggerZone{"exit_to_warehouse_block", {{0.0F, 0.5F, 47.0F}, {6.0F, 1.5F, 3.0F}}};
        districtExit_.targetDistrict = DistrictId::WarehouseBlock;
        districtExit_.targetEntryPosition = {0.0F, 1.70F, 20.0F};
        districtExit_.targetEntryYaw = 0.0F;
        AddBox("district_exit_marker", districtExit_.trigger.bounds.center + Vector3(0.0F, 0.05F, 0.0F),
               {12.0F, 0.10F, 6.0F}, Color(120, 170, 230, 255), false);
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
