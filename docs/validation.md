# Validation record

## M12 profiling baseline (2026-08-24)

The `dev-easygl` and `release-easygl` presets now both configure, compile, launch, and render on
this workspace's host display (OpenGL ES 3.2 Mesa through CNA EasyGL). Iron Gang now has a bounded
`--profile <json>` capture and a deterministic `--profile-scenario mixed` workload covering walk,
drive, ambient AI, physics, audio control, and a real district transition. Unit tests cover p95/
average/maximum calculation and report policy; the real EasyGL runs validate the integrated path.

The first Release baseline does **not** close M12. Intro/idle passes the 30 FPS minimum at 16.876 ms
frame p95, but mixed movement/load captures reproduce a 51.628-57.705 ms p95 failure even though
all measured CPU subsystem p95 values are far inside budget. RAM passes at ~220 MiB and district
load passes at ~6 ms. Two final identical 180-frame intro runs also flipped back-to-back from
51.381 ms to 16.897 ms p95 with virtually unchanged render CPU, exposing independent host/display
frame-pacing instability. VRAM reporting is deliberately marked incomplete. Full evidence and the
next diagnostic question are in `docs/performance-baseline.md`.

Follow-up instrumentation adds isolated `intro`/`idle`/`walk`/`drive`/`mixed` scenarios,
`--vsync on|off`, requested scheduler/presentation metadata, and direct `present_cpu` timing around
CNA's `Game::EndDraw()`. The final graphical integration used an isolated Xvfb `:99` with
`WAYLAND_DISPLAY` removed and `SDL_VIDEODRIVER=x11` forced, preventing a visible Wayland window.
Mesa llvmpipe completed paired 540-frame full-mixed runs at 17.087 ms p95 (v-sync requested off)
and 16.898 ms p95 (requested on), each with two district-load samples. Present p95 was 11.604 ms
and 13.220 ms respectively. Xvfb has no real vblank and is unaccelerated, so these are diagnostic
integration results rather than hardware qualification; M12 remains open.

VRAM visibility was then extended without changing CNA: `VideoMemoryAccumulator` walks loaded CNJ
meshes, deduplicates their vertex/index buffers and built-in/generic-effect textures by identity,
and computes logical texture storage including mips and block compression. Unit tests cover exact
uncompressed, DXT, cube, and 3D texture-size calculations plus JSON category output. A Release
EasyGL 12-frame idle integration run on the same isolated Xvfb/X11 path reported 394,340 tracked
bytes: 386,168 game-owned, 8,160 imported model buffers, and 12 imported model textures. This is a
stronger lower bound, not full residency; backend programs, swapchain/depth/render targets,
transients, driver padding, and physical residency remain unknown, so `tracking_complete=false`
and M12 stays open.

The next M12 follow-up added a non-blocking GPU command-range timer without enabling CNA's entire
77-source GraphicsExt module: `GpuFrameTimer` uses the same renderer timer-query contract, guards
one pending query from overwrite, and never substitutes a CPU clock. JSON tests cover support
metadata, escaped unsupported reasons, discarded-result counts, and the `gpu_render` metric. A
full 540-frame Release EasyGL mixed run on isolated Xvfb/X11 produced 538 valid samples with GPU
p95 7.786 ms, Present CPU p95 12.189 ms, and frame p95 16.918 ms; a separate 120-frame idle run
reported GPU/frame p95 9.150/17.091 ms. One 32-bit all-ones EasyGL/metagl timer sentinel in each
run was explicitly counted and discarded rather than reported as a false 4.295-second GPU frame.
Release/development EasyGL, software plus all CTest targets, strict syntax checks, Emscripten/Web,
and the full mixed runtime flow pass. This proves the reporting path; llvmpipe is still not
qualifying hardware.

`IG-35-005` then added scoped render-workload counts to JSON schema 2. Unit coverage checks exact
nearest-rank statistics and scope metadata. A 540-frame Release EasyGL mixed run, again only on
isolated Xvfb/X11, sampled every frame and reported p95/max of 18 draw calls, 56 explicit
state-change calls, 1,768 declared vertices, 948 triangles, 16 geometry instances, and 67 submitted
visible objects. Transition frames reset to zero instead of leaking the previous frame's counts.
The report explicitly excludes HUD backend batching and driver state deduplication; “visible” means
submitted because no culling path exists yet. Software/CTest, syntax, Release/development EasyGL,
Web/Emscripten, and the mixed runtime flow all pass.

The next presentation diagnostic uses CNA's public platform GL-context acknowledgement rather than
linking Iron Gang directly to SDL. JSON schema 3 distinguishes requested/applied/unknown and warns
that platform acceptance is not physical-vblank proof. Two 60-frame Release EasyGL idle runs on
isolated Xvfb requested intervals 0 and 1; the platform rejected both (`apply_succeeded=false`,
`applied=null`). Their frame p95 values, 17.049/16.950 ms, are therefore correctly treated as an
invalid v-sync comparison instead of evidence that the settings are equivalent.

The district-load follow-up adds schema-4 per-transition phase, target-object, and memory-delta
evidence. Unit tests verify exact phase aggregation, counts, negative RSS delta and positive tracked
video-memory delta output. One isolated 540-frame Release EasyGL `mixed` run captured exactly one
WarehouseBlock -> Countryside transition: 0.045 ms world/static-physics activation + 0.219 ms
renderer rebuild/upload submission = 0.264 ms total, 25 procedural world objects, 5 target static
bodies, 0 B RSS delta, and -135,576 B tracked logical renderer-memory delta. District I/O,
decompression, and parse are correctly `null`: runtime districts are generated in memory and read
no package. Software plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and
the isolated real flow pass. Initialization is now solely `startup_cpu`, correcting the older
district p95 that accidentally included broad startup work.

The physics-profiler follow-up adds JSON schema 5 and an opt-in Jolt-seam snapshot. A deterministic
physics test proves exact body, fixed-step, public-raycast, character-update, and four-wheel-raycast
counts; it also verifies operation counters are consumed while current body state is retained.
Actual rigid-body and CharacterVirtual contacts are covered separately. One isolated 540-frame
Release EasyGL `mixed` run sampled 542 updates, reaching 9 bodies, 1 active rigid body, 2 character
contacts, 1 fixed step, 1 character collision update, and 4 wheel rays per update at p95/max where
applicable; physics CPU was 0.206 ms p95. Software plus 3/3 CTest, strict syntax, Release/development
EasyGL, Web/Emscripten, and the isolated real flow pass. Ordinary play keeps profiling disabled and
does not take the contact-counter mutex.

