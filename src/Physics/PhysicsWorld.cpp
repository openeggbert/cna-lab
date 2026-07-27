#include "IronShadows/Physics/PhysicsWorld.hpp"

// The Jolt headers don't include Jolt.h. Always include Jolt.h before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace IronShadows::Physics
{
    namespace
    {
        [[nodiscard]] JPH::Vec3 ToJolt(const Vector3& v) noexcept
        {
            return JPH::Vec3(v.X, v.Y, v.Z);
        }

        [[nodiscard]] Vector3 FromJolt(JPH::Vec3Arg v) noexcept
        {
            return Vector3(v.GetX(), v.GetY(), v.GetZ());
        }

        [[nodiscard]] Vector3 FromJoltR(JPH::RVec3Arg v) noexcept
        {
            return Vector3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
        }

        [[nodiscard]] RigidBodyHandle HandleFromBodyId(JPH::BodyID id) noexcept
        {
            return RigidBodyHandle{id.GetIndexAndSequenceNumber()};
        }

        [[nodiscard]] JPH::BodyID BodyIdFromHandle(RigidBodyHandle handle) noexcept
        {
            return JPH::BodyID(handle.value);
        }

        // Two object layers is enough for this prototype: everything that never moves on its own
        // (world geometry, trigger volumes) and everything that does (dynamic bodies, the vehicle
        // chassis). Triggers are plain bodies with Body::SetIsSensor(true), not a separate layer.
        namespace Layers
        {
            constexpr JPH::ObjectLayer kNonMoving = 0;
            constexpr JPH::ObjectLayer kMoving = 1;
            constexpr JPH::uint kCount = 2;
        }

        namespace BroadPhaseLayers
        {
            constexpr JPH::BroadPhaseLayer kNonMoving(0);
            constexpr JPH::BroadPhaseLayer kMoving(1);
            constexpr JPH::uint kCount = 2;
        }

        class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
        public:
            BroadPhaseLayerInterfaceImpl()
            {
                objectToBroadPhase_[Layers::kNonMoving] = BroadPhaseLayers::kNonMoving;
                objectToBroadPhase_[Layers::kMoving] = BroadPhaseLayers::kMoving;
            }

            [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::kCount; }

            [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                return objectToBroadPhase_[layer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
            {
                switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
                {
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::kNonMoving):
                    return "NON_MOVING";
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::kMoving):
                    return "MOVING";
                default:
                    return "INVALID";
                }
            }
#endif

        private:
            JPH::BroadPhaseLayer objectToBroadPhase_[Layers::kCount];
        };

        class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
        };

        class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
        {
        public:
            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
        };

        // Sensor (trigger) enter/exit tracking. Contact callbacks fire from Jolt's job threads
        // during PhysicsSystem::Update(), so the event queue is mutex-guarded even though
        // PhysicsWorld only ever drains it from the caller's thread after Step() returns.
        class TriggerContactListener final : public JPH::ContactListener
        {
        public:
            void RegisterSensor(JPH::BodyID id) { sensorIds_.insert(id); }
            void UnregisterSensor(JPH::BodyID id) { sensorIds_.erase(id); }

            void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold&,
                               JPH::ContactSettings&) override
            {
                RecordEvent(body1.GetID(), body2.GetID(), true);
            }

            void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
            {
                RecordEvent(pair.GetBody1ID(), pair.GetBody2ID(), false);
            }

            [[nodiscard]] std::vector<TriggerEvent> Consume()
            {
                std::lock_guard<std::mutex> lock(mutex_);
                std::vector<TriggerEvent> out = std::move(events_);
                events_.clear();
                return out;
            }

        private:
            void RecordEvent(JPH::BodyID id1, JPH::BodyID id2, bool entered)
            {
                const bool sensor1 = sensorIds_.contains(id1);
                const bool sensor2 = sensorIds_.contains(id2);
                if (!sensor1 && !sensor2)
                {
                    return;
                }
                const JPH::BodyID sensorId = sensor1 ? id1 : id2;
                const JPH::BodyID otherId = sensor1 ? id2 : id1;
                std::lock_guard<std::mutex> lock(mutex_);
                events_.push_back(TriggerEvent{HandleFromBodyId(sensorId), HandleFromBodyId(otherId), entered});
            }

            std::mutex mutex_;
            std::vector<TriggerEvent> events_;
            std::unordered_set<JPH::BodyID> sensorIds_;
        };

        [[nodiscard]] JPH::RefConst<JPH::Shape> MakeShape(const ShapeDesc& desc)
        {
            switch (desc.kind)
            {
            case ShapeKind::Box:
            {
                JPH::BoxShapeSettings settings(ToJolt(desc.halfExtents));
                return settings.Create().Get();
            }
            case ShapeKind::Capsule:
            {
                JPH::CapsuleShapeSettings settings(desc.cylinderHalfHeight, desc.radius);
                return settings.Create().Get();
            }
            }
            JPH_ASSERT(false);
            JPH::BoxShapeSettings fallback(JPH::Vec3(0.5f, 0.5f, 0.5f));
            return fallback.Create().Get();
        }

        void JoltTraceImpl(const char* format, ...)
        {
            va_list list;
            va_start(list, format);
            char buffer[1024];
            vsnprintf(buffer, sizeof(buffer), format, list);
            va_end(list);
            std::cerr << "[Jolt] " << buffer << '\n';
        }

