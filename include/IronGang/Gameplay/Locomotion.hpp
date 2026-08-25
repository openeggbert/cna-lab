#pragma once

namespace IronGang
{
    // plan_16 IG-16-005: how on-foot movement builds up and dies away, as pure arithmetic with no
    // physics in it.
    //
    // Before this, movement was instantaneous: the moment a key went down the character was at
    // full speed, and the moment it came up he stopped dead. That reads as a cursor, not a person.
    // Every value here is in the mover's own frame -- forward/strafe/turn -- so the caller keeps
    // owning what "forward" means in the world.
    struct LocomotionSettings
    {
        float walkSpeed{4.2F};
        float sprintMultiplier{1.65F};
        // Metres per second squared. Starting is slower than stopping on purpose: people lean into
        // a walk and plant their feet to halt, and a character who stops faster than he starts
        // feels responsive rather than sluggish.
        float acceleration{18.0F};
        float deceleration{26.0F};
        // Radians per second at full turn input, and how quickly that rate itself is reached.
        float turnSpeed{2.0F};
        float turnAcceleration{14.0F};
    };

    class Locomotion final
    {
    public:
        void Configure(const LocomotionSettings& settings) noexcept { settings_ = settings; }
        [[nodiscard]] const LocomotionSettings& GetSettings() const noexcept { return settings_; }

        // Advances one frame toward what the input asks for. forwardInput/strafeInput/turnInput
        // are each in [-1, 1]; a longer combined stick is clamped so diagonal movement is not
        // faster than straight movement.
        void Update(float deltaSeconds,
                    float forwardInput,
                    float strafeInput,
                    float turnInput,
                    bool sprint) noexcept;

        // Current velocity in the mover's own frame, metres per second.
        [[nodiscard]] float GetForwardVelocity() const noexcept { return forwardVelocity_; }
        [[nodiscard]] float GetStrafeVelocity() const noexcept { return strafeVelocity_; }
        // Magnitude of the two, which is what an animation blend wants.
        [[nodiscard]] float GetSpeed() const noexcept;
        // Radians per second, signed.
        [[nodiscard]] float GetTurnRate() const noexcept { return turnRate_; }
        // True once the character is moving at all -- past a threshold well below walking pace, so
        // it does not flicker as he eases to a stop.
        [[nodiscard]] bool IsMoving() const noexcept;

        // Drops all momentum: a teleport, a respawn, or getting into a car.
        void Stop() noexcept;

    private:
        LocomotionSettings settings_;
        float forwardVelocity_{0.0F};
        float strafeVelocity_{0.0F};
        float turnRate_{0.0F};
    };
}
