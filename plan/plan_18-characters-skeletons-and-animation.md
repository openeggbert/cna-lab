# 18. Characters, skeletons, and animation

[Back to master plan](../plan.md)

Create a reusable character pipeline and a production animation system sized for one shared skeleton across a campaign-sized character roster (see group 32). Full facial/jaw animation, heavy animation-LOD/update-throttling systems, and per-backend validation (EasyGL is the only real target — see group 05) are cut down to a single someday note each.

- [ ] **IS-18-001 P0** — Define one standard human skeleton and naming convention. *(Not started at the "standard for the whole campaign" level; the gate M6 test character used a minimal, ad hoc 3-bone rig (Root/LeftLeg/RightLeg) sized only to prove the pipeline, not a reusable named-bone convention for future content authors.)*
- [x] **IS-18-002 P0** — Import one skinned character through glTF/CNJ and render it with CNA. *(Gate M6: `assets/source/gltf/test_character.gltf` -- hand-authored, since Mesh Craft/MC3 has no rigging/skinning authoring support -- converted via `cna_tool_gltf_to_cnj` to `assets/generated/models/cnj/test_character.cnj`, loaded via `Content.Load<Model>` in `IronShadowsGame::Initialize`, and drawn by `PrototypeRenderer::Draw` in place of the procedural player box. See `plan_39` gate M6 for the full verification record.)*
- [ ] **IS-18-003 P0** — Play idle, walk, run, and driving clips. *(Partial: "Idle" and "Walk" clips exist and are selected from `PlayerController`'s current input in `IronShadowsGame::Update`; no separate "run"/sprint clip (sprint only changes movement speed, not animation) and no driving/vehicle-seated clip -- the character is not drawn at all while `playerDriving_` is true, matching the existing procedural-box behavior it replaced.)*
- [ ] **IS-18-004 P0** — Implement clip blending and state transitions. *(Not done: `ModelAnimationSystem3DEXT` (cna-extended) only supports a hard cut between clips by design -- see that system's own header comment. Blending is real follow-up work in cna-extended, not something this content/integration pass could add on its own.)*
- [x] **IS-18-005 P0** — Connect locomotion speed/direction to animation selection. *(Binary, not speed-graded: `IronShadowsGame::Update` selects "Walk" when `OnFootInput::forward`/`strafe` are non-zero, "Idle" otherwise. No blend between them and no distinct sprint-speed clip -- see IS-18-003/004.)*
- [ ] **IS-18-006 P1** — Create animation assets for enter car, exit car, sit, drive, and steer.
- [ ] **IS-18-007 P1** — Create bone masks and upper/lower-body layers.
- [ ] **IS-18-008 P1** — Create additive look, breathing, recoil, and gesture layers.
- [ ] **IS-18-009 P1** — Create root-motion extraction and policy.
- [ ] **IS-18-010 P1** — Create animation events/notifies for footsteps, impacts, sounds, and mission hooks.
- [ ] **IS-18-011 P1** — Create retargeting validation for AI-assisted/AI-generated characters.
- [ ] **IS-18-012 P1** — Create foot IK and ground alignment.
- [ ] **IS-18-013 P1** — Create hand IK for steering wheels and props.
- [ ] **IS-18-014 P1** — Create head/eye look-at.
- [ ] **IS-18-015 P1** — Create seat and vehicle mount alignment.
- [ ] **IS-18-016 P1** — Create transition interruption rules.
- [ ] **IS-18-017 P1** — Create deterministic animation state checkpoint restoration.
- [ ] **IS-18-018 P1** — Create skeleton/clip compatibility checks in the asset build.
- [ ] **IS-18-019 P1** — Create a text/overlay animation-graph debug view (no GUI editor).
- [ ] **IS-18-020 P2** — Create simple jaw/flap animation from dialogue timing (full facial animation is out of scope).
- [ ] **IS-18-021 P2** — Create basic animation LOD (reduced update rate for distant characters) only if profiling shows it is needed; no full animation-LOD framework.
- [ ] **IS-18-022 P2** — Create ragdoll blend-in/blend-out after physics integration.
- [ ] **IS-18-023 P2** — Create crowd animation sharing/caching for the small nearby-pedestrian count established in group 20.
- [ ] **IS-18-024 P2** — Create procedural pose correction for varying vehicle interiors.

## Skeleton convention

- [ ] **IS-18-025 P1** — Define the scope, public API, and versioned config for the skeleton convention.
- [ ] **IS-18-026 P1** — Implement the smallest deterministic reference path for the skeleton convention.
- [ ] **IS-18-027 P1** — Add input validation and actionable failure reporting to the skeleton convention.
- [ ] **IS-18-028 P1** — Add focused unit tests for the skeleton convention.
- [ ] **IS-18-029 P1** — Add an integration scenario that exercises the skeleton convention in a running game flow.
- [ ] **IS-18-030 P1** — Define save/checkpoint serialization and restoration for skeleton-dependent state.
- [ ] **IS-18-031 P2** — Document usage examples and common failure modes for the skeleton convention.

## Animation clip player

- [x] **IS-18-032 P1** — Define the scope, public API, and versioned config for the animation clip player. *(Scope: reuse cna's own `Microsoft::Xna::Framework::Graphics::AnimationPlayer`/`SkinningData` (the classic XNA "Skinned Model Sample" pattern) rather than build a new one -- see cna-extended's new `ModelAnimationComponentEXT`. No versioned config file; the clip player's state is plain C++ fields (`ClipNameEXT`, `LoopEXT`).)*
- [x] **IS-18-033 P1** — Implement the smallest deterministic reference path for the animation clip player. *(`CNA::Extended::World3DEXT::ModelAnimationSystem3DEXT` (in the `cna-extended` sibling repo, added for this gate): starts `ClipNameEXT` from the beginning when it names a different clip than the one currently playing, otherwise advances `AnimationPlayer::Update()` -- a hard cut, see IS-18-004.)*
- [ ] **IS-18-034 P1** — Add input validation and actionable failure reporting to the animation clip player. *(Partial: an unknown `ClipNameEXT` is silently ignored (holds the last pose) rather than reported -- acceptable for a hardcoded "Idle"/"Walk" pair today, but would hide a real bug once clip names come from data.)*
- [x] **IS-18-035 P1** — Add focused unit tests for the animation clip player. *(`cna-extended`'s `ModelAnimationSystem3DEXTTests.cpp`: clip start/switch/hard-cut/unknown-name behavior, verified against `AnimationPlayer`'s real bone-transform math using a proven-good glTF fixture, not mocked.)*
- [x] **IS-18-036 P1** — Add an integration scenario that exercises the animation clip player in a running game flow. *(`IronShadowsGame::Update` selects "Walk"/"Idle" from live `OnFootInput` each frame and calls `PrototypeRenderer::UpdateCharacterAnimation`, which ticks the real `ModelAnimationSystem3DEXT`/`ModelAnimationComponentEXT` pair against the real `test_character.cnj` asset; verified via `--smoke` runs with no crash. Not verified: an interactive session actually pressing a movement key and watching the clip switch, since this environment has no display -- see `NEXT.md`.)*
- [ ] **IS-18-037 P1** — Define save/checkpoint serialization and restoration for clip-player state. *(Not done: `SaveGame`/`SaveSnapshot` do not persist the current animation clip/position -- acceptable today since it is fully re-derived from `OnFootInput` every frame, but would need real handling once clips stop being purely input-driven, e.g. a one-shot vehicle-entry animation mid-playback at save time.)*
- [ ] **IS-18-038 P2** — Document usage examples and common failure modes for the animation clip player. *(Covered by `ModelAnimationComponentEXT.hpp`/`ModelAnimationSystem3DEXT.hpp`'s own header comments in `cna-extended`, not a separate doc page.)*

## Animation state machine

- [ ] **IS-18-039 P1** — Define the scope, public API, and versioned config for the animation state machine.
- [ ] **IS-18-040 P1** — Implement the smallest deterministic reference path for the animation state machine.
- [ ] **IS-18-041 P1** — Add input validation and actionable failure reporting to the animation state machine.
- [ ] **IS-18-042 P1** — Add focused unit tests for the animation state machine.
- [ ] **IS-18-043 P1** — Add an integration scenario that exercises the animation state machine in a running game flow.
- [ ] **IS-18-044 P1** — Define save/checkpoint serialization and restoration for animation-state-machine state.
- [ ] **IS-18-045 P2** — Add a text/overlay debug view for the current animation state.
- [ ] **IS-18-046 P2** — Document usage examples and common failure modes for the animation state machine.

## Animation blend layers

- [ ] **IS-18-047 P1** — Define the scope, public API, and versioned config for animation blend layers.
- [ ] **IS-18-048 P1** — Implement the smallest deterministic reference path for animation blend layers.
- [ ] **IS-18-049 P1** — Add input validation and actionable failure reporting to animation blend layers.
- [ ] **IS-18-050 P1** — Add focused unit tests for animation blend layers.
- [ ] **IS-18-051 P1** — Add an integration scenario that exercises animation blend layers in a running game flow.
- [ ] **IS-18-052 P1** — Define save/checkpoint serialization and restoration for blend-layer state.
- [ ] **IS-18-053 P2** — Add a text/overlay debug view for active blend layers.
- [ ] **IS-18-054 P2** — Document usage examples and common failure modes for animation blend layers.

## Root motion

- [ ] **IS-18-055 P1** — Define the scope, public API, and versioned config for root motion.
- [ ] **IS-18-056 P1** — Implement the smallest deterministic reference path for root motion.
- [ ] **IS-18-057 P1** — Add input validation and actionable failure reporting to root motion.
- [ ] **IS-18-058 P1** — Add focused unit tests for root motion.
- [ ] **IS-18-059 P1** — Add an integration scenario that exercises root motion in a running game flow.
- [ ] **IS-18-060 P1** — Define save/checkpoint serialization and restoration for root-motion state.
- [ ] **IS-18-061 P2** — Document usage examples and common failure modes for root motion.

## Animation notifies and events

Merges the standalone "animation event router" into the notifies system — they are the same concept (fire an event at a point in a clip).

- [ ] **IS-18-062 P1** — Define the scope, public API, and versioned config for animation notifies/events.
- [ ] **IS-18-063 P1** — Implement the smallest deterministic reference path for animation notifies/events.
- [ ] **IS-18-064 P1** — Add input validation and actionable failure reporting to animation notifies/events.
- [ ] **IS-18-065 P1** — Add focused unit tests for animation notifies/events.
- [ ] **IS-18-066 P1** — Add an integration scenario that exercises animation notifies/events (footsteps, mission hooks) in a running game flow.
- [ ] **IS-18-067 P1** — Define save/checkpoint serialization and restoration for pending notify state.
- [ ] **IS-18-068 P2** — Add a text/overlay debug view for fired notifies/events.
- [ ] **IS-18-069 P2** — Document usage examples and common failure modes for animation notifies/events.

## Foot IK

- [ ] **IS-18-070 P1** — Define the scope, public API, and versioned config for foot IK.
- [ ] **IS-18-071 P1** — Implement the smallest deterministic reference path for foot IK and ground alignment.
- [ ] **IS-18-072 P1** — Add focused unit tests for foot IK.
- [ ] **IS-18-073 P1** — Add an integration scenario that exercises foot IK on uneven ground.
- [ ] **IS-18-074 P1** — Define save/checkpoint serialization and restoration for foot IK state.
- [ ] **IS-18-075 P2** — Document usage examples and common failure modes for foot IK.

## Hand IK

- [ ] **IS-18-076 P1** — Define the scope, public API, and versioned config for hand IK.
- [ ] **IS-18-077 P1** — Implement the smallest deterministic reference path for hand IK on the steering wheel.
- [ ] **IS-18-078 P1** — Add focused unit tests for hand IK.
- [ ] **IS-18-079 P1** — Add an integration scenario that exercises hand IK while driving.
- [ ] **IS-18-080 P2** — Document usage examples and common failure modes for hand IK.

## Look-at controller

- [ ] **IS-18-081 P1** — Define the scope, public API, and versioned config for the head/eye look-at controller.
- [ ] **IS-18-082 P1** — Implement the smallest deterministic reference path for the look-at controller.
- [ ] **IS-18-083 P1** — Add focused unit tests for the look-at controller.
- [ ] **IS-18-084 P1** — Add an integration scenario that exercises look-at during a dialogue encounter.
- [ ] **IS-18-085 P2** — Document usage examples and common failure modes for the look-at controller.

## Seat and vehicle-mount pose system

- [ ] **IS-18-086 P1** — Define the scope, public API, and versioned config for seat/vehicle-mount pose alignment.
- [ ] **IS-18-087 P1** — Implement the smallest deterministic reference path for seat/vehicle-mount pose alignment.
- [ ] **IS-18-088 P1** — Add input validation and actionable failure reporting to the pose system.
- [ ] **IS-18-089 P1** — Add focused unit tests for the pose system.
- [ ] **IS-18-090 P1** — Add an integration scenario that exercises entry/exit and driving pose alignment for the sedan.
- [ ] **IS-18-091 P1** — Define save/checkpoint serialization and restoration for pose-system state.
- [ ] **IS-18-092 P2** — Document usage examples and common failure modes for the pose system.

## Animation retargeter

- [ ] **IS-18-093 P1** — Define the scope and public API for retargeting AI-assisted/AI-generated character rigs onto the standard skeleton.
- [ ] **IS-18-094 P1** — Implement the smallest deterministic reference path for the retargeter.
- [ ] **IS-18-095 P1** — Add input validation and actionable failure reporting to the retargeter.
- [ ] **IS-18-096 P1** — Add focused unit tests for the retargeter.
- [ ] **IS-18-097 P1** — Add an integration scenario retargeting one new character onto the standard skeleton.
- [ ] **IS-18-098 P2** — Document usage examples and common failure modes for the retargeter.
