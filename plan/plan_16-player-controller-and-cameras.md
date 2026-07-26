# 16. Player controller and cameras

[Back to master plan](../plan.md)

Evolve the existing debug controller (on-foot movement with simple collision already works, see `src/Gameplay/PlayerController.cpp`) into a production third-person character controller and camera. Photo mode is cut to a single someday task.

- [x] **IS-16-001 P0** — Run and tune the existing on-foot controller in the integrated build.
- [ ] **IS-16-002 P0** — Replace circle/AABB collision with the production character controller.
- [ ] **IS-16-003 P0** — Create camera collision and obstruction handling.
- [ ] **IS-16-004 P0** — Create interaction prompts for the car and mission objects.
- [ ] **IS-16-005 P1** — Add acceleration, deceleration, turning inertia, and slope handling.
- [ ] **IS-16-006 P1** — Add step-up, ledge, ground, and falling states.
- [ ] **IS-16-007 P1** — Add crouch only if required by mission design.
- [ ] **IS-16-008 P1** — Add sprint stamina only if it contributes to gameplay.
- [ ] **IS-16-009 P1** — Add context-sensitive interaction targeting.
- [ ] **IS-16-010 P1** — Add doors, switches, pickups, seats, and talk interactions.
- [ ] **IS-16-011 P1** — Add camera shoulder selection and collision fade rules.
- [ ] **IS-16-012 P1** — Add camera zones for interiors and narrow spaces.
- [ ] **IS-16-013 P1** — Add vehicle entry/exit alignment and safe-position search.
- [ ] **IS-16-014 P1** — Add control locking through explicit gameplay modes.
- [ ] **IS-16-015 P1** — Add motion/animation synchronization.
- [ ] **IS-16-016 P1** — Add footstep event and surface lookup.
- [ ] **IS-16-017 P1** — Add player state restoration at checkpoints.
- [ ] **IS-16-018 P1** — Add gamepad input and vibration.
- [ ] **IS-16-019 P1** — Add input buffering where animation gates interactions.
- [ ] **IS-16-020 P3** — Add a debug free-camera mode (photo mode is not otherwise planned).
- [ ] **IS-16-021 P2** — Add first-person inspection only if campaign design requires it.
- [ ] **IS-16-022 P2** — Add cinematic camera handoff blending.

## On-foot locomotion

- [ ] **IS-16-023 P1** — Define the scope, public API, and versioned config for on-foot locomotion.
- [ ] **IS-16-024 P1** — Implement the smallest deterministic reference path for production on-foot locomotion.
- [ ] **IS-16-025 P1** — Add input validation and actionable failure reporting to on-foot locomotion.
- [ ] **IS-16-026 P1** — Add focused unit tests for on-foot locomotion.
- [ ] **IS-16-027 P1** — Add an integration scenario that exercises on-foot locomotion in a running game flow.
- [ ] **IS-16-028 P1** — Define save/checkpoint serialization and restoration for on-foot locomotion.
- [ ] **IS-16-029 P2** — Add logging and debug inspection for on-foot locomotion.
- [ ] **IS-16-030 P2** — Document usage examples and common failure modes for on-foot locomotion.

## Character collision controller

