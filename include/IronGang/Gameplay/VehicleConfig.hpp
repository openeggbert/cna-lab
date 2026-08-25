#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <array>
#include <string>
#include <vector>

namespace IronGang
{
    // Schema versions this build understands (plan_17 IG-17-003).
    inline constexpr int kMinVehicleConfigVersion = 1;
    inline constexpr int kMaxVehicleConfigVersion = 1;

    // Everything about the sedan that used to be a constant in VehicleController.cpp: the numbers
    // a designer tunes, separated from the code that drives them.
    //
    // Each member's initializer is the value the game shipped with before this file existed, so a
    // missing or partly unusable vehicle file leaves the sedan exactly as it was rather than
    // breaking driving. The chassis and wheel geometry must keep matching PrototypeRenderer's body
    // and wheel offsets, or the physics chassis and the rendered car stop lining up -- that
    // coupling is real, and the loader cannot check it.
    struct VehicleConfig
    {
        std::string id{"sedan"};
        int version{kMaxVehicleConfigVersion};

        float chassisMass{1400.0F};
        Vector3 chassisHalfExtents{1.05F, 0.325F, 2.1F};

        float wheelRadius{0.33F};
        float wheelWidth{0.3F};
        // Front-left, front-right, rear-left, rear-right, in chassis-local space. Exactly four:
        // the physics layer builds a four-wheel raycast vehicle, not an arbitrary axle count.
        std::array<Vector3, 4> wheelPositions{Vector3{-1.05F, -0.20F, -1.35F},
                                              Vector3{1.05F, -0.20F, -1.35F},
                                              Vector3{-1.05F, -0.20F, 1.35F},
                                              Vector3{1.05F, -0.20F, 1.35F}};

        float maxForwardSpeed{22.0F};
        float maxReverseSpeed{6.0F};
    };

    // Loads @p path into @p out, leaving anything the file does not usably specify at its default.
    //
    // Returns false only when the file exists but cannot be understood at all -- malformed JSON, a
    // root that is not an object, or an unsupported schema version -- and then leaves @p out
    // untouched. A **missing file is not a failure**: the defaults are the sedan the game already
    // drives. Per-field problems are warnings: an unknown key (which catches typos), a value of the
    // wrong type, a value outside its usable range, or a wheel list that is not exactly four
    // positions.
    [[nodiscard]] bool LoadVehicleConfig(const std::string& path,
                                         VehicleConfig& out,
                                         std::string& errorMessage,
                                         std::vector<std::string>* warnings = nullptr);
}