The ambient-AI follow-up adds schema 6 with per-update current traffic/pedestrian/fleeing/patrol
counts and exact traffic-update/obstacle, pedestrian-update/threat, police-witness/patrol loop
counts. `ai_cpu` now excludes the adjacent mission update so its scope matches the documented
ambient-AI budget. Unit coverage proves statistics/JSON and direct police instrumentation,
including two patrol iterations on the escalation tick. An isolated 540-frame Release EasyGL
`mixed` run sampled 543 updates: AI CPU 0.007 ms p95, with p95/max 2 traffic vehicles, 2
pedestrians, 4 obstacle checks, 2 threat checks, and 4 witness checks. This route correctly had no
fleeing pedestrian or active police patrol; the deterministic police test covers their nonzero
work. No road-graph/path-request numbers are invented while only fixed WaypointPaths exist.

The audio follow-up adds schema 7 with exact game-owned loaded-asset, retained-loop state, streamed-
asset, one-shot start, loop-control, and parameter-update counts. Unit coverage proves nearest-rank
statistics and the report's explicit observability boundary. An isolated 540-frame Release EasyGL
`mixed` run with dummy SDL audio sampled 544 updates: audio-control CPU was 0.019 ms p95, all four
one-shot footstep requests succeeded, and the one retained engine loop started and reached playing
state. CNA exposes neither fire-and-forget voice lifetime nor decoder/mixer callback, active backend
channel, or bus costs, so the report marks those unavailable instead of inventing zeroes. Software
plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and the isolated real flow
pass; no visible host display was used.

The frame-pacing follow-up adds schema 8 with five mutually exclusive histogram buckets, strict
>33.333 ms minimum-budget misses, >50 ms hitches, >100 ms severe hitches, and first-frame-after-
district-transition association. Unit coverage proves exact-threshold inclusivity and transition
indexing. An isolated 540-frame Release EasyGL `mixed` run sampled 539 intervals at 16.988 ms p95
and found one 55.936 ms hitch (0.186%), no severe hitch, and a non-hitch 17.345 ms transition
boundary. Software plus 3/3 CTest, strict syntax, Release/development EasyGL, Web/Emscripten, and
the isolated real flow pass. The result validates the detector but does not qualify llvmpipe/Xvfb
as physical target hardware or diagnose the one hitch; no visible host display was used.

The release-summary follow-up adds `scripts/performance_report.py`, a standard-library schema-8
Markdown generator that independently evaluates raw measurements against locked targets. It
requires two mixed captures with distinct canonical contents, explicit physical hardware identity,
Release OPENGLES3, acknowledged presentation, complete VRAM, and all direct budgets before emitting `PASS`; absent
qualification remains `DIAGNOSTIC`, while a declared qualification with verified archives but
measured blockers is `FAIL`. Missing or unverifiable archive sources are invalid input (exit 2), as
recorded by the later archive-enforcement follow-up. A new fourth CTest covers diagnostic Xvfb, a
synthetic two-capture pass, duplicate-copy refusal, swap/VRAM failures, stale schemas, and histogram
mismatch. The real isolated capture correctly remains
diagnostic with one-run, virtual-display, rejected-swap, and incomplete-VRAM blockers.

The content-budget follow-up adds versioned `assets/content-budgets.json` plus standard-library
`scripts/content_budget.py`. Exact committed baselines pass: district prototype 96 triangles/5
materials/0 textures, warehouse 12/1/0, grouped sedan 48/4/0, and test character 36/1/1. Bootstrap
triangle ceilings are 4x current geometry; material/texture-count reserves are documented and not
claimed as final production limits. `build-assets.sh` now requires a matching passing budget group
after XSD validation and before conversion. New `iron_gang_content_budget_tests` makes CTest 5/5
and proves real-source integration, actionable overflow, unsupported-MC3 rejection, malformed glTF
triangle rejection, and unregistered-source rejection. Texture resolution remains deliberately
outside this first policy until representative production textures exist; aggregate M12 VRAM
tracking remains authoritative.

The lifecycle-memory follow-up adds no-window `iron_gang_memory_soak_tests`, making CTest 6/6. Its
bounded CI case runs 200 mission-reset/save-read-resume completions and 400 district transitions;
after 20 warm-up cycles, current and peak RSS each grew only 4,096 B, current-RSS slope was 34
B/cycle, and WarehouseBlock returned to exactly 8 Jolt bodies after every round trip. A separate
5,000-cycle local run (10,000 transitions/save replay each cycle) also ended at +4,096 B current/
peak RSS with 0 B/cycle rounded trend and 8 bodies. The test enforces 8 MiB current, 16 MiB peak,
and 32 KiB/cycle trend allowances with a 60-second CTest timeout. It validates core ownership, not
render/audio/backend residency or physical-hardware M12 qualification.

The capture-comparison follow-up adds standard-library `scripts/performance_compare.py` and a
seventh CTest. Six CLI tests cover identical evidence, multiple simultaneous regressions, tolerance
overrides, hardware/kind mismatch, changed budget or GPU-sample availability, and incomplete
qualifying evidence. The latest real schema-8 Xvfb capture self-comparison reports all 15 available
metrics unchanged and exits 0. That is an integrated parser/report check only: meaningful temporal
comparison still requires a later compatible capture from the same named environment, and physical
M12 qualification still requires the separate release-summary rules.

The representative-mission follow-up adds `--profile-scenario mission`. Parser/name tests cover
the public scenario seam. A 120-frame isolated Xvfb negative integration correctly refused an
incomplete report; a 900-frame upper-bound run exited on real mission completion after 647 updates
and produced 642 frame intervals with 16.936 ms p95, one 69.503 ms hitch, no severe hitch, 167.9 MiB
peak RSS, and no accidental district transition. The schema-8 output passes both summary and
comparison tooling. This validates the full production mission route but remains a virtual-display
diagnostic, not physical M12 qualification.

