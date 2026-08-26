#include "IronGang/World/DistrictManager.hpp"

#include "IronGang/Physics/PhysicsWorld.hpp"

#include <algorithm>

namespace IronGang
{
    DistrictManager::DistrictManager(DistrictId initial, std::string assetRoot)
        : assetRoot_(std::move(assetRoot)),
          world_(std::make_unique<PrototypeWorld>(initial, assetRoot_))
    {
    }

    void DistrictManager::Initialize(Physics::PhysicsWorld& physics)
    {
        staticBodies_ = world_->BuildPhysicsStaticBodies(physics);
    }

    void DistrictManager::SwapDistrict(DistrictId target, Physics::PhysicsWorld& physics)
    {
        for (Physics::RigidBodyHandle handle : staticBodies_)
        {
            physics.DestroyBody(handle);
        }
        staticBodies_.clear();

        world_ = std::make_unique<PrototypeWorld>(target, assetRoot_);
        staticBodies_ = world_->BuildPhysicsStaticBodies(physics);
    }

    void DistrictManager::RequestTransition(Physics::PhysicsWorld& physics)
    {
        const DistrictExit exit = world_->GetDistrictExit();
        SwapDistrict(exit.targetDistrict, physics);
        transitionTimer_ = kMinLoadingScreenSeconds;
        pendingArrival_ = true;
    }

    void DistrictManager::LoadDistrict(DistrictId id, Physics::PhysicsWorld& physics)
    {
        if (id == world_->GetId())
        {
            return;
        }
        SwapDistrict(id, physics);
    }

    void DistrictManager::Update(float deltaSeconds)
    {
        if (transitionTimer_ > 0.0F)
        {
            transitionTimer_ = std::max(0.0F, transitionTimer_ - deltaSeconds);
        }
    }

    float DistrictManager::GetTransitionProgress() const noexcept
    {
        if (kMinLoadingScreenSeconds <= 0.0F)
        {
            return 1.0F;
        }
        return 1.0F - (transitionTimer_ / kMinLoadingScreenSeconds);
    }

    bool DistrictManager::ConsumeArrival()
    {
        if (pendingArrival_ && transitionTimer_ <= 0.0F)
        {
            pendingArrival_ = false;
            return true;
        }
        return false;
    }
}