#ifdef JPH_ENABLE_ASSERTS
        bool JoltAssertFailedImpl(const char* expression, const char* message, const char* file, JPH::uint line)
        {
            std::cerr << file << ':' << line << ": (" << expression << ") " << (message != nullptr ? message : "")
                      << '\n';
            return true; // breakpoint
        }
#endif

        // Jolt requires exactly one process-wide Factory/type registration, done lazily on first
        // use and never torn down (matching the lifetime of the process, same as HelloWorld.cpp's
        // one-shot RegisterTypes()/UnregisterTypes() -- we deliberately never call the latter so a
        // second PhysicsWorld in the same process, e.g. in tests, does not re-register).
        void EnsureJoltRegistered()
        {
            static const bool registered = [] {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = JoltTraceImpl;
                JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl;)
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
                return true;
            }();
            (void)registered;
        }
    }

    struct VehicleRuntime
    {
        JPH::BodyID chassisId;
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::Ref<JPH::VehicleCollisionTester> collisionTester;
    };

    struct PhysicsWorld::Impl
    {
        // Must be Impl's FIRST member: C++ initializes members in declaration order, and Jolt's
        // Factory/Trace/type registration must complete before TempAllocatorImpl or
        // JobSystemThreadPool (below) construct, or those constructors segfault calling into an
        // unregistered Jolt (confirmed empirically -- EnsureJoltRegistered() in the constructor
        // *body* runs too late, after the member initializer list already ran).
        struct JoltRegistrationGuard
        {
            JoltRegistrationGuard() { EnsureJoltRegistered(); }
        };
        JoltRegistrationGuard joltRegistrationGuard;

        Impl()
            : tempAllocator(8 * 1024 * 1024),
              jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                       static_cast<int>(std::clamp(
                           std::thread::hardware_concurrency() > 1
                               ? static_cast<int>(std::thread::hardware_concurrency()) - 1
                               : 1,
                           1, 4)))
        {
            constexpr JPH::uint kMaxBodies = 4096;
            constexpr JPH::uint kNumBodyMutexes = 0;
            constexpr JPH::uint kMaxBodyPairs = 4096;
            constexpr JPH::uint kMaxContactConstraints = 4096;

            physicsSystem.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
                               broadPhaseLayerInterface, objectVsBroadPhaseFilter, objectVsObjectFilter);
            physicsSystem.SetContactListener(&contactListener);
            physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
        }

        ~Impl()
        {
            for (auto& [handle, vehicle] : vehicles)
            {
                physicsSystem.RemoveStepListener(vehicle.constraint);
                physicsSystem.RemoveConstraint(vehicle.constraint);
                bodyInterface().RemoveBody(vehicle.chassisId);
                bodyInterface().DestroyBody(vehicle.chassisId);
            }
        }

        [[nodiscard]] JPH::BodyInterface& bodyInterface() { return physicsSystem.GetBodyInterface(); }
        [[nodiscard]] const JPH::BodyInterface& bodyInterface() const { return physicsSystem.GetBodyInterfaceNoLock(); }

        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseFilter;
        ObjectLayerPairFilterImpl objectVsObjectFilter;
        TriggerContactListener contactListener;
        JPH::PhysicsSystem physicsSystem;

        float accumulatedSeconds = 0.0F;
        static constexpr float kFixedStep = 1.0F / 60.0F;
        static constexpr int kMaxStepsPerCall = 4; // avoid a spiral of death after a long stall

        std::uint32_t nextCharacterHandle = 0;
        std::unordered_map<std::uint32_t, JPH::Ref<JPH::CharacterVirtual>> characters;

        std::uint32_t nextVehicleHandle = 0;
        std::unordered_map<std::uint32_t, VehicleRuntime> vehicles;
    };

    PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {}
    PhysicsWorld::~PhysicsWorld() = default;

    void PhysicsWorld::Step(float deltaSeconds)
    {
        impl_->accumulatedSeconds += deltaSeconds;
        int steps = 0;
        while (impl_->accumulatedSeconds >= Impl::kFixedStep && steps < Impl::kMaxStepsPerCall)
        {
            impl_->physicsSystem.Update(Impl::kFixedStep, 1, &impl_->tempAllocator, &impl_->jobSystem);

            for (auto& [handle, character] : impl_->characters)
            {
                character->Update(Impl::kFixedStep, impl_->physicsSystem.GetGravity(),
                                  JPH::BroadPhaseLayerFilter{}, JPH::ObjectLayerFilter{},
                                  JPH::BodyFilter{}, JPH::ShapeFilter{}, impl_->tempAllocator);
            }

            impl_->accumulatedSeconds -= Impl::kFixedStep;
            ++steps;
        }
        if (steps == Impl::kMaxStepsPerCall)
        {
            impl_->accumulatedSeconds = 0.0F; // drop the rest rather than spiral
        }
    }

    RigidBodyHandle PhysicsWorld::CreateStaticBody(const ShapeDesc& shape, const Vector3& position)
    {
        JPH::BodyCreationSettings settings(MakeShape(shape), JPH::RVec3(ToJolt(position)), JPH::Quat::sIdentity(),
                                          JPH::EMotionType::Static, Layers::kNonMoving);
        const JPH::BodyID id = impl_->bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        return HandleFromBodyId(id);
    }

    RigidBodyHandle PhysicsWorld::CreateDynamicBody(const ShapeDesc& shape, const Vector3& position, float mass)
    {
        JPH::BodyCreationSettings settings(MakeShape(shape), JPH::RVec3(ToJolt(position)), JPH::Quat::sIdentity(),
                                          JPH::EMotionType::Dynamic, Layers::kMoving);
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
        const JPH::BodyID id = impl_->bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
        return HandleFromBodyId(id);
    }

    RigidBodyHandle PhysicsWorld::CreateTrigger(const ShapeDesc& shape, const Vector3& position)
    {
        JPH::BodyCreationSettings settings(MakeShape(shape), JPH::RVec3(ToJolt(position)), JPH::Quat::sIdentity(),
                                          JPH::EMotionType::Static, Layers::kNonMoving);
        settings.mIsSensor = true;
        const JPH::BodyID id = impl_->bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        impl_->contactListener.RegisterSensor(id);
        return HandleFromBodyId(id);
    }

    void PhysicsWorld::DestroyBody(RigidBodyHandle handle)
    {
        if (!handle.IsValid())
        {
            return;
        }
        const JPH::BodyID id = BodyIdFromHandle(handle);
        impl_->contactListener.UnregisterSensor(id);
        impl_->bodyInterface().RemoveBody(id);
        impl_->bodyInterface().DestroyBody(id);
    }

    Vector3 PhysicsWorld::GetBodyPosition(RigidBodyHandle handle) const
    {
        if (!handle.IsValid())
        {
            return Vector3{};
        }
        return FromJoltR(impl_->bodyInterface().GetPosition(BodyIdFromHandle(handle)));
    }

    void PhysicsWorld::SetBodyLinearVelocity(RigidBodyHandle handle, const Vector3& velocity)
    {
        if (!handle.IsValid())
        {
            return;
        }
        impl_->bodyInterface().SetLinearVelocity(BodyIdFromHandle(handle), ToJolt(velocity));
    }

    Vector3 PhysicsWorld::GetBodyLinearVelocity(RigidBodyHandle handle) const
    {
        if (!handle.IsValid())
        {
            return Vector3{};
        }
        return FromJolt(impl_->bodyInterface().GetLinearVelocity(BodyIdFromHandle(handle)));
    }

    RaycastHit PhysicsWorld::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance) const
    {
        RaycastHit result;
        const JPH::RRayCast ray(JPH::RVec3(ToJolt(origin)), ToJolt(direction) * maxDistance);
        JPH::RayCastResult joltHit;
        const bool hit = impl_->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, joltHit);
        if (!hit)
        {
            return result;
        }
        result.hit = true;
        result.fraction = joltHit.mFraction;
        result.body = HandleFromBodyId(joltHit.mBodyID);
        const JPH::RVec3 point = ray.GetPointOnRay(joltHit.mFraction);
        result.point = FromJoltR(point);

        JPH::BodyLockRead lock(impl_->physicsSystem.GetBodyLockInterface(), joltHit.mBodyID);
        if (lock.Succeeded())
        {
            result.normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(joltHit.mSubShapeID2, point));
        }
        return result;
    }

    std::vector<TriggerEvent> PhysicsWorld::ConsumeTriggerEvents() { return impl_->contactListener.Consume(); }

    CharacterHandle PhysicsWorld::CreateCharacter(const Vector3& position, float radius, float cylinderHalfHeight)
    {
        JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
        settings->mShape = new JPH::CapsuleShape(cylinderHalfHeight, radius);
        settings->mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
        settings->mMass = 80.0f;

        const std::uint32_t handleValue = impl_->nextCharacterHandle++;
        impl_->characters[handleValue] = new JPH::CharacterVirtual(
            settings, JPH::RVec3(ToJolt(position)), JPH::Quat::sIdentity(), &impl_->physicsSystem);
        return CharacterHandle{handleValue};
    }

    void PhysicsWorld::DestroyCharacter(CharacterHandle handle)
    {
        impl_->characters.erase(handle.value);
    }

    void PhysicsWorld::MoveCharacter(CharacterHandle handle, const Vector3& desiredVelocity, float)
    {
        const auto it = impl_->characters.find(handle.value);
        if (it == impl_->characters.end())
        {
            return;
        }
        it->second->SetLinearVelocity(ToJolt(desiredVelocity));
    }

    Vector3 PhysicsWorld::GetCharacterPosition(CharacterHandle handle) const
    {
        const auto it = impl_->characters.find(handle.value);
        if (it == impl_->characters.end())
        {
            return Vector3{};
        }
        return FromJoltR(it->second->GetPosition());
    }

    bool PhysicsWorld::IsCharacterGrounded(CharacterHandle handle) const
    {
        const auto it = impl_->characters.find(handle.value);
        if (it == impl_->characters.end())
        {
            return false;
        }
        return it->second->IsSupported();
    }

    VehicleHandle PhysicsWorld::CreateFourWheelVehicle(const Vector3& chassisHalfExtents, float chassisMass,
                                                       const Vector3& position,
                                                       const std::array<Vector3, 4>& wheelLocalPositions,
                                                       float wheelRadius, float wheelWidth)
    {
        JPH::BodyCreationSettings chassisSettings(new JPH::BoxShape(ToJolt(chassisHalfExtents)),
                                                  JPH::RVec3(ToJolt(position)), JPH::Quat::sIdentity(),
                                                  JPH::EMotionType::Dynamic, Layers::kMoving);
        chassisSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        chassisSettings.mMassPropertiesOverride.mMass = chassisMass;
        JPH::Body* chassisBody = impl_->bodyInterface().CreateBody(chassisSettings);
        impl_->bodyInterface().AddBody(chassisBody->GetID(), JPH::EActivation::Activate);

        JPH::VehicleConstraintSettings vehicleSettings;
        for (const Vector3& wheelPos : wheelLocalPositions)
        {
            auto* wheel = new JPH::WheelSettingsWV();
            wheel->mPosition = ToJolt(wheelPos);
            wheel->mRadius = wheelRadius;
            wheel->mWidth = wheelWidth;
            wheel->mSuspensionMinLength = 0.3f;
            wheel->mSuspensionMaxLength = 0.5f;
            wheel->mMaxSteerAngle = wheelPos.Z < 0.0F ? JPH::DegreesToRadians(35.0f) : 0.0f;
            wheel->mMaxHandBrakeTorque = wheelPos.Z >= 0.0F ? 4000.0f : 0.0f;
            vehicleSettings.mWheels.push_back(wheel);
        }

        auto* controllerSettings = new JPH::WheeledVehicleControllerSettings();
        controllerSettings->mDifferentials.resize(1);
        controllerSettings->mDifferentials[0].mLeftWheel = 2;
        controllerSettings->mDifferentials[0].mRightWheel = 3;
        vehicleSettings.mController = controllerSettings;

        VehicleRuntime runtime;
        runtime.chassisId = chassisBody->GetID();
        runtime.constraint = new JPH::VehicleConstraint(*chassisBody, vehicleSettings);
        runtime.collisionTester = new JPH::VehicleCollisionTesterRay(Layers::kMoving);
        runtime.constraint->SetVehicleCollisionTester(runtime.collisionTester);

        impl_->physicsSystem.AddConstraint(runtime.constraint);
        impl_->physicsSystem.AddStepListener(runtime.constraint);

        const std::uint32_t handleValue = impl_->nextVehicleHandle++;
        impl_->vehicles[handleValue] = std::move(runtime);
        return VehicleHandle{handleValue};
    }

    void PhysicsWorld::DestroyVehicle(VehicleHandle handle)
    {
        const auto it = impl_->vehicles.find(handle.value);
        if (it == impl_->vehicles.end())
        {
            return;
        }
        impl_->physicsSystem.RemoveStepListener(it->second.constraint);
        impl_->physicsSystem.RemoveConstraint(it->second.constraint);
        impl_->bodyInterface().RemoveBody(it->second.chassisId);
        impl_->bodyInterface().DestroyBody(it->second.chassisId);
        impl_->vehicles.erase(it);
    }

    void PhysicsWorld::SetVehicleInput(VehicleHandle handle, float forward, float steer, float brake, float handBrake)
    {
        const auto it = impl_->vehicles.find(handle.value);
        if (it == impl_->vehicles.end())
        {
            return;
        }
        impl_->bodyInterface().ActivateBody(it->second.chassisId);
        auto* controller = static_cast<JPH::WheeledVehicleController*>(it->second.constraint->GetController());
        controller->SetDriverInput(forward, steer, brake, handBrake);
    }

    Vector3 PhysicsWorld::GetVehiclePosition(VehicleHandle handle) const
    {
        const auto it = impl_->vehicles.find(handle.value);
        if (it == impl_->vehicles.end())
        {
            return Vector3{};
        }
        return FromJoltR(impl_->bodyInterface().GetPosition(it->second.chassisId));
    }

    float PhysicsWorld::GetVehicleYaw(VehicleHandle handle) const
    {
        const auto it = impl_->vehicles.find(handle.value);
        if (it == impl_->vehicles.end())
        {
            return 0.0F;
        }
        const JPH::Quat rotation = impl_->bodyInterface().GetRotation(it->second.chassisId);
        return rotation.GetEulerAngles().GetY();
    }

    std::array<VehicleWheelState, 4> PhysicsWorld::GetVehicleWheelStates(VehicleHandle handle) const
    {
        std::array<VehicleWheelState, 4> states{};
        const auto it = impl_->vehicles.find(handle.value);
        if (it == impl_->vehicles.end())
        {
            return states;
        }
        const JPH::VehicleConstraint& constraint = *it->second.constraint;
        const std::size_t wheelCount = std::min<std::size_t>(4, constraint.GetWheels().size());
        for (std::size_t i = 0; i < wheelCount; ++i)
        {
            const JPH::Wheel* wheel = constraint.GetWheel(static_cast<JPH::uint>(i));
            VehicleWheelState& state = states[i];
            state.hasContact = wheel->HasContact();
            state.suspensionLength = wheel->GetSuspensionLength();
            state.rotationAngle = wheel->GetRotationAngle();
            state.steerAngle = wheel->GetSteerAngle();
            if (state.hasContact)
            {
                state.contactPoint = FromJoltR(wheel->GetContactPosition());
            }
        }
        return states;
    }
}
