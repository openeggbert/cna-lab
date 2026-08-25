#pragma once

namespace IronGang
{
    // plan_17 IG-17-015: how a sedan takes damage, kept as pure arithmetic so it can be tested
    // without a physics world.
    //
    // Impacts are detected from the vehicle's own speed history rather than from contact reports:
    // a frame in which speed drops faster than any brake could manage **is** a collision. That is
    // deliberate. It needs nothing new from the physics layer, it cannot miss a collision the
    // solver resolved internally, and at Mafia-1 fidelity the difference between "hit a wall" and
    // "decelerated at 30 g" is not one the player can perceive.
    //
    // The separation is wide: a good car brakes at roughly 1 g (about 0.17 m/s per 60 Hz frame),
    // while a 20 m/s crash stops in a frame or two. The threshold sits an order of magnitude above
    // braking, so ordinary driving can never scratch the paint.
    struct VehicleDamageSettings
    {
        // Speed lost in one second, above which the loss counts as an impact rather than braking.
        // 40 m/s^2 is about 4 g -- far beyond any brake, far below a real crash.
        float impactDecelerationThreshold{40.0F};
        // Integrity lost per m/s of impact severity (the speed lost beyond the threshold).
        float integrityLostPerImpactSpeed{0.055F};
        // How much of its top speed a wrecked car keeps. A disabled sedan still rolls; it does not
        // become a wall.
        float minimumSpeedFactor{0.35F};
    };

    // Integrity runs 1 (undamaged) to 0 (wrecked). Nothing here knows about physics, input, or
    // rendering -- callers feed it speeds and read the consequences.
    class VehicleDamage final
    {
    public:
        void Configure(const VehicleDamageSettings& settings) noexcept { settings_ = settings; }
        [[nodiscard]] const VehicleDamageSettings& GetSettings() const noexcept { return settings_; }

        // Feeds one frame of motion. Returns the integrity lost this frame (0 when nothing
        // happened), so a caller can react to the impact itself -- a sound, a screen shake -- and
        // not merely to the total.
        float RegisterFrame(float previousSpeed, float currentSpeed, float deltaSeconds) noexcept;

        [[nodiscard]] float GetIntegrity() const noexcept { return integrity_; }
        // True once the car is wrecked. It still steers and rolls, at minimumSpeedFactor of its
        // top speed -- being stranded in a wreck is a situation; being teleported out of a car
        // that vanished is a bug.
        [[nodiscard]] bool IsDisabled() const noexcept { return integrity_ <= 0.0F; }
        // Fraction of top speed the car can still reach, 1 when undamaged, falling to
        // minimumSpeedFactor when wrecked.
        [[nodiscard]] float GetSpeedFactor() const noexcept;

        // Repairs completely -- what starting or retrying a mission gives the player.
        void Reset() noexcept { integrity_ = 1.0F; }
        // Restores a saved value, clamped to the valid range.
        void SetIntegrity(float integrity) noexcept;

    private:
        VehicleDamageSettings settings_;
        float integrity_{1.0F};
    };
}
