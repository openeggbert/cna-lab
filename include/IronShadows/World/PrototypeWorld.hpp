#pragma once

#include "IronShadows/Core/WorldTypes.hpp"
#include "IronShadows/Physics/PhysicsTypes.hpp"

#include <vector>

namespace IronShadows
{
    namespace Physics
    {
        class PhysicsWorld;
    }

    class PrototypeWorld final
    {
    public:
        explicit PrototypeWorld(DistrictId id = DistrictId::WarehouseBlock);

        [[nodiscard]] DistrictId GetId() const noexcept { return id_; }
        [[nodiscard]] const std::vector<WorldBox>& GetBoxes() const noexcept { return boxes_; }
        [[nodiscard]] const std::vector<Aabb>& GetSolidColliders() const noexcept { return colliders_; }

        // Creates one static Jolt box body per solid collider (plus the ground plane) and
        // returns their handles so a district transition can destroy them again -- world
        // geometry collision is owned by physics (plan_15-physics-integration.md IS-15-009/010)
        // instead of the independent CanOccupy()/ResolveHorizontalMotion() AABB checks below,
        // which remain for lightweight one-off queries (e.g. the vehicle-exit safe-position
        // check).
        [[nodiscard]] std::vector<Physics::RigidBodyHandle> BuildPhysicsStaticBodies(Physics::PhysicsWorld& physics) const;

        [[nodiscard]] const Vector3& GetPlayerSpawn() const noexcept { return playerSpawn_; }
        [[nodiscard]] const Vector3& GetVehicleSpawn() const noexcept { return vehicleSpawn_; }
        [[nodiscard]] float GetVehicleSpawnYaw() const noexcept { return vehicleSpawnYaw_; }
        [[nodiscard]] const TriggerZone& GetWarehouseGoal() const noexcept { return warehouseGoal_; }
        [[nodiscard]] const DistrictExit& GetDistrictExit() const noexcept { return districtExit_; }

        [[nodiscard]] bool CanOccupy(const Vector3& position, float radius) const;
        [[nodiscard]] Vector3 ResolveHorizontalMotion(const Vector3& position,
                                                      const Vector3& delta,
                                                      float radius) const;

    private:
        void AddBox(std::string name,
                    const Vector3& center,
                    const Vector3& size,
                    const Color& color,
                    bool collidable = true);
        void SetGround(const Vector3& center, const Vector3& size, const Color& color);
        void BuildWarehouseBlock();
        void BuildCountryside();

        DistrictId id_;
        std::vector<WorldBox> boxes_;
        std::vector<Aabb> colliders_;
        // The ground plane is deliberately excluded from colliders_ (CanOccupy()/
        // ResolveHorizontalMotion() are XZ-only checks; a ground collider there would cover the
        // whole map and reject every position). Physics needs a real floor, so its bounds are
        // tracked separately and given to physics on its own in BuildPhysicsStaticBodies().
        Aabb groundCollider_{};
        Vector3 playerSpawn_{0.0F, 1.70F, 20.0F};
        Vector3 vehicleSpawn_{0.0F, 0.65F, 11.0F};
        float vehicleSpawnYaw_{0.0F};
        TriggerZone warehouseGoal_{"warehouse_delivery", {{0.0F, 0.0F, -34.0F}, {4.5F, 2.0F, 4.5F}}};
        DistrictExit districtExit_{};
    };
}
