# 16. Player controller and cameras

[Back to master plan](../plan.md)

Evolve the existing debug controller (on-foot movement with simple collision already works, see `src/Gameplay/PlayerController.cpp`) into a production third-person character controller and camera. Photo mode is cut to a single someday task.

- [x] **IG-16-001 P0** — Run and tune the existing on-foot controller in the integrated build.
- [ ] **IG-16-002 P0** — Replace circle/AABB collision with the production character controller.
- [x] **IG-16-003 P0** — Create camera collision and obstruction handling. *(`ResolveCameraObstruction()` (`include/`/`src/Gameplay/CameraCollision.hpp/.cpp`): the segment from the look-at target to the desired camera position is tested against the district's collidable `WorldBox`es and the camera pulled in to the first hit, minus a 0.35 m skin because a camera exactly on a surface still clips through it. `SegmentIntersectsBox()` gained an overload reporting the entry fraction -- the slab method already computed it. Non-collidable boxes are ignored, the same rule `HasLineOfSight()` uses: pulling the camera in to a lane marking would break the camera everywhere on a road. Applied to the on-foot and driving cameras, **not** to cutscenes, whose keyframes are authored shots. The minimum standoff is a **distance** (0.6 m), not a fraction of the boom: as a fraction it scales with the 7.5 m boom and pushed the camera back through a wall the player stood a metre in front of -- caught by the real-district test, not by review. Not done: no easing, so the camera snaps rather than glides; no occlusion fade for thin geometry; and this is a segment test, not a swept sphere, so a corner can still clip the near plane.)*
- [x] **IG-16-004 P0** — Create interaction prompts for the car and mission objects. *(`InteractionPromptSelector` (`include/`/`src/UI/InteractionPrompt.hpp/.cpp`) picks the nearest available target in XZ range and formats `[E] Enter the sedan`, with the key read from `InputBindings` so a rebind changes every prompt without touching content, and `unbound` rather than empty brackets when the action has no key. Hysteresis (1.35x radius) keeps the prompt on the target already being offered, so standing between two does not make them trade it every frame. `IronGangGame::CurrentInteractionPrompt()` offers `Enter the sedan` on foot within the same 3 m `HandleInteraction()` itself uses -- a prompt that lies about its own range is worse than none -- and `Leave the sedan` while driving; every other input context clears the sticky target so a prompt cannot resume on a stale one. Drawn centred on a dimmed pill, above the subtitle when one is showing. **Mission objects deliberately excluded**: the district exit and the warehouse goal are walk-/drive-into triggers, not press-E objects, and offering a key for them would be a prompt that does nothing. Not done: no world-space anchoring (the prompt is centred on screen, not drawn at the object), and no fade in/out.)*
- [ ] **IG-16-005 P1** — Add acceleration, deceleration, turning inertia, and slope handling. *(Partial: acceleration, deceleration, and turning inertia are done. `Locomotion` (`include/IronGang/Gameplay/Locomotion.hpp`) eases the character's forward/strafe velocity and turn rate toward what the input asks for, instead of the previous behaviour where a keypress **was** full speed and a release **was** a dead stop -- that read as a cursor, not a person. Stopping (26 m/s^2) is quicker than starting (18 m/s^2) on purpose: people lean into a walk and plant their feet to halt, and a character who stops faster than he starts feels responsive rather than sluggish. Deceleration is chosen per axis, so releasing forward while still strafing does not brake the strafe; diagonal input is clamped so it is not faster than straight input; a teleport (`Reset`/`SetPosition`) drops momentum. Pure arithmetic with no physics in it, so all of it is unit-tested (`TestLocomotionAcceleratesAndDecelerates`). **Slope handling is not done** -- Jolt's `CharacterVirtual` resolves slopes as geometry, but nothing changes speed uphill or downhill, and there is no slide threshold (IG-16-006's ground/falling states are the natural place for it).)*
- [ ] **IG-16-006 P1** — Add step-up, ledge, ground, and falling states.
- [ ] **IG-16-007 P1** — Add crouch only if required by mission design.
- [ ] **IG-16-008 P1** — Add sprint stamina only if it contributes to gameplay.
- [ ] **IG-16-009 P1** — Add context-sensitive interaction targeting.
- [ ] **IG-16-010 P1** — Add doors, switches, pickups, seats, and talk interactions.
- [ ] **IG-16-011 P1** — Add camera shoulder selection and collision fade rules.
- [ ] **IG-16-012 P1** — Add camera zones for interiors and narrow spaces.
- [ ] **IG-16-013 P1** — Add vehicle entry/exit alignment and safe-position search.
- [ ] **IG-16-014 P1** — Add control locking through explicit gameplay modes.
- [ ] **IG-16-015 P1** — Add motion/animation synchronization.
- [ ] **IG-16-016 P1** — Add footstep event and surface lookup.
- [ ] **IG-16-017 P1** — Add player state restoration at checkpoints.
- [ ] **IG-16-018 P1** — Add gamepad input and vibration.
- [ ] **IG-16-019 P1** — Add input buffering where animation gates interactions.
- [ ] **IG-16-020 P3** — Add a debug free-camera mode (photo mode is not otherwise planned).
- [ ] **IG-16-021 P2** — Add first-person inspection only if campaign design requires it.
- [ ] **IG-16-022 P2** — Add cinematic camera handoff blending.

## On-foot locomotion

- [ ] **IG-16-023 P1** — Define the scope, public API, and versioned config for on-foot locomotion.
- [ ] **IG-16-024 P1** — Implement the smallest deterministic reference path for production on-foot locomotion.
- [ ] **IG-16-025 P1** — Add input validation and actionable failure reporting to on-foot locomotion.
- [ ] **IG-16-026 P1** — Add focused unit tests for on-foot locomotion.
- [ ] **IG-16-027 P1** — Add an integration scenario that exercises on-foot locomotion in a running game flow.
- [ ] **IG-16-028 P1** — Define save/checkpoint serialization and restoration for on-foot locomotion.
- [ ] **IG-16-029 P2** — Add logging and debug inspection for on-foot locomotion.
- [ ] **IG-16-030 P2** — Document usage examples and common failure modes for on-foot locomotion.

## Character collision controller

- [ ] **IG-16-031 P1** — Define the scope, public API, and versioned config for the character collision controller.
- [ ] **IG-16-032 P1** — Implement the smallest deterministic reference path for the character collision controller (reuse cna-extended's 3D collision where it fits).
- [ ] **IG-16-033 P1** — Add input validation and actionable failure reporting to the character collision controller.
- [ ] **IG-16-034 P1** — Add focused unit tests for the character collision controller.
- [ ] **IG-16-035 P1** — Add an integration scenario that exercises the character collision controller in a running game flow.
- [ ] **IG-16-036 P1** — Define save/checkpoint serialization and restoration for the character collision controller.
- [ ] **IG-16-037 P2** — Add logging and debug inspection for the character collision controller.
- [ ] **IG-16-038 P2** — Document usage examples and common failure modes for the character collision controller.

## Third-person camera

- [ ] **IG-16-039 P1** — Define the scope, public API, and versioned config for the third-person camera.
- [ ] **IG-16-040 P1** — Implement the smallest deterministic reference path for the third-person camera.
- [ ] **IG-16-041 P1** — Add input validation and actionable failure reporting to the third-person camera.
- [ ] **IG-16-042 P1** — Add focused unit tests for the third-person camera.
- [ ] **IG-16-043 P1** — Add an integration scenario that exercises the third-person camera in a running game flow.
- [ ] **IG-16-044 P1** — Define save/checkpoint serialization and restoration for camera settings.
- [ ] **IG-16-045 P2** — Add logging and debug inspection for the third-person camera.
- [ ] **IG-16-046 P2** — Document usage examples and common failure modes for the third-person camera.

## Camera collision

- [ ] **IG-16-047 P1** — Define the scope, public API, and versioned config for camera collision.
- [ ] **IG-16-048 P1** — Implement the smallest deterministic reference path for camera collision.
- [ ] **IG-16-049 P1** — Add input validation and actionable failure reporting to camera collision.
- [ ] **IG-16-050 P1** — Add focused unit tests for camera collision.
- [ ] **IG-16-051 P1** — Add an integration scenario that exercises camera collision in a running game flow.
- [ ] **IG-16-052 P2** — Add logging and debug inspection for camera collision.
- [ ] **IG-16-053 P2** — Document usage examples and common failure modes for camera collision.

## Interaction targeting

- [ ] **IG-16-054 P1** — Define the scope, public API, and versioned config for interaction targeting.
- [ ] **IG-16-055 P1** — Implement the smallest deterministic reference path for interaction targeting.
- [ ] **IG-16-056 P1** — Add input validation and actionable failure reporting to interaction targeting.
- [ ] **IG-16-057 P1** — Add focused unit tests for interaction targeting.
- [ ] **IG-16-058 P1** — Add an integration scenario that exercises interaction targeting in a running game flow.
- [ ] **IG-16-059 P2** — Add logging and debug inspection for interaction targeting.
- [ ] **IG-16-060 P2** — Document usage examples and common failure modes for interaction targeting.

## Vehicle enter/exit flow

- [ ] **IG-16-061 P1** — Define the scope, public API, and versioned config for the vehicle enter/exit flow.
- [ ] **IG-16-062 P1** — Implement the smallest deterministic reference path for the vehicle enter/exit flow.
- [ ] **IG-16-063 P1** — Add input validation and actionable failure reporting to the vehicle enter/exit flow.
- [ ] **IG-16-064 P1** — Add focused unit tests for the vehicle enter/exit flow.
- [ ] **IG-16-065 P1** — Add an integration scenario that exercises the vehicle enter/exit flow in a running game flow.
- [ ] **IG-16-066 P1** — Define save/checkpoint serialization and restoration for the vehicle enter/exit flow.
- [ ] **IG-16-067 P2** — Add logging and debug inspection for the vehicle enter/exit flow.
- [ ] **IG-16-068 P2** — Document usage examples and common failure modes for the vehicle enter/exit flow.

## Gameplay mode manager

Merges interaction-mode management, input-context routing, and the player state machine into one coherent system — these overlap heavily (all answer "what mode is the player in, and how does that route input").

- [ ] **IG-16-069 P1** — Define the scope, public API, and versioned config for the gameplay mode manager (on-foot, in-vehicle, dialogue, cutscene, menu).
- [ ] **IG-16-070 P1** — Implement the smallest deterministic reference path for mode-based input routing.
- [ ] **IG-16-071 P1** — Add input validation and actionable failure reporting to the mode manager.
- [ ] **IG-16-072 P1** — Add focused unit tests for the mode manager, including mode-transition edge cases.
- [ ] **IG-16-073 P1** — Add an integration scenario covering on-foot -> vehicle -> dialogue -> on-foot transitions.
- [ ] **IG-16-074 P1** — Define save/checkpoint serialization and restoration for the current gameplay mode.
- [ ] **IG-16-075 P2** — Add logging and debug inspection for the mode manager.
- [ ] **IG-16-076 P2** — Document usage examples and common failure modes for the mode manager.

## Camera polish

- [ ] **IG-16-077 P2** — Add camera zone transitions for interiors and narrow spaces.
- [ ] **IG-16-078 P2** — Add camera shake channels with accessibility scaling.
- [ ] **IG-16-079 P2** — Add focused unit tests for camera zone/shake behavior.
- [ ] **IG-16-080 P2** — Document camera zone/shake usage and common failure modes.
