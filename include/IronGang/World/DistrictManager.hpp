#pragma once

#include "IronGang/Physics/PhysicsTypes.hpp"
#include "IronGang/World/PrototypeWorld.hpp"

#include <memory>
#include <string>
#include <vector>

namespace IronGang
{
    namespace Physics
    {
        class PhysicsWorld;
    }

    // Owns the currently loaded district and its physics static bodies, and the small state
    // machine that moves between two districts through a loading screen (plan_13
    // IG-13-001/002/006/010/013). No background loading, no streaming: a transition destroys the
    // current district's static bodies, constructs the target PrototypeWorld, and rebuilds them,
    // all synchronously -- the "loading screen" exists to give that instant swap a minimum
    // display time (IG-13-013), not to hide real asynchronous work (there is none yet; that is
    // separate, larger follow-up work, see IG-13-034/035).
    class DistrictManager final
    {
    public:
explicit DistrictManager(DistrictId initial = DistrictId::WarehouseBlock,
                                 std::string assetRoot = std::string());

        // Builds the initial district's physics bodies. Call once after construction.
        void Initialize(Physics::PhysicsWorld& physics);

        // Destroys the current district's static bodies, swaps to the district named by the
        // current district's own DistrictExit, and rebuilds them for the new district. Player/
        // vehicle bodies are untouched here -- the caller repositions them once ConsumeArrival()
        // reports the loading screen's minimum display time has elapsed.
        void RequestTransition(Physics::PhysicsWorld& physics);

        // Direct load, bypassing the exit-trigger/loading-screen flow: used by save/load restore
        // and the prototype reset key, where the target district is already known and there is
        // no in-world walk-through-a-trigger moment to show a loading screen for. A no-op if
        // already in the requested district. The caller is responsible for repositioning
        // player/vehicle and rebuilding renderer geometry afterwards (same as after
        // ConsumeArrival() returns true).
        void LoadDistrict(DistrictId id, Physics::PhysicsWorld& physics);

        // Ticks the loading-screen timer. Call once per frame regardless of transition state.
        void Update(float deltaSeconds);

        [[nodiscard]] bool IsTransitioning() const noexcept { return transitionTimer_ > 0.0F; }
        [[nodiscard]] float GetTransitionProgress() const noexcept;

        // True exactly once, the first Update() after a transition's minimum display time has
        // elapsed; the caller must reposition the player/vehicle at GetWorld()'s spawn points
        // when this returns true (IG-13-008/009/017/018).
        [[nodiscard]] bool ConsumeArrival();

        [[nodiscard]] const PrototypeWorld& GetWorld() const noexcept { return *world_; }

    private:
        void SwapDistrict(DistrictId target, Physics::PhysicsWorld& physics);

        std::string assetRoot_;
        std::unique_ptr<PrototypeWorld> world_;
        std::vector<Physics::RigidBodyHandle> staticBodies_;
        float transitionTimer_{0.0F};
        bool pendingArrival_{false};

        static constexpr float kMinLoadingScreenSeconds = 0.6F;
    };
}
