# Vehicles

The sedan's numbers live in `assets/vehicles/sedan.vehicle.json`, read at startup by
`IronGang::LoadVehicleConfig` (`include/IronGang/Gameplay/VehicleConfig.hpp`,
`src/Gameplay/VehicleConfig.cpp`) and applied through `VehicleController::Configure`. Plan entries:
`plan/plan_17-vehicles-and-driving.md` `IG-17-003`, and `plan_15` `IG-15-025` for the physics behind
it.

Driving itself is a Jolt four-wheel raycast vehicle behind `Physics::PhysicsWorld`; this file tunes
it, it does not implement it.

## Schema

```json
{
  "id": "sedan",
  "version": 1,
  "chassis": { "mass": 1400, "halfExtents": [1.05, 0.325, 2.1] },
  "wheels": {
    "radius": 0.33,
    "width": 0.3,
    "positions": [[-1.05,-0.20,-1.35],[1.05,-0.20,-1.35],[-1.05,-0.20,1.35],[1.05,-0.20,1.35]]
  },
  "performance": { "maxForwardSpeed": 22.0, "maxReverseSpeed": 6.0 },
  "notes": "free text, ignored"
}
```

| Key | Type | Default | Range | Meaning |
| --- | --- | --- | --- | --- |
| `id` | string | `sedan` | non-empty | Identifies the vehicle in logs. |
| `version` | int | `1` | 1 | Schema version. Anything else is a load failure, not a warning. |
| `chassis.mass` | number | `1400` | 50–20000 | Kilograms. |
| `chassis.halfExtents` | 3 numbers | `[1.05, 0.325, 2.1]` | each > 0 | Half the chassis box, in metres. |
| `wheels.radius` | number | `0.33` | 0.05–2.0 | Metres. |
| `wheels.width` | number | `0.3` | 0.02–1.0 | Metres. |
| `wheels.positions` | exactly 4 × 3 numbers | front-left, front-right, rear-left, rear-right | — | Chassis-local, in metres. |
| `performance.maxForwardSpeed` | number | `22.0` | 1–200 | Metres per second (≈ 79 km/h). |
| `performance.maxReverseSpeed` | number | `6.0` | 0.5–200 | Metres per second. |
| `notes` | string | — | — | Accepted and ignored. |

## The coupling the loader cannot check

**`chassis.halfExtents` and `wheels.positions` must keep matching `PrototypeRenderer`'s body box and
wheel offsets.** Nothing validates that: the physics chassis and the drawn car are built from
different code, and the only symptom of a mismatch is a sedan whose wheels float or sink. Change one,
change the other.

## What happens when something is wrong

The same rule as `docs/configuration.md`: a broken or partial file costs the tuning, never the run.

| Situation | Result |
| --- | --- |
| File missing | The built-in sedan, unchanged. Warning, not an error. |
| Unknown key | Ignored, named in a warning **with its section** (`chassis.masss`). |
| Wrong type, or out of range | That value keeps its default; warning names the key and the range. A zero mass or a zero-radius wheel must never reach a physics engine. |
| `wheels.positions` not exactly four entries | **All four** keep their defaults; the count is named. |
| One malformed wheel entry | All four keep their defaults — three applied and one defaulted would be a wheelbase nobody designed. |
| `maxReverseSpeed` > `maxForwardSpeed` | Loads, with a warning: legal, but a typo far more often than a design. |
| Malformed JSON, non-object root, unsupported `version` | **Failures.** `LoadVehicleConfig` returns false, leaves the caller's configuration untouched, and the game logs it and drives the built-in sedan. |

## Applying it

`VehicleController::Configure` must be called **before** the vehicle's physics body is created —
before the first `Reset()`/`Restore()` — because mass, chassis, and wheels are baked in at creation.
Calling it later logs a warning and changes only the speed limits, rather than silently doing half
of what the file says. `IronGangGame::Initialize` therefore loads this file before
`districtManager_.Initialize()`.

## Adding a value

1. Add the member to `VehicleConfig` **with the value the game already uses** as its initializer.
2. Read it in `LoadVehicleConfig` with a range check; add its key to `IsKnownKey` for that section,
   or files using it will warn.
3. Add it to `assets/vehicles/sedan.vehicle.json` and update that file's hash in
   `assets/licenses/asset-registry.csv`.
4. Extend `TestVehicleConfigLoadsValidatesAndFallsBack` — it asserts the committed file loads with
   **zero** warnings and still carries the values the code used to hard-code.
5. Update the tables above.

## Not implemented yet

A second vehicle (this is a one-car roster), gears and an engine torque curve, per-surface grip,
damage, lights, doors and seats (`IG-17-004`, `IG-17-009`–`IG-17-015`), and any runtime reload of
this file. Suspension, steering, and braking currently use Jolt's own defaults rather than tuned
values, so they are not in the schema yet: putting a key in this file that the game does not read
would be worse than leaving it out.
