# Copper Boots development plan

Updated: 2026-08-24

Statuses are `TODO`, `DOING`, `DONE`, `BLOCKED`, and `DEFERRED`. Stable IDs are
never recycled. A task becomes `DONE` only when every acceptance criterion is
met and the evidence is recorded in its commit or this file.

## Working protocol

1. Work on `develop` after the documentation-only baseline on `main`.
2. Select the highest-priority unblocked `MAR-*` item.
3. Make the smallest coherent implementation that satisfies its criteria.
4. Build with at most two parallel jobs, run focused tests, and run the game or
   a smoke mode when presentation changed.
5. Update this ledger and important new findings in `analysis.md`.
6. Commit with a message describing the completed behavior.
7. Do not modify CNA or sharp-runtime for a game workaround; record a minimal
   `MAR-CNA-*` or `MAR-SR-*` issue first.

## Current milestone

**M1 — First playable Green Ruins workshop**

One original scrolling stage where a generated courier can walk, run, jump, and
collide with tiles while multiple procedural background layers visibly scroll.
The game must use CNA-only graphics/input and a stable fixed simulation tick.

## Foundation and research

### MAR-001 — Repository baseline — DONE

Acceptance:

- Git uses `main` and `develop`; ordinary implementation stays on `develop`.
- The first `main` commit contains only `README.md`, `.gitignore`, `LICENSE`,
  `THIRD_PARTY.md`, `analysis.md`, and `plan.md`.
- README states independent status, historical inspiration, CNA/sharp-runtime
  stack, non-affiliation, and original-asset policy.
- MIT scope explicitly excludes historical and third-party material.

### MAR-002 — Original-source acquisition and provenance — DONE

Acceptance:

- TP6/7 archive is downloaded from the official URL into ignored reference
  storage, with date, exact URL, SHA-256, file count, and version recorded.
- Archive contents never enter Git.
- TP5.5 version is identified but not fetched without a research need.

### MAR-003 — Pascal source inventory — DONE

Acceptance:

- All exact Pascal filenames and archive file groups are recorded.
- Each unit has responsibility, dependencies, important structures/procedures,
  DOS coupling, and research value summarized in `analysis.md`.
- Important units (`PLAY`, `PLAYERS`, `ENEMIES`, `FIGURES`, `WORLDS`, `BACKGR`)
  receive source-level inspection rather than inference from filenames.

### MAR-004 — Gameplay and historical behavior analysis — DONE

Acceptance:

- Player, collision, camera, enemy, power-up, projectile, transition, score,
  lives, controls, rendering, palette, parallax, and memory techniques are
  documented.
- Level dimensions/orientation, main/subarea widths, options layout, collision
  sets, preprocessing, and stage order are recorded.
- Historical facts and new-design targets are clearly distinguished.

### MAR-005 — CNA/sharp-runtime integration — DONE

Acceptance:

- Actual local branches/commits, public CMake target, consumer workarounds, and
  relevant CNA APIs are recorded.
- Project consumes CNA via configurable sibling path and CNA consumes
  sharp-runtime through its own component closure.
- Game targets link `CNA`, not renderer or sharp-runtime implementation targets.
- CNA and sharp-runtime source trees remain unmodified.

### MAR-006 — Minimal runnable CNA game — DONE

Depends on: MAR-005.

Acceptance:

- C++23 executable derives from CNA `Game`, owns `GraphicsDeviceManager`, creates
  a window, clears it, and draws a programmatically generated texture with
  `SpriteBatch`.
- `--smoke-test` exits deterministically after rendering a small frame count.
- Configure, two-job build, CTest, and runtime results are recorded.
- No game source includes SDL, graphics-backend, or native platform headers.

## Core simulation and rendering

### MAR-010 — Fixed simulation clock — DONE

Acceptance:

- CNA elapsed time feeds a 60 Hz accumulator with 250 ms clamp and eight-step
  catch-up cap.
- Gameplay sees exact fixed `1/60` ticks; rendering frequency cannot change
  simulation speed.
- Unit tests cover accumulation, clamp, catch-up cap, and fractional remainder.