The complete-VRAM follow-up adds `scripts/vram_evidence.py` and an eighth CTest. The binder requires
an original schema-8 profile, a versioned manifest, and the raw vendor/OS capture; SHA-256 binds both
artifacts, exact scope/process/time metadata is validated, inputs cannot be overwritten, and the
enriched total conservatively keeps the larger of logical bytes and external peak residency.
The CLI also reconstructs and verifies a later archived enriched capture without writing any file.
Six tests cover a synthetic report-qualified/verified flow plus semantic tampering, capture hash,
scope/time, raw-artifact, flooring, duplicate JSON keys, and overwrite failures. Report/comparison
tests validate evidence structure, hardware identity, profiler compatibility, and duplicate-key
refusal for generated capture input. No physical profiler artifact exists in this workspace, so the
real Xvfb evidence remains incomplete and this plumbing does not close M12.
The shared evidence validator additionally refuses any process basename other than
`iron_gang`/`iron_gang.exe`, a non-positive PID, and an end time that is not strictly later than the
start; binder and report tests cover the same downstream contract.

The capture-correlation follow-up adds a backward-compatible schema-8 `capture_session` object with
executable, PID-known state/value, and microsecond UTC start/end. Complete VRAM binding now requires
the external artifact manifest to name the same PID and a measurement interval enclosing the whole
game capture. Python fixtures cover PID/interval mismatch; the C++ report test covers emitted
metadata. Software, development/Release EasyGL, Web/Emscripten, strict syntax, and all 8 CTests pass.
A 60-frame Release EasyGL `idle` run on isolated Xvfb emitted a positive 1.198-second session and
remained correctly diagnostic; no host-visible window was used.

The qualification-archive follow-up makes the report itself re-run complete four-file VRAM
verification. Every declared qualifying enriched capture now requires one ordered
`--vram-bundle` containing its original profile, manifest, and raw profiler artifact. Missing
bundles/sources, count mismatches, raw-artifact mutation, semantic enriched-output changes, and an
enriched input changing during verification/parsing exit 2 before evaluation. The report and VRAM
CLI suites both remain 6/6, including a two-independent-bundle synthetic `PASS`; diagnostics remain
readable without bundles. This strengthens provenance but does not supply the still-missing
physical M12 capture.

Report-output preservation then closes the destructive alias case: `--output` is refused when it
equals or hardlinks to any capture/archive input, and valid output uses a same-directory atomic
replace. The seventh report CLI test proves the protected bytes remain identical for capture,
hardlink, and raw-artifact collisions and that a successful nested output leaves no temporary file.

Release summaries now embed exact evidence provenance: evaluated-capture SHA-256, capture PID/UTC
interval, and—when supplied—the verified original/manifest/raw-artifact names and hashes. Inputs
are re-hashed after parsing and again immediately before output. The real stored Xvfb session
renders its known PID/interval and hash
`df217f17b3cf32c3c279fbf582a3075a6bb61f759f9ec3d5d2b695be3da41cd0`; bundle fields correctly
remain absent for that diagnostic. Report 7/7 and VRAM 6/6 focused tests pass.

Presentation evidence is now structurally correlated too. The shared schema-8 loader requires a
0/1 `swap_interval.requested` matching `timing.vertical_sync_requested`; known/success/null states
must be coherent, and successful `applied` must equal the request. Report coverage rejects both
request-1/applied-0 and v-sync/request contradictions; comparator coverage independently reaches
the shared refusal. Report 7/7, comparator 6/6, VRAM 6/6, and both existing real Xvfb diagnostics
pass parsing with their honest rejected-acknowledgement blocker.

Frame-pacing derived fields are now validated against their stored histogram rather than trusted
independently. Fixed bucket bounds, minimum-miss/hitch/severe counts and percentages, comparison
operators, district transition/load sample equality, measured/hitch count bounds, and boundary
maximum/hitch consistency are enforced. Report tests reject a forged hitch count, threshold, and
transition count; both retained Xvfb captures still parse with unchanged diagnostic output.

VRAM archive roles now require three distinct source files/inodes and a non-empty regular raw
profiler artifact. The binder/verifier and report-driven verification all inherit the check.
Expanded VRAM CLI coverage rejects an empty artifact, a raw-artifact hardlink to the original
profile, and an output hardlink to an input while proving source bytes remain unchanged; 6/6 tests
pass.

Capture/evidence times now require canonical `YYYY-MM-DDTHH:MM:SS[.ffffff]Z`. Date-only input, a
space instead of `T`, and seven fractional digits are refused, avoiding Python ISO parsing that
would otherwise accept reduced forms or truncate sub-microsecond evidence boundaries. Report 7/7,
VRAM 6/6, comparator 6/6, and both locally retained real Xvfb captures pass.

The user-requested district-map follow-up adds a real top-down overlay toggled by `Tab`; `M` has no
map binding. It projects the current district's authored `WorldBox` footprints and shows the player,
vehicle, mission target, district exit, north, a legend, and a straight player-to-exit guide. The
guide is not road-aware because no district road graph exists yet. `TestDistrictMapProjection`
proves exact X/Z-to-screen point and footprint mapping. Software build plus 3/3 CTest and strict
syntax pass. Two synthetic `Tab` presses inside isolated Xvfb/X11 were visually captured: the first
showed the complete map and the second restored the unobscured game view. No test used the visible
host display. Both `LeftShift` and `RightShift` remain mapped to sprint only in the on-foot input
branch.

## Current modular dependency baseline (2026-08-22)

Iron Gang now configures against the sibling `../cnanext` and modular `../sharp-runtime`
checkouts. Its own CMake graph names `CNA::GraphicsCore`, `CNA::Runtime`,
`SharpRuntime::IO`, and `SharpRuntime::Text.Json`; CNA is added `EXCLUDE_FROM_ALL`, so its
unrelated Devices, GraphicsExt, C API, examples, and tools are absent from Iron Gang's default
`all` target. The former `cna-extended` animation wrapper was replaced by a small game-owned
state over CNA's `AnimationPlayer`, preserving the existing 0.25-second clip crossfade.

Validated against `cnanext` 0.1.0-alpha.1 and `sharp-runtime` 0.1.0-alpha.1:

- a fresh `compile-software` configure and full build completed successfully;
- the default build graph contains only Iron Gang, its selected CNA/Sharp Runtime module closure,
  and Jolt—not unused CNA devices, extensions, examples, or tools;