- [ ] **IS-16-031 P1** — Define the scope, public API, and versioned config for the character collision controller.
- [ ] **IS-16-032 P1** — Implement the smallest deterministic reference path for the character collision controller (reuse cna-extended's 3D collision where it fits).
- [ ] **IS-16-033 P1** — Add input validation and actionable failure reporting to the character collision controller.
- [ ] **IS-16-034 P1** — Add focused unit tests for the character collision controller.
- [ ] **IS-16-035 P1** — Add an integration scenario that exercises the character collision controller in a running game flow.
- [ ] **IS-16-036 P1** — Define save/checkpoint serialization and restoration for the character collision controller.
- [ ] **IS-16-037 P2** — Add logging and debug inspection for the character collision controller.
- [ ] **IS-16-038 P2** — Document usage examples and common failure modes for the character collision controller.

## Third-person camera

- [ ] **IS-16-039 P1** — Define the scope, public API, and versioned config for the third-person camera.
- [ ] **IS-16-040 P1** — Implement the smallest deterministic reference path for the third-person camera.
- [ ] **IS-16-041 P1** — Add input validation and actionable failure reporting to the third-person camera.
- [ ] **IS-16-042 P1** — Add focused unit tests for the third-person camera.
- [ ] **IS-16-043 P1** — Add an integration scenario that exercises the third-person camera in a running game flow.
- [ ] **IS-16-044 P1** — Define save/checkpoint serialization and restoration for camera settings.
- [ ] **IS-16-045 P2** — Add logging and debug inspection for the third-person camera.
- [ ] **IS-16-046 P2** — Document usage examples and common failure modes for the third-person camera.

## Camera collision

- [ ] **IS-16-047 P1** — Define the scope, public API, and versioned config for camera collision.
- [ ] **IS-16-048 P1** — Implement the smallest deterministic reference path for camera collision.
- [ ] **IS-16-049 P1** — Add input validation and actionable failure reporting to camera collision.
- [ ] **IS-16-050 P1** — Add focused unit tests for camera collision.
- [ ] **IS-16-051 P1** — Add an integration scenario that exercises camera collision in a running game flow.
- [ ] **IS-16-052 P2** — Add logging and debug inspection for camera collision.
- [ ] **IS-16-053 P2** — Document usage examples and common failure modes for camera collision.

## Interaction targeting

- [ ] **IS-16-054 P1** — Define the scope, public API, and versioned config for interaction targeting.
- [ ] **IS-16-055 P1** — Implement the smallest deterministic reference path for interaction targeting.
- [ ] **IS-16-056 P1** — Add input validation and actionable failure reporting to interaction targeting.
- [ ] **IS-16-057 P1** — Add focused unit tests for interaction targeting.
- [ ] **IS-16-058 P1** — Add an integration scenario that exercises interaction targeting in a running game flow.
- [ ] **IS-16-059 P2** — Add logging and debug inspection for interaction targeting.
- [ ] **IS-16-060 P2** — Document usage examples and common failure modes for interaction targeting.

## Vehicle enter/exit flow

- [ ] **IS-16-061 P1** — Define the scope, public API, and versioned config for the vehicle enter/exit flow.
- [ ] **IS-16-062 P1** — Implement the smallest deterministic reference path for the vehicle enter/exit flow.
- [ ] **IS-16-063 P1** — Add input validation and actionable failure reporting to the vehicle enter/exit flow.
- [ ] **IS-16-064 P1** — Add focused unit tests for the vehicle enter/exit flow.
- [ ] **IS-16-065 P1** — Add an integration scenario that exercises the vehicle enter/exit flow in a running game flow.
- [ ] **IS-16-066 P1** — Define save/checkpoint serialization and restoration for the vehicle enter/exit flow.
- [ ] **IS-16-067 P2** — Add logging and debug inspection for the vehicle enter/exit flow.
- [ ] **IS-16-068 P2** — Document usage examples and common failure modes for the vehicle enter/exit flow.

## Gameplay mode manager

Merges interaction-mode management, input-context routing, and the player state machine into one coherent system — these overlap heavily (all answer "what mode is the player in, and how does that route input").

- [ ] **IS-16-069 P1** — Define the scope, public API, and versioned config for the gameplay mode manager (on-foot, in-vehicle, dialogue, cutscene, menu).
- [ ] **IS-16-070 P1** — Implement the smallest deterministic reference path for mode-based input routing.
- [ ] **IS-16-071 P1** — Add input validation and actionable failure reporting to the mode manager.
- [ ] **IS-16-072 P1** — Add focused unit tests for the mode manager, including mode-transition edge cases.
- [ ] **IS-16-073 P1** — Add an integration scenario covering on-foot -> vehicle -> dialogue -> on-foot transitions.
- [ ] **IS-16-074 P1** — Define save/checkpoint serialization and restoration for the current gameplay mode.
- [ ] **IS-16-075 P2** — Add logging and debug inspection for the mode manager.
- [ ] **IS-16-076 P2** — Document usage examples and common failure modes for the mode manager.

## Camera polish

- [ ] **IS-16-077 P2** — Add camera zone transitions for interiors and narrow spaces.
- [ ] **IS-16-078 P2** — Add camera shake channels with accessibility scaling.
- [ ] **IS-16-079 P2** — Add focused unit tests for camera zone/shake behavior.
- [ ] **IS-16-080 P2** — Document camera zone/shake usage and common failure modes.