### MAR-011 — Logical low-resolution framebuffer — DONE

Depends on: MAR-006.

Acceptance:

- World/HUD draw to a 320x180 `RenderTarget2D` using public CNA APIs.
- Logical surface is point-scaled to the largest centered integer 16:9 viewport
  when possible; non-integer small-window fallback stays centered and sane.
- Backbuffer is cleared around letterbox bars and resize does not stretch
  gameplay coordinates.
- Smoke test exercises render-target bind/unbind and point-filtered presentation.

### MAR-012 — Procedural VGA-inspired palette and primitives — DONE

Acceptance:

- One programmatic 1x1 texture supports colored rectangles; any larger generated
  texture is created wholly from new pixel data.
- A documented project palette is used consistently for sky, ruins, player,
  solids, hazards, and debug overlays.
- No shipping image asset is required for the milestone.

### MAR-013 — Tile model and renderer — DONE

Acceptance:

- Visual tile and collision semantic are distinct.
- Renderer draws only visible rows/columns plus one safe margin using
  `SpriteBatch`, without rebuilding the map or allocating per tile per frame.
- Original procedural solid, breakable, hazard, exit, and decorative tiles are
  visually distinguishable.
- Rendering accepts a camera snapshot and never mutates simulation.

### MAR-014 — Camera2D — TODO

Acceptance:

- Camera has viewport/world bounds, smooth target tracking, velocity look-ahead,
  optional vertical tracking policy, and shake offset isolated from base state.
- Camera never exposes outside the configured level bounds.
- Unit tests cover left/right/top/bottom clamps and look-ahead convergence.
- Player code does not directly set the camera position.

### MAR-015 — Reusable parallax layers — TODO

Acceptance:

- Layer descriptor supports factor, fixed/repeating behavior, tint, depth, and
  deterministic procedural geometry.
- First level visibly uses at least three non-world factors, including a far
  layer near 0.1 and mid layer near 0.5.
- Repetition has no seams at camera bounds and no per-frame heap churn.

### MAR-016 — Debug overlay — TODO

Acceptance:

- Toggle exposes collision boxes, tile coordinates, camera bounds, velocity,
  simulation tick, rendered sprite count, and frame/update timing.
- Overlay is rendered through CNA and can be disabled with negligible update
  cost.

## Player and collision

### MAR-020 — Player entity and state model — DONE

Acceptance:

- Renderer-free player stores position, velocity, facing, grounded state,
  vertical state, abilities, invulnerability, and transition/death state.
- Standing/walking/running/jumping/falling presentation state is derived without
  a giant switch-based update function.
- Spawn/reset behavior is deterministic.

### MAR-021 — Horizontal acceleration and deceleration — DONE

Acceptance:

- Left/right movement has momentum, ground acceleration/braking, responsive
  reversal, limited air control, and neutral simultaneous opposing input.
- After specified tick counts, velocity and stop distance fall within named
  source-inspired ranges in unit tests.
- No frame-time value enters the controller directly.

### MAR-022 — Running — DONE

Depends on: MAR-021.

Acceptance:

- Hold-run raises speed cap and affects jump reach without changing global time.
- Walk and run caps are tested in both directions.
- Releasing run while above walk cap decelerates smoothly rather than snapping.

### MAR-023 — Jumping and variable height — DONE

Acceptance:

- Jump starts only from grounded/coyote/buffer policy explicitly chosen and
  documented; holding jump gives a higher apex than early release.
- Jump input is edge-triggered and cannot auto-repeat merely by landing.
- Tests pin impulse, apex tick/range, total airtime, early-release difference,
  and running horizontal reach.

### MAR-024 — Gravity and falling — DONE

Acceptance:

- Gravity and terminal velocity are deterministic at 60 Hz.
- Walking off an edge enters falling cleanly; landing resets vertical state.
- Pathological fall below world emits a death event once.

### MAR-025 — Tile collision system — TODO

Acceptance:

- Axis-separated or swept AABB resolution prevents tunneling at supported
  player speeds and resolves left/right/floor/ceiling contact.