- `./scripts/check-syntax.sh` passed all 23 Iron Gang source/test translation units;
- all three CTest targets passed; and
- `iron_gang --smoke 5` loaded both districts' prototype assets, the skinned character and its
  animation clips, textures, and audio, then exited successfully.

The historical entries below describe the milestone state at the time each gate was completed;
references there to the old `../cna`/`cna-extended` graph are not current build instructions.

## Completed for this scaffold

- Every Iron Gang `.cpp` file passed a C++23 syntax-only compile against the actual supplied CNA and sharp-runtime headers with software-backend definitions.
- The prototype MC3 scene passed the supplied Mesh Craft `mc3.xsd`.
- `./scripts/preflight.sh` confirms CNA, sharp-runtime, EasyGL, and cna-extended siblings, plus populated CNA-vendored SDL/SDL_image/SDL_mixer.
- The full `compile-software` preset configured and built (780 targets, `-j4`, ccache), including `cna-extended` linking against the parent-provided `CNA` target as designed.
- `iron_gang_core_tests` linked against the real project sources, CNA, sharp-runtime, and `CNA_EXTENDED`, and all tests (collision, vehicle, mission, dialogue, save round-trip) passed via `ctest --preset compile-software`.
- CMake target and backend names used by Iron Gang were checked against CNA's and cna-extended's current CMake files.
- The full MC3 -> GLB -> CNJ pipeline ran end to end for a real production asset: `assets/source/mc3/warehouse.mc3.xml` validated against `mc3.xsd`, converted via Mesh Craft's `mc3togltf` and CNA's `cna_tool_gltf_to_cnj` (both already built in this workspace), producing `warehouse.cnj` + binary vertex/index sidecars. `./cmake-build-compile-software/iron_gang --smoke 30` loaded it through `Content.Load<Model>()` (confirmed by the `[IronGang] Loaded generated warehouse.cnj` log line), drew it in place of the procedural warehouse box, and exited cleanly; `ctest --preset compile-software` still passes, confirming the mission/collision logic is unaffected.
- The same pipeline was repeated for the sedan as four single-object MC3 files (`vehicle_body`/`vehicle_cabin`/`vehicle_windshield`/`vehicle_wheel`, one MC3 file per part because `cna_tool_gltf_to_cnj` does not bake per-object node transforms -- see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IG-10-004b`), composed by `PrototypeRenderer` with Iron Gang's own per-part transforms; the smoke run logs `[IronGang] Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj` and `ctest` still passes.
- Jolt Physics v5.6.0 (shared checkout at `~/deps/jolt`) was added as a dependency and built successfully as part of the `compile-software` preset (GPU-compute shader options disabled; not needed for CPU rigid-body/character/vehicle physics). `IronGang::Physics::PhysicsWorld` (`src/Physics/PhysicsWorld.cpp`) hides every Jolt type behind a PIMPL boundary. `tests/PhysicsTests.cpp` (`iron_gang_physics_tests`, run via `ctest`) exercises and passes 5 scenarios: a raycast hitting a known static floor, a dynamic box settling under gravity, a trigger volume firing enter/exit events, a capsule character controller stopped by a wall while remaining grounded, and a 4-wheel `VehicleConstraint` vehicle settling onto its suspension and then driving forward under throttle input.
- `PlayerController` and `VehicleController` were migrated to be driven by `PhysicsWorld` instead of the old fixed-height/kinematic math, with world geometry collision coming from real static bodies (`PrototypeWorld::BuildPhysicsStaticBodies`, including a dedicated ground-plane body -- a real bug: the render-only ground box is deliberately `collidable=false` for the unrelated XZ-only `CanOccupy` check, so it was silently never getting a physics body until this was added). `tests/CoreTests.cpp` gained `TestPlayerMotion` (walks forward from spawn, and confirms walking into the hotel's static collider for 5 simulated seconds does not tunnel through it) and an updated `TestVehicleMotion` (confirms the physics-driven vehicle accelerates on the real district's ground plane); both pass via `ctest --preset compile-software`. A standalone diagnostic (not committed) confirmed the sedan settles with all four wheels in ground contact and accelerates smoothly. **Not verified**: actual driving/handling *feel* (acceleration curve, top speed, steering sharpness) and visual alignment of the rendered mesh with the physics capsule/chassis, since this environment has no display or interactive input -- see `NEXT.md`.
- Gate M5 (second district): `IronGang::DistrictManager` (`src/World/DistrictManager.cpp`) was added to own the currently loaded `PrototypeWorld` and its static physics bodies, and a second, genuinely different `Countryside` district was added alongside `WarehouseBlock`, each with its own exit trigger back to the other. `PhysicsWorld::GetBodyCount()` was added (test/diagnostic only, wraps `JPH::PhysicsSystem::GetNumBodies()`) so `tests/CoreTests.cpp`'s new `TestDistrictTransition` could assert that two full round trips (warehouse -> countryside -> warehouse -> countryside) leave the physics body count exactly where it started each time it revisits the same district -- this caught a test-design mistake (asserting equal body counts after a *one-way* swap, which is wrong since the two districts have a different number of static bodies) before it was corrected to only assert equality on round trips. `SaveSnapshot`/`SaveGame` gained a `districtId` field (additive, no save-format version bump) and `TestSaveRoundTrip` now covers it. A full `./scripts/check-syntax.sh` pass and a `./cmake-build-compile-software/iron_gang --smoke 120` run (exit 0, correct warehouse/vehicle CNJ loading logged) both passed after this change. **Not verified**: actually walking/driving through an exit trigger interactively (no display access), and the loading screen's visual appearance beyond a dark clear + "Loading..." window title.
- Gate M6 (one skinned character, partial): `cna-extended` (a sibling repo, touched with the owner's explicit go-ahead for this one addition) gained `ModelAnimationComponentEXT`/`ModelAnimationSystem3DEXT` -- an ECS component/system pair wrapping cna's general-purpose `Model`+`AnimationPlayer` skinned path, which the existing `SkinnedModelComponentEXT`/`AnimationSystem3DEXT` do not (they only wrap the separate Avatar-specific `SkinnedModelEXT` data model, not the path cna's own glTF/CNJ import tools actually populate for a skinned asset). Verified there: 3 new unit tests (`ModelAnimationSystem3DEXTTests.cpp`, clip start/switch/hard-cut/unknown-name behavior against a real `AnimationPlayer`) plus a new `RenderSystem3DEXTTest.ModelAnimationComponent_PosesAndDraws` real-pixel-render case, all reusing cna's own proven-good `kSkinnedAnimatedGltf` fixture; full `cna-extended` suite re-run clean (2367 tests, 1 unrelated pre-existing parallel-run flake). For Iron Gang itself: a hand-authored 3-bone test character (`assets/source/gltf/gen_test_character_gltf.py` generates `assets/source/gltf/test_character.gltf` -- Mesh Craft/MC3 has no rigging/skinning authoring support, confirmed by checking `mc3.xsd` for any skin/bone/joint concept and finding none) was converted via `cna_tool_gltf_to_cnj` to `assets/generated/models/cnj/test_character.cnj` and verified by a standalone diagnostic program (not committed) that loaded it through `ContentManager`/`AnimationPlayer` directly and confirmed the "Walk" clip's leg-swing math matches hand-derived pivot-rotation values (`y≈0.084`, `z≈±0.380` at the swing extremes) and that "Idle" holds bind pose. `PrototypeRenderer` gained a minimal, dedicated `CNA::Extended::ECS::World` (just `ModelAnimationSystem3DEXT`, one entity) to drive the real component/system pair, and draws the resulting model directly via `Model::Draw()` (the same pattern already used for `warehouseModel_`/`vehicleModels_`) after pushing bone transforms onto its `SkinnedEffect`, rather than introducing `RenderSystem3DEXT`/`Camera3DEXT` (Iron Gang has no other ECS-driven rendering to justify that). A first attempt at this asset with no material/texture at all crashed at runtime (`TextureEnabled=true but texture0 is null` -- a skinned mesh with no material still gets `TextureEnabled=true` from cna's importer); fixed by adding a trivial 1x1 white texture (the same bytes cna's own test fixtures use) to every primitive. `./scripts/check-syntax.sh`, the full `ctest` suite, and a `--smoke 30` run (exit 0, `[IronGang] Loaded generated test_character.cnj` logged, no crash) all passed after the fix -- note this environment's shared multi-tenant machine was under heavy contention while validating this change (`load average` ~5 on 16 cores from concurrent unrelated sessions), which made the CPU software rasterizer visibly slower per frame; this was confirmed to be real system load, not a hang, by running longer and observing continued forward progress (236 frames in 60 real seconds) rather than a stuck call. **Not verified**: dialogue-pose/vehicle-entry-exit animations (not implemented), and any interactive/visual check of how the character actually looks or moves, since this environment has no display.
- Gate M6 clip blending (follow-up, same session): `ModelAnimationComponentEXT`/`ModelAnimationSystem3DEXT` (cna-extended) gained `BlendDurationEXT`/`BlendFromSkinTransformsEXT`/`BlendedSkinTransformsEXT` -- a per-bone `Matrix::Lerp` crossfade from a frozen outgoing-pose snapshot to the new clip's live pose over `BlendDurationEXT` seconds (default 0.25s; 0 = hard cut), replacing the earlier hard-cut-only behavior. `RenderSystem3DEXT` and Iron Gang's `PrototypeRenderer` were updated to read `BlendedSkinTransformsEXT` instead of `PlayerEXT.GetSkinTransforms()` directly. Verified in `cna-extended`: 2 new tests using a hand-verified two-named-clip glTF fixture (cross-checked against a real `AnimationPlayer` via a throwaway diagnostic before being embedded) proving the blend math numerically at t=0/halfway/finished, plus a `BlendDurationEXT=0` hard-cut case; full suite re-run clean at 2369/2369, including the previously-flaky `TexturePackerFileReaderTests` case (confirming that earlier failure was a parallel-run isolation flake, not a real regression). Verified in Iron Gang: full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` passes on every file, and a `--smoke 20` run exits 0 with correct asset-loading logs. **Not verified**: how the crossfade actually looks during a real Idle<->Walk transition, since this environment has no display.
- Gate M6 dialogue pose (follow-up, same session): `gen_test_character_gltf.py` gained a third "Dialogue" clip -- a static "parade rest" leg stance rotated about the local Z axis (a different axis than Walk's X-axis swing, so it reads as a genuinely distinct pose, not a frozen mid-walk frame) -- regenerated into `test_character.cnj` (now 3 clips). `IronGangGame::Update` calls `renderer_.UpdateCharacterAnimation(deltaSeconds, "Dialogue")` whenever `dialogue_.IsActive() && !playerDriving_ && !transitioning`, alongside the existing Walk/Idle call (which only fires when dialogue is NOT active, so the two never race). Verified: a standalone diagnostic (not committed) loaded the regenerated CNJ through `ContentManager`/`AnimationPlayer` and confirmed the Dialogue pose's foot position matches hand-derived pivot-rotation math exactly (`(-0.0747, 0.00876, 0)` for the left foot at an 8° Z-axis rotation about the hip pivot). Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, and a `--smoke 20` run exits 0 -- notably, smoke mode never dismisses the opening dialogue, so this run exercised the new Dialogue clip continuously for its entire duration, not just at t=0. **Not verified**: how the pose actually looks, since this environment has no display.
- Gate M6 vehicle entry/exit animation (follow-up, same session, gate M6 now fully done): `gen_test_character_gltf.py` gained two more one-shot clips, "EnterVehicle" (standing -> sitting, both legs bending forward together via `quat_x`, unlike Walk's alternating phase) and "ExitVehicle" (the reverse), each authored as a 1-second clip but only ever played for 0.5s (`IronGangGame::kVehicleTransitionSeconds`) so the motion is still visibly in progress -- not already at its end pose -- when the game switches away, and so `LoopEXT`'s default-true modulo wraparound never triggers. `test_character.cnj` regenerated with 5 clips total. Added a small `VehicleTransitionState` (`None`/`Entering`/`Exiting`) state machine to `IronGangGame`: entering the sedan starts `Entering` and keeps the character visible/on-foot-input-suppressed while "EnterVehicle" plays, only flipping `playerDriving_` true (hiding the character) once the clip finishes; exiting flips `playerDriving_` false immediately (character visible right where the car is) and starts `Exiting` while "ExitVehicle" plays. `HandleInteraction()` ignores a new interaction while a transition is already in progress; `LoadPrototype()`/`ResetPrototype()` both reset the state to `None` for safety. Verified: two standalone diagnostics (not committed) loaded the regenerated CNJ and confirmed both clips' foot-position math at t=0/0.5s matches hand-derived pivot-rotation values exactly (e.g. `EnterVehicle@0.5` and `ExitVehicle@0.5` both land at the same halfway pose, as expected by symmetry). Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, and a `--smoke 20` run exits 0. **Not verified**: the vehicle-transition *state machine itself* (as opposed to the underlying clip math) was checked by careful manual code review of `HandleInteraction()`/`Update()`'s new branches, not by an automated test -- smoke mode never presses 'E' and `IronGangGame` is not unit-testable headlessly today. Also not verified: how any of this actually looks, since this environment has no display.
- Gate M7 (one data-driven mission, done at prototype fidelity): `assets/missions/prologue.mission.json` (a pre-existing stub file from the original scaffold, previously just documentation of an "intended future form") is now the real, active mission definition -- 5 states, each with objective text, a named transition condition, and a next-state id. New `MissionDefinition`/`LoadMissionDefinition` (`include/`/`src/Missions/MissionDefinition.hpp/.cpp`) parse and validate it using sharp-runtime's own `System::Text::Json` (`JsonDocument`/`JsonElement`, backed by vendored `nlohmann::json`) -- already a linked dependency, no new library added. Validation rejects (with an actionable error): duplicate state ids, an `initialState`/`next` that doesn't match any state id, an unrecognized condition name, and an empty state list. `PrototypeMission` now resolves its current state's objective text and transition condition against a loaded `MissionDefinition` instead of a hardcoded switch statement, while keeping `PrototypeMissionState` (a fixed enum) for `SaveGame`'s existing int-based format and public-API compatibility -- `LoadMission()` additionally rejects any mission file introducing a state id outside that fixed set. `IronGangGame::Initialize` loads the real file, falling back to an identical hardcoded default (matching `DialogueSystem::LoadFallbackPrologue()`'s convention) on any failure. Verified: the pre-existing `TestMissionFlow` (hardcoded default) still passes unchanged; two new tests -- `TestMissionLoadsCommittedFile` (loads the real committed file and drives it through the identical reach-vehicle/enter-vehicle/drive-to-warehouse/completed flow) and `TestMissionValidationRejectsMalformedData` (6 cases: dangling `next`, dangling `initialState`, duplicate id, unknown condition, empty states, missing file -- all correctly rejected -- plus one well-formed minimal mission that still loads correctly afterward, proving failures don't corrupt the loader's own state) -- both pass. A standalone diagnostic (not committed) confirmed the real committed file parses to the exact expected 5-state structure. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` passes on every file (including the new `IRON_GANG_SOURCE_ASSET_DIR` compile definition added to both the CMake test target and the syntax-check script so the real committed file can be validated by path), and a `--smoke 20` run exits 0 with no mission-load fallback message (confirming the real file loaded successfully at runtime, not just in the standalone test). **Not verified**: save/load resuming correctly mid-mission with the new data-driven system was reasoned through (the int-enum save format and `SetState()`/`GetState()` are unchanged) but not covered by a new dedicated test, and there is no display to check the objective-text window title actually updates correctly on screen.
- Gate M8 (one in-engine cutscene, done at prototype fidelity, camera-track-only scope): new `CutscenePlayer`/`CutsceneSequence` (`include/`/`src/Cutscenes/CutscenePlayer.hpp/.cpp`, `CutsceneSequence.hpp/.cpp`) play a hand-written, versioned JSON camera keyframe sequence, parsed/validated the same way as `MissionDefinition` (sharp-runtime's `System::Text::Json`, inline validation rejecting an empty keyframe list, a first keyframe not at time 0, non-strictly-ascending keyframe times, and a duration shorter than the last keyframe). `assets/cutscenes/prologue_intro.cutscene.json`: a 2.5s pan from an establishing shot of the warehouse delivery target `(25, 12, -34)` looking at `(0, 2, -34)` to `(0, 4.65, 27.5)` looking at `(0, 1.25, 20)` -- the second keyframe computed by hand to exactly match `Draw()`'s own `target = playerPos + (0,-0.45,0); camera = target - forward*7.5 + (0,3.4,0)` formula at the player's spawn point (`(0, 1.70, 20)`, yaw 0), so the cut back to the normal follow camera has no visible pop. `IronGangGame::Initialize()` starts it alongside the opening dialogue; `Update()` ticks it every frame (gated on `!transitioning`, independent of dialogue's own pace) and treats a second Enter press (once dialogue has already finished) as a skip; `Draw()` overrides `view`/`target` with the cutscene's interpolated camera whenever `IsActive()`; player/vehicle input and physics stepping are frozen while it plays via the same `!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning` gate already used for dialogue. `LoadPrototype()`/`ResetPrototype()` both force `cutscene_.Skip()` defensively. Verified: `TestCutscenePlayerAdvancesAndFinishes` (fixed-time updates, asserts exact linear-interpolation numbers at the halfway point and the exact terminal keyframe once finished), `TestCutscenePlayerSkipAppliesTerminalState` (Skip() partway through must produce the identical terminal camera state a natural finish would), `TestCutsceneValidationRejectsMalformedData` (5 malformed-data cases + a missing file, all correctly rejected, plus one well-formed sequence that still loads afterward) -- all pass. A standalone diagnostic (not committed) confirmed the real committed file parses to the exact intended keyframe values. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, a `--smoke 20` run (covers only the first ~0.3 simulated seconds, mid-cutscene) and a `--smoke 200` run (covers ~3.3 simulated seconds, past the cutscene's 2.5s duration, confirming it naturally finishes and the game keeps running normally afterward) both exit 0 with no cutscene-load fallback message. **Not verified**: the actual Enter-press skip path in a real running game (smoke mode's dialogue never becomes inactive on its own, so the skip branch was only exercised by the standalone unit tests, not an end-to-end game run), and any visual/interactive check of how the pan actually looks, since this environment has no display.
- Gate M9 (traffic, pedestrians, one police-response scenario, done at first-pass/prototype fidelity, including the gate's own literal "ten-minute soak" wording): new `WaypointPath`/`AdvanceAlongPath()` (`include/`/`src/World/WaypointPath.hpp/.cpp`) -- a hand-authored ordered polyline plus a `loop` flag, not a graph -- is a shared path-following helper used by both new `TrafficVehicle` and new `Pedestrian` (`include/`/`src/Gameplay/`). `TrafficVehicle::Update()` accelerates toward a cruise speed (6 units/s²) and brakes (12 units/s²) when an externally-computed obstacle distance falls inside a 10-unit braking distance / 3-unit minimum gap; `IronGangGame`'s new `DistanceAheadIfInLane()` helper supplies that distance from other traffic vehicles and the player's own vehicle. `Pedestrian::Update()` walks a fixed sidewalk path at 1.6 units/s, overridden by a 4-second "flee directly away from the last-known threat position" state (2.5x speed, continuing off its own timer even after the threat is no longer reported present) when the player's vehicle comes within 6 units. New `PoliceSystem::Update()` runs `Clear -> Dispatched -> Chasing`: Clear triggers on a witnessed offense (player speeding >70 km/h, or within 2.5 units of a witness, while any traffic-vehicle/pedestrian position is within a 15-unit "witness radius" -- a simplified proximity check, not a real vision-cone/line-of-sight test); Dispatched holds for a fixed 2 seconds; Chasing drives up to 2 patrol cars straight toward the player's current position (ignoring roads) at 9 units/s, escalating to a second car after 20 seconds still chasing (the one locked escalation tier), and resolves back to Clear once the closest patrol car has stayed beyond 40 units for 3 sustained seconds. `PrototypeWorld::BuildWarehouseBlock()` hand-authors one 4-point traffic loop (both lanes of `road_north_south`, X=±3) and two 2-point sidewalk paths (X=±7.5, matching `sidewalk_west`/`sidewalk_east`); `BuildCountryside()` was not touched, so it has neither, by design. `IronGangGame` gained `RespawnTrafficAndPedestrians()` (spawns 2 traffic vehicles + 2 pedestrians, resets `police_`; called from `Initialize()`, district arrival, load, and reset, since none of this ambient state is part of `SaveGame`), ticks all three systems every frame gated only on `!transitioning` (they keep running through dialogue/cutscenes), and a new `PrototypeRenderer::DrawTraffic()` draws them as colored boxes distinct from the player's own sedan/character meshes. The window title appends "Police dispatched..."/"WANTED" while not Clear. Verified: 4 new deterministic unit tests in `tests/CoreTests.cpp` -- `TestWaypointPathAdvancesAndWraps`, `TestTrafficVehicleAcceleratesAndBrakes`, `TestPedestrianFleesAndResumesPath`, and `TestPoliceSystemFullCycle` (the big one: exercises the FULL `Clear -> Dispatched -> Chasing -> escalate -> resolve` cycle against hand-computed, standalone-diagnostic-confirmed position/timer values, plus two negative cases -- not driving, and a witness outside the radius -- both correctly never triggering a chase). A standalone diagnostic (not committed) caught a wrong test assumption before it shipped: starting a mover exactly AT its first waypoint (as `Reset()` does) immediately wraps to the second waypoint on the very first `AdvanceAlongPath()` call, since distance-to-current-target is already 0 -- the first draft of `TestWaypointPathAdvancesAndWraps` assumed no wrap on that first call and failed until corrected to match the actual (correct) behavior. Full `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh` clean, two short `--smoke` runs (30 and 90 draw frames, ~10s and ~25s wall-clock), and the gate's own literal "ten-minute soak" requirement: a `--smoke 3000` run, launched in the background and timed via `date +%s` before/after, ran for 980 real seconds (~16.3 minutes) with `trafficVehicles_`/`pedestrians_`/`police_` all ticking every frame throughout, exiting cleanly (exit 0, no crash, no error, no asset-fallback message in the log). Confirmed via `cna/src/Microsoft/Xna/Framework/Game.cpp` that `Game::Tick()` uses a fixed 1/60s timestep accumulator fed by real elapsed wall-clock time (`IsFixedTimeStep_` defaults true, `TargetElapsedTime_` = 1/60s), so simulated game time tracks real wall-clock time 1:1 for this workload -- a ten-minute soak genuinely requires close to ten real minutes of runtime, there is no way to compress it via a larger `--smoke` frame count alone. **Not verified**: memory-leak growth specifically over the soak run (only crash/stall-freedom was checked, no periodic memory sampling was added), and, as with every other visual milestone this session, there is no display to check how any of this actually looks.

- Gate M10 (production assets/collision, baked lighting, one dynamic sun, limited shadows, audio, UI -- all five pieces done at first-pass/prototype fidelity): **UI** -- `include/`/`src/UI/BitmapFont.hpp`/`.cpp` builds a real `SpriteFont` at runtime from the public-domain font8x8 bitmap glyphs (CNA has no XNB font pipeline), drawn via `SpriteBatch` each frame in `IronGangGame::Draw()` (objective/speed/dialogue/wanted/status), replacing the window-title-only display. **Dynamic sun** -- `include/IronGang/Graphics/SunLight.hpp`'s `ComputeSunBrightness()`/`ComputeBrightnessForNormal()` compute a CPU brightness scalar from a single fixed sun direction (confirmed `BasicEffect`/`SkinnedEffect`/etc.'s built-in `DirectionalLight0` lighting is a no-op on the SOFTWARE backend by reading its own source), applied via `PrototypeRenderer::DrawMesh()`'s new `tint` parameter (procedural boxes) and a new `SetModelDiffuseColor()` helper (CNJ `Model` content) to every dynamic actor (player/vehicle/traffic/pedestrians/police) each frame -- confirmed `DiffuseColor` DOES apply unconditionally on this backend, independent of the lighting no-op. **Limited shadows** -- new `PrototypeRenderer::DrawShadowDecal()` draws a flat, dark, alpha-blended "blob shadow" beneath the player and their own vehicle only (confirmed the SOFTWARE backend has no shadow-map support at all and real shadow-mapping is not achievable without modifying CNA itself). **Baked lighting** -- new `include/`/`src/Graphics/LightmapMesh.hpp`/`.cpp`: `LightmapMeshBuilder` builds the static city mesh (procedural boxes only) with 24 vertices per box (one per face, no sharing, since each face needs its own UV), baking one flat-shaded lightmap tile per face from the shared sun direction into a real texture atlas (fixed 32-column grid of 4x4 tiles, each face sampling the exact tile center to avoid bilinear bleed), sampled via a new `PrototypeRenderer::lightmapEffect_` (`DualTextureEffect`, confirmed fully implemented on the SOFTWARE backend: `finalColor = vertexColor * (texture0*2) * texture1 * diffuseColor`) with `texture0` a flat near-gray identity texture and `texture1` the real baked atlas; MC3-sourced models stay out of scope (no lightmap UV channel in that pipeline). **Audio** -- real CC0 sound from Nox Sound Design's "Essentials Series" pack (itch.io, 988MB); WebSearch/WebFetch confirmed the license but the actual download needed a JS-driven payment-bypass step neither `curl` nor this environment's (unconnected) browser tools could complete, so the user downloaded it manually and handed it off. Three files extracted (renamed only) into `assets/audio/` (`engine_loop.wav`, `horn.wav`, `footstep.wav`), recorded in `assets/licenses/asset-registry.csv`; no ambience/siren (not in the pack, and a second CC0 source hit the same download friction). `IronGangGame` gained `engineSound_`/`engineSoundInstance_`/`footstepSound_`/`hornSound_` (`SoundEffect(const std::string&)`, SDL3_mixer decodes WAV natively, no XNB step), each optional with the same try/catch-with-fallback convention as every other optional asset; the looped engine sound ties to `playerDriving_` with speed-scaled volume/pitch, the horn fires on H while driving, footsteps fire on a fixed-interval timer while walking. Verified: `TestSunBrightnessMatchesHandComputedValue`, `TestLightmapMeshBuilderBakesPerFaceBrightness` (both against hand-computed/Python-cross-checked values), `TestBitmapFontGlyphAtlas`, the full `ctest` suite, `./scripts/check-syntax.sh`, `--smoke` runs, and -- since `--smoke` mode never drives or walks -- a standalone diagnostic (not committed) that directly exercised `SoundEffectInstance`/`SoundEffect` playback against the real audio files through the real SDL3_mixer pipeline, confirming correct state transitions and successful playback with no exceptions (this environment's audio hardware initialized successfully, so `NoAudioHardwareException`'s fallback was reasoned about but not actually exercised). **Measured a real ~4-5x per-frame slowdown** from the lightmap's two-bilinear-texture-sample draw path on this environment's CPU software rasterizer (a `--smoke 20` run went from ~10s to ~26s wall-clock), not yet profiled against `docs/performance-targets.md` (real gate M12 scope). **Not verified**: how any of this actually looks or sounds, since this environment has no display and cannot play audio for a human to hear.

- Gate M11 (mission happy-path/failure/retry/save-load/cutscene-skip automation, all ten sub-tasks `IG-39-060`-`069` done): almost no new production code -- mostly proving what already exists end to end. New tests in `tests/CoreTests.cpp`: `TestSaveLoadMidMissionPlaythrough` (saves mid-mission, loads into a fresh `PrototypeMission`, proves it can still complete -- not just that the state enum round-trips), `TestCutsceneSkipDoesNotBlockMissionProgression` (confirms `PrototypeMission::Update()`'s architectural independence from cutscene state with a regression test), `TestMissionResetActsAsRetry` (this prototype's one mission has no real failure/branching state -- `plan_24`'s own locked scope -- so "retry" is proven via `Reset()` returning to the initial state and completing again), `TestVehicleStatePersistsIndependentlyOfPlayer` ("vehicle-loss recovery" reinterpreted at the level that actually exists, since no combat/damage system exists yet: a save made on foot far from a parked vehicle restores both positions independently), and `TestDistrictTransitionPreservesMissionState` (a full district round trip mid-mission leaves mission state untouched). Fresh-start playthrough and missing-optional-asset behavior were already covered by pre-existing tests. Soak test: a `--smoke 3000` background run exercising the full M10-era rendering/audio path; the lightmap draw path made it far slower per frame than the M9 baseline, so it was manually stopped (`TaskStop`, not a crash) after 65 minutes (3925s) of continuous, error-free execution -- more than six times the gate's own "ten minutes" wording. Performance capture: `/usr/bin/time -v ./iron_gang --smoke 60` measured 55988 KB (~55MB) maximum resident set size (far under the ~2-4GB budget) and ~1.55s/frame average, dominated by the lightmap's two-bilinear-texture-sample draw path plus HUD text drawing -- not meaningfully comparable to the 30-60 FPS target, since this CPU software rasterizer backend was never intended to be performant (it exists only because this environment has no GPU/display); real frame-time verification needs a `dev-easygl`/`dev-vulkan` build, still unverified here. License audit: found and fixed two real gaps -- `assets/licenses/asset-registry.csv` was missing rows for `assets/missions/prologue.mission.json` and `assets/cutscenes/prologue_intro.cutscene.json` (both original content, same convention as the already-tracked dialogue file), and `THIRD_PARTY.md` still claimed no external content was bundled, which became false as of gate M10 (font8x8 Public Domain font, Nox Sound Design CC0 audio) -- both corrected. Verified: full `compile-software` rebuild (clean), all three `ctest` targets pass (5 new tests), `./scripts/check-syntax.sh` clean.

## Full CNA-linked build status

A full Iron Gang executable (`iron_gang`) links successfully in this workspace using the `compile-software` preset against `../cnanext` and `../sharp-runtime`. The CNA-vendored SDL/SDL_image/SDL_mixer submodules are populated here; `cna-extended` is no longer required. The `dev-easygl` preset now selects CNA's public `OPENGLES3` renderer name (whose implementation is EasyGL), while `dev-vulkan` selects `VULKAN`. Only `compile-software` has been exercised end to end in this validation environment.

## Reproduction

```bash
./scripts/preflight.sh compile-software
./scripts/check-syntax.sh
MESH_CRAFT_SOURCE_DIR=../mesh-craft ./scripts/validate-mc3.sh
```

After dependencies are complete:

```bash
./scripts/configure.sh dev-easygl
./scripts/build.sh dev-easygl
./scripts/test.sh dev-easygl
./scripts/run.sh dev-easygl --smoke 120
```