- Solid and OneWay semantics are distinct; decoration never implies collision.
- Tests cover corners, one-tile gaps, slopes-not-supported behavior, map edges,
  floor snap, ceiling hit, and a long repeated-run stability case.

### MAR-026 — Player procedural animation — TODO

Acceptance:

- Generated courier silhouette clearly communicates facing, idle, walk/run,
  rise, fall, damage blink, and death.
- Animation selection reads simulation state and cannot affect collision.

### MAR-027 — Damage, death, checkpoint, respawn — DONE

Acceptance:

- Hazard/enemy damage respects invulnerability; plated state absorbs one hit.
- Unprotected damage and falling trigger a bounded death sequence then respawn
  at an explicit checkpoint.
- Tests cover repeated contact, life decrement/progress policy, and reset state.

### MAR-028 — Physics fidelity measurement — TODO

Acceptance:

- Original executable is run legally in local DOSBox when available.
- Measured/observed jump apex time, airtime, acceleration, stopping, camera lag,
  and at least one enemy speed are recorded with emulator settings.
- Modern constants are compared honestly; intentional differences are listed.

## Levels and data

### MAR-030 — Versioned external level format — DONE

Acceptance:

- Format includes magic/version, name, dimensions, spawn/checkpoint, parallax,
  legend, row-major map, and later-extension policy.
- Format and example are documented; original character codes/layouts are not
  copied.
- Visual identity and collision semantic can vary independently.

### MAR-031 — Level loader — DONE

Acceptance:

- Parser accepts an independent text buffer so tests need no CNA; game transports
  that text using verified CNA `TitleContainer` and sharp-runtime stream APIs.
- Errors include path/context and line number for bad version, dimensions, row
  width, glyph, duplicates, missing spawn, or out-of-bounds metadata.
- Valid and invalid fixtures have deterministic tests.

### MAR-032 — Green Ruins original level — DONE

Acceptance:

- At least 80x11 original tiles teach movement, run and jump without enemy
  dependency, include height variation and one optional collectible route.
- Layout is new and has no column-for-column relation to historical levels.
- Spawn, checkpoint, exit, camera bounds, and parallax metadata load externally.

### MAR-033 — Transition/subarea model — TODO

Acceptance:

- Named route endpoints support same-area and subarea travel without scanning
  for magic paired bytes.
- Player input/alignment, transition lock, fade, destination spawn and camera
  reset are explicit and testable.
- Invalid or cyclic route metadata fails clearly or behaves by documented rule.

### MAR-034 — Level exits and completion — TODO

Acceptance:

- Exit freezes harmful interactions, records completion, performs a short CNA
  transition, and returns a structured result.
- Exit collision and repeat-contact behavior are unit-tested.

### MAR-035 — Checkpoints — TODO

Acceptance:

- Levels declare one initial and optional later checkpoints.
- Activation and respawn persist only intended state; tests cover both.

### MAR-036 — Additional original stages — DEFERRED

Acceptance:

- Factory, Underground, Rooftops, Frozen Facility, and Core stages each add one
  measured mechanic and use original layouts/themes.
- Six-stage campaign has progression and a full playthrough test checklist.

## Gameplay systems

### MAR-040 — Copper cog collectibles — DONE

Acceptance:

- Collectible has deterministic overlap, one-shot removal, score/count event,
  generated animation, and provenance-free presentation.
- Tests cover duplicate ticks and save/reload state policy.

### MAR-041 — Interactive and breakable blocks — DONE

Acceptance:

- Ceiling hit emits a block event; block bump is visual state separate from map
  collision; contents emerge once; plated player can break marked blocks.
- Collision remains stable while visual bump animates.
- Tests cover empty/content/used/breakable cases.

### MAR-042 — Clockwork crawler enemy — DONE

Acceptance:

- Crawler patrols, turns at walls, follows/falls from edges by explicit subtype,
  activates near camera, and despawns without losing persistent defeat state.
- Top contact defeats/bounces; side contact damages.
- State and collision tests require no renderer.

### MAR-043 — Enemy collision framework — DOING

Acceptance:

- Player/enemy classification uses previous/current bounds and relative vertical
  motion to avoid ambiguous corner stomps.
- Enemy/world and selected enemy/enemy contacts are deterministic.
- Tests cover stomp, side hit, underside, invulnerability, and simultaneous hit.

### MAR-044 — Plated-jacket power-up — DONE

Acceptance:

- Original generated pickup emerges from an interactive block, moves by clear
  rules, grants one-hit protection, score, and a short visual transition.
- Pickup collection and downgrade are unit-tested.

### MAR-045 — Arc-capacitor projectile ability — DONE

Acceptance:

- Ability pickup enables at most two live projectiles; attack is edge-triggered.
- Projectile has level/up/down aim, wall death, floor bounce, enemy defeat, and
  off-camera cleanup under documented rules.
- All trajectories and collisions have tick-based tests.

### MAR-046 — Moving platforms and drop plates — TODO

Acceptance:

- Player is carried without jitter or collision penetration.
- Horizontal, vertical, and delayed-fall paths are data-driven and tested.

### MAR-047 — Secrets and maintenance conduits — TODO

Acceptance:

- Green Ruins contains one optional secret and one original conduit/subarea
  interaction using MAR-033, discoverable without external explanation.

### MAR-048 — Additional enemy archetypes — DEFERRED

Acceptance:

- At least four mechanically distinct original enemies have small state
  diagrams, deterministic tests, generated/original assets, and tuned placement.

### MAR-049 — Core encounter — DEFERRED

Acceptance:

- Final special encounter reuses existing systems, has explicit phases and
  deterministic tests, and does not require a general boss framework.

## Input, UI, persistence, and audio

### MAR-050 — Input action adapter — TODO

Acceptance:

- CNA keyboard state maps A/D and arrows, Shift run, Space jump, Ctrl attack,
  S/Down interaction, Escape pause.
- Simulation consumes action values, not CNA key enums.
- Pressed/released/held semantics are tested.

### MAR-051 — CNA gamepad support — TODO

Acceptance:

- Movement, run, jump, attack, interact and pause work through CNA gamepad API;
  disconnect/reconnect does not corrupt input state.
- Dead zone and button defaults are documented and tested where logic-only.

### MAR-052 — HUD — TODO

Acceptance:

- Original generated/font-backed HUD shows cogs, ability/armor, level, and
  debug-independent status at logical resolution.
- HUD remains within safe bounds and is legible at integer scales.

### MAR-053 — Pause menu — TODO

Acceptance:

- Escape/gamepad pause freezes simulation but not required UI timing, resumes
  without latent input, and offers restart/quit safely.

### MAR-054 — Configurable controls and settings — TODO

Acceptance:

- Versioned settings store bindings, audio volumes, fullscreen and presentation
  mode through CNA/Sharp Runtime platform storage.
- Invalid/old settings migrate or reset predictably; no raw object layout is
  serialized.

### MAR-055 — Save/progression — TODO

Acceptance:

- Versioned save records unlocked stages and optional best score/time without
  embedding runtime object layouts.
- Atomic/recoverable write strategy and corrupted-save fallback are tested.

### MAR-056 — Original sound effects — TODO

Acceptance:

- Jump, cog, hit, enemy defeat, projectile, block, complete and UI cues play
  through CNA audio only.
- Sounds are project-generated or permissively sourced and fully recorded in
  `THIRD_PARTY.md`; no Nintendo/historical sound data is used.
- Audio-disabled or unavailable configuration fails gracefully.

### MAR-057 — Original music — DEFERRED

Acceptance:

- At least title/level/complete original tracks use CNA media/audio APIs with
  loop, pause and volume behavior verified; provenance is complete.

## Quality, portability, and release

### MAR-070 — Deterministic gameplay test suite — TODO

Acceptance:

- Tests cover all implemented controller/collision/level/entity systems and run
  without a graphics device.
- Repeated runs produce identical state hashes for scripted inputs.
- CTest exposes focused logic and smoke labels.

### MAR-071 — Performance instrumentation — TODO

Acceptance:

- Debug counters report draw calls when CNA exposes them, sprite count, entity
  count, update time, render time, frame time, and gameplay allocations.
- Static tile data is not rebuilt per frame and normal gameplay update performs
  no avoidable heap allocation.

### MAR-072 — Renderer compatibility matrix — TODO

Acceptance:

- Available mature CNA renderers record configure/build/startup, SpriteBatch,
  textures, render targets, point filtering, input, audio, and known defects.
- SDL_RENDERER plus at least one non-SDL-renderer CNA lane run the milestone when
  available; compile-only results are labeled correctly.

### MAR-073 — Resize and fullscreen validation — TODO

Acceptance:

- Repeated resize, minimize/restore, fullscreen toggle, and aspect changes keep
  logical coordinates, render target, camera and input sane on tested lanes.

### MAR-074 — Cross-platform build lanes — DEFERRED

Acceptance:

- Linux is runtime-tested; available Windows/macOS/web configurations are built
  and runtime-tested where actual environments exist, with limitations stated.

### MAR-075 — First complete playable release — DEFERRED

Acceptance:

- Six original stages, several enemies, abilities/projectiles, transitions,
  parallax, secrets, gamepad, audio, settings and progress are integrated.
- Clean clone instructions are verified with pinned CNA/sharp-runtime revisions.
- License/provenance audit finds no untracked or Nintendo-derived asset.

### MAR-076 — Packaging and release notes — DEFERRED

Acceptance:

- Source and supported binary packages include all required notices, dependency
  runtime files, controls, renderer/platform requirements, and honest known
  issues; ignored reference material is absent.

## Framework issue ledger

### MAR-CNA-001 — Embedded consumer root-layout validator — TODO

Observed at CNA `1bb2145d99ed572dd4eb15009c34e2e5f410fcf0`.

CNA's module-layout validator checks `${CMAKE_SOURCE_DIR}/src` and `/include`.
When CNA is added as a subdirectory, `CMAKE_SOURCE_DIR` is the game consumer, so
conventional consumer directories falsely look like forbidden legacy CNA trees.

Acceptance for upstream resolution:

- Minimal consumer with root `src/` configures.
- CNA standalone validator still detects an actual legacy tree in CNA itself.
- Copper Boots can move from `game/src` if that later improves clarity.

Current workaround: use `game/src` and `game/include`; no CNA modification.

### MAR-CNA-002 — Embedded vendored-header source-root paths — TODO

Observed at the same CNA revision. CNA content paths for `cgltf` and `stb` are
formed from `${CMAKE_SOURCE_DIR}`, which points at the consumer during embedded
builds.

Acceptance for upstream resolution:

- CNA uses its own project/source root for vendored include paths.
- Embedded consumer builds content module without inherited include workaround.

Current workaround: add only `${CNA_ROOT_DIR}/third_party/cgltf` and `/stb` as
inherited include directories before `add_subdirectory`.

### MAR-CNA-003 — 320x180 render-target/point-scale validation — TODO

Not yet a confirmed defect. Build a minimal public-API reproduction if any
renderer differs in orientation, first-use content, resizing, point sampling,
or render-target preservation. Record renderer/platform, exact expected/actual
pixels, CNA commit, and capability report before proposing a dependency change.

### MAR-SR-001 — sharp-runtime consumer blocker placeholder — DEFERRED

No sharp-runtime defect currently blocks the game. Create a concrete issue only
with a minimal reproduction, exact component/revision, expected/actual behavior,
and a task that cannot proceed through CNA's public surface.

## Next-task order

1. Finish `MAR-005` and `MAR-006` on `develop`.
2. Implement `MAR-010`, `MAR-011`, and `MAR-012`.
3. Implement tile/camera/player path `MAR-013`, `MAR-014`, `MAR-020` through
   `MAR-025`.
4. Add external data `MAR-030` through `MAR-032`.
5. Complete parallax `MAR-015` and validate the M1 runtime.
6. Proceed to collectibles, blocks, crawler, damage, power-up and projectile in
   `MAR-040` through `MAR-045` before menus or elaborate content.
