# NEXT.md

Handoff document for resuming work on **cna-craft**. Last updated 2026-07-11
after shipping **ambient occlusion** (plan.md §12.1 item 12, the last
`blocked` item — see §3 item 19 below), on branch `develop`. The 2026-07-10
player-vs-Craft parity audit (`plan.md` §12.1 items 26/27) compared
cna-craft against real Craft feature-by-feature and found 12 player-facing
differences; the user was asked about each one individually and decided per
item. Four small/safe fixes shipped first (item 26), then the light toggle
(item 27, via a user-approved glow-pass design pivot), the textured sky
dome (item 33), F11 fullscreen (item 34), and now ambient occlusion —
**notably NOT via the "EASYGL-only custom ShaderEffect" route the audit
recorded**: planning research disproved that premise (CNA's 3D draw path
ignores `ShaderEffect` entirely) and found a mathematically exact
no-engine-work path instead (Craft's block lighting factors into a static
per-vertex value × a per-frame daylight scalar), which the user approved
("libi se mi Vertex color reseni") — it works on ALL backends, not just
EasyGL. See CRAFT_PARITY.md §5.1's rewritten entry, which explicitly owns
and corrects the two disproven claims the old entry carried. **Later the
same day, the multiplayer planning pass was completed too** — see
`MULTIPLAYER_PLAN.md` (the design deliverable) and §3 item 20; multiplayer
IMPLEMENTATION remains gated on an explicit user go-ahead, and the plan's
§11 lists the open questions the user must decide first. See §8.

**Immediately after item 27 shipped**, the user hit it live and reported two
real bugs: an invisible window on startup (both EasyGL and Vulkan) and
general slowdown. Both root-caused by reproducing directly (not guessed) and
fixed same-session as **item 28**: `Initialize()`'s ~14-second synchronous
spawn force-load (blocking the window's first presented frame — several
compositors render that as invisible) was removed in favor of using the
already-existing backgrounded streaming pipeline from frame 1; and item 27's
glow render pass (an unconditional third full per-frame chunk-iteration +
frustum-test pass) now short-circuits entirely when no chunk is actually
lit. See item 28's plan.md writeup for the full diagnosis and fix detail.

**Then, in the same session**, the user asked for a broader change: switch
cna-craft's flight/look controls from the Craft-exact scheme (item 24, made
earlier the same day) to Minecraft-style, having tried the Craft-accurate
version hands-on and found it annoying. Shipped as **item 29**: flying is
now Space=ascend/Shift=descend independent of pitch (not Craft's real
pitch-coupled climb/descend), arrow-key look was removed entirely (mouse is
now the only way to turn), and hotbar scroll-wheel cycling turned out to
already be implemented (matches both Craft's `on_scroll` and Minecraft's
scroll-hotbar — no change needed). This is a deliberate, permanent
departure from Craft parity for these specific controls, not a bug fix —
see item 29's plan.md writeup and CRAFT_PARITY.md §1.6's notes for the full
same-day back-and-forth history.

**Almost immediately after item 29 shipped**, the user reported cna-craft
"completely broken" with a screenshot showing terrain rendered as shredded
diagonal streaks, on both EasyGL and Vulkan. Root-caused (not guessed) as
**item 30**: CNA's `Matrix::CreateLookAt` computes `Cross3(Up, forward)` as
its first step; at pitch exactly ±π/2 the player's look direction is
exactly parallel to `Up`, collapsing that cross product toward zero and
producing a numerically garbage view matrix. This was a latent bug exposed
by two of *this same session's own* earlier changes compounding: item 24
tightened the pitch clamp to the literal exact π/2 (making the unsafe value
trivially reachable), and item 29's new flying controls made "look straight
up while ascending" the natural first thing to try. Fixed by backing
`kPitchLimit` off from the literal π/2 by a small epsilon — confirmed
numerically (both a standalone check and a new unit test) that this keeps
the cross product safely non-degenerate. See item 30's plan.md writeup for
the full mechanism and CRAFT_PARITY.md §1.4 for the updated pitch-clamp
entry.

**After pulling item 30 the user was STILL broken** ("obraz je stale trhany
wasd a sipky neumoznuji chuzi") — because the real remaining bug poisoned
their `world.db`, not just the code. Root-caused and fixed as **item 31**:
item 28's async-spawn change let the player free-fall through
not-yet-loaded ground and get **entombed inside terrain** when the column
materialized around them (WASD dead — collision rejects every move from
inside solid blocks; torn rendering — camera inside geometry), and the
then-every-frame position save wrote the entombed position to `world.db`,
so it survived every restart and every code fix. Fixed by: synchronously
loading exactly one column (the player's own) at startup, nearest-first
streaming dispatch, and a detect-and-heal (`PlayerController::IsEmbedded` +
`CnaCraftGame::HealPlayerIfEmbedded`) that snaps an entombed player to the
surface — including one poisoned by the old bug, verified end-to-end
against a deliberately corrupted `world.db`. The same item also fixed the
persistent stutter: `SavePlayerState` ran every frame with two fsyncing
SQLite transactions per call (~120 fsyncs/s); now throttled to once per
second in a single transaction. See item 31's plan.md writeup.

**Then the user asked why blocks can be built on clouds** — fixed as **item
32**: `tryPlaceBlock` had only ported HALF of Craft's `on_right_click`
guard (the player-overlap check), missing the `is_obstacle(hw)` half that
makes clouds and plants unbuildable-against in real Craft; and Cloud's
`BlockDef` had wrongly left `breakable` at its default `true` (Craft's
`is_destructable(CLOUD)==0`), which the same fix corrected — with the free
side effect that light toggles and `/cube`-erases on clouds are now also
correctly rejected (both route through `IsBreakable`). Notably, this was a
prior-session **audit error owned in CRAFT_PARITY.md §2.6's notes**: the
entry quoted both halves of Craft's guard, ported one, and still claimed
`complete`. See item 32's plan.md writeup.

**Then the user picked "textured sky dome" from the remaining-work list** —
shipped as **item 33**: the untextured vertex-color gradient hemisphere was
replaced by a full UV sphere textured with a 64×16 gradient sampled
directly from Craft's own shipped `sky.png` (real dawn/dusk orange bands),
new `Render::SkyTexture` + rewritten `Render::SkyDome` +
`Worlds::ComputeTimeOfDay` (Craft's `time_of_day()`, unit-tested). Fog and
the clear color now sample the same gradient's horizon band, and the game
clock now starts at `day_length/3` (mid-morning, Craft's own
`glfwSetTime(g->day_length/3.0)`) instead of literal midnight — the
previously-unnoticed reason every screenshot this project ever took looked
dark. Also fixed in passing: the degenerate `dayLength<=0` daylight branch
returned 0.5 brightness, matching nothing in Craft (Craft pins the *timer*
to noon → brightness ≈1.0). And **item 34** (user request "how do I go
fullscreen?"): F11 now toggles fullscreen via CNA's existing
`GraphicsDeviceManager::ToggleFullScreen()` — Minecraft's binding; real
Craft has no runtime toggle at all. See items 33/34's plan.md writeups.

## 1. Project summary

CNA Craft is a first-person voxel-world game/prototype, built entirely on
[CNA](https://github.com/openeggbert/cna) — a C++ reimplementation of the
XNA 4.0 programming model — and its `sharp-runtime` utility layer. The goal
is a faithful CNA-native **port of [fogleman/Craft](https://github.com/fogleman/Craft)**
(MIT-licensed reference project; see `THIRD_PARTY_NOTICES.md`), not merely a
"Craft-inspired" prototype.

**Primary sources of truth, in order**:
1. [CRAFT_PARITY.md](CRAFT_PARITY.md) — the systematic feature-by-feature
   parity audit against the real Craft source. ~35 feature areas, each
   citing exact Craft file/line and exact cna-craft file/line, with a
   status (`complete`/`partial`/`missing`/`blocked`/`needs_human`).
2. `plan.md` §12 ("Craft Feature Parity Port") — converts `CRAFT_PARITY.md`
   into an ordered, numbered task list (24 items) with the same status
   vocabulary. **This supersedes `plan.md` §11's older backlog** wherever
   they disagree.
3. This file — a handoff summary, not an independent source of truth. Keep
   it in sync with the two files above, not the other way around.

**Key architectural decisions, updated across these sessions**: the world is
now **unbounded and chunk-streamed** in X/Z (packed-`(cx,cz)`-keyed hash map
of columns), not the old fixed `128×64×128` array — Y stays fixed
(`WORLD_CHUNKS_Y=4`), matching Craft's own real behavior. Generation and
meshing are backgrounded via `sharp-runtime`'s `System::Threading::Tasks::
TaskT` — this project's first use of threading. The block roster is
Craft's full 54-item set. Typing now generalizes to two modes (Sign,
Command via `/`), backed by a new engine-agnostic `Worlds/WorldEditor`
module for world-editing commands. Everything else is unchanged: two-layer
split (`Worlds/` engine-agnostic, `Render/` + `CnaCraftGame` CNA-dependent),
naive per-face meshing (plus a cross-billboard path for plants),
backend-agnostic game code, two-mesh-buffer (opaque/transparent)
convention.

## 2. Current status

**Build status**: verified at the end of this session (EasyGL backend) —
clean configure + build from scratch, zero warnings, `CnaCraft` executable
links and runs. Verified via real interactive Xvfb/xdotool/ImageMagick
screenshots across all three pieces of work: the chunk redesign (flew far
beyond the old 128×128 boundary in multiple directions, confirmed terrain
streams in cleanly with no permanent visual holes — a real bug found and
fixed, see §3 — flew back toward spawn, confirmed no crashes/leaks and a
bounded thread count throughout), the roster expansion (cycled the hotbar
through all 54 slots via `E`, confirmed the HUD showed the right name at
each slot, confirmed naturally-generated flowers now render in more than
one color side by side), and chat/slash-commands (confirmed `/` opens the
Command typing box, confirmed `/view 20` updates both the console and the
new on-screen message log, confirmed an invalid `/view 99` and an
unrecognized `/nonsense` both produce Craft-accurate feedback that stacks
correctly in the message log, confirmed the log persists after the typing
box closes, confirmed Sign typing is unaffected by the `TypingMode`
generalization).

**Test status**: `tests/worlds_smoke_test.cpp` — **341 checks, all
passing** (up from 303 — the 38 new checks are the item-12 AO known-value
suite; 173 at the start of the chunk-redesign session). Plus
`cna_craft_persistence_smoke_test` — **40 checks, all passing**. Both build
and run standalone with `-DCNA_CRAFT_BUILD_GAME=OFF`. The worlds suite also
got FASTER with AO in (18.7s vs 20.4s before) — the padded-occupancy
snapshot removed per-face `World::IsOpaque` hash walks from face culling.

**plan.md §12.1 status as of this session's end** (34 items):
- **33 `completed`**: everything from before (items 26-34 — see §3), plus
  **item 12, ambient occlusion** (2026-07-11, the last `blocked` item —
  see §3 item 19 below). Nothing is `blocked` anymore.
- **1 `pending` (implementation only)**: multiplayer — its **planning pass
  is done** (2026-07-11, `MULTIPLAYER_PLAN.md`); writing actual networking
  code still requires an explicit user go-ahead plus answers to the plan's
  §11 open questions (compatibility target, server placement, auth scope).

Nothing decided is unstarted anymore — see §8 for what's actionable next.

## 3. Recent changes (this session)

All individually committed and pushed to `develop`. This was a large,
multi-phase architecture change — planned via `EnterPlanMode` (three
research passes + a design-review pass) before any code was touched, then
implemented in 7 phases, each independently built/tested before the next
started (plan.md §12.1 item 19 has the full per-phase writeup):

1. **Phase 0** — SQLite schema migration: added Craft's real `p,q`
   chunk-address columns to `block`/`sign`. Pre-migration `world.db` files
   now fail loudly at open instead of silently misbehaving. No migration
   path — delete `world.db` to upgrade.
2. **Phase 1** — `World`'s storage rebuilt as a two-level hash map keyed by
   packed `(cx,cz)` — unbounded X/Z, Y still fixed. `GenerateColumn`/
   `IsColumnLoaded`/`UnloadColumn` new API; `World::Generate(seed)` kept as
   a legacy whole-region wrapper so most pre-existing tests needed zero
   changes. Ported Craft's real tree-canopy chunk-boundary margin check as
   a documented, deliberate minor regression (a few boundary-adjacent trees
   that used to spawn no longer do).
3. **Phase 2** — `WorldStore` per-chunk-scoped I/O (`LoadColumnInto`/
   `LoadColumnSignsInto`/`LoadColumnEdits`, `WHERE p=? AND q=?`).
4. **Phase 3** — `CnaCraftGame` streaming integration, synchronous first
   (deliberate de-risking before threading): `kCreateRadius=6`/
   `kDeleteRadius=9` chebyshev load/unload, spawn moved to world-origin,
   `chunkRenderers_` became a hash map (also fixed a latent correctness bug
   — the old flat vector was only ever kept in lockstep with `World` by
   construction order), signs wired to column load/unload, floor-catch
   fixed for unloaded columns, fog now tracks `kCreateRadius * CHUNK_SIZE`.
5. **Phase 4** — backgrounded generation + meshing via `TaskT`. Found
   `TaskT<T>::getResultProperty()` requires `T` copy-constructible (a
   `shared_ptr`-reached member return, not a named local — no implicit
   move), which ruled out returning a move-only chunk array; solved via
   `World::CopyColumn`/`AdoptColumnCopy`/`InstallChunkCopy`. **Real bug
   found via visual verification and fixed**: completed-but-over-the-
   per-frame-apply-cap jobs were discarded outright instead of deferred —
   harmless for generation, but for meshing (whose dirty flag clears at
   *dispatch* time) this caused **permanent unfilled rectangular holes** in
   distant terrain. Fixed by deferring instead of discarding, plus added a
   missing dispatch cap on meshing that could otherwise fan out into dozens
   of concurrent tasks per column arrival. Re-verified via matched-
   flight-path screenshot comparisons.
6. **Phase 5** — added the three genuinely new-risk tests: column
   load/unload lifecycle, boundary-remesh correctness (a boundary face is
   exposed while its neighbor column is unloaded, correctly culled once the
   neighbor loads with a solid block across the shared boundary), and a
   real generate→edit→save→unload→regenerate→reload persistence cycle for
   one column.
7. **Phase 6** — this documentation pass: `CRAFT_PARITY.md` §3.1/§3.2/
   §4.2/§4.3/§5.2, `plan.md` §12.1 item 19's writeup, `README.md` (world.db
   reset note, unbounded-world player-visible behavior), this file.

Test count progression: 173 → 199 (phases 1-3, incremental) → 207 (phase 5)
in `worlds_smoke_test`; 21 → 26 (phase 0/2 schema/column tests) → 30
(phase 5) in `persistence_smoke_test`.

8. **Block roster expansion to Craft's full 54 items** (plan.md §12.1 item
   25, CRAFT_PARITY.md §2.2/§3.7), picked up immediately after item 19 in
   the same day, per user choice between it and Chat/slash-commands. Added
   `Chest` (plain solid cube, no special mesh), the other 5 flower colors
   (`Red/Purple/Sun/White/BlueFlower` — reuse the existing plant-billboard
   path unchanged), and all 32 dye colors (`Dye00`-`Dye31`, plain solid
   cubes). **New `BlockType` values appended after `Bedrock`, never
   inserted earlier** — `WorldStore` persists `BlockType` as its raw enum
   ordinal, so inserting mid-roster would silently reinterpret every
   existing `world.db`'s saved blocks; `Hotbar::kSlots` likewise appends
   its 38 new slots after the original 16 so number keys 1-9 and every
   pre-existing test assertion keep meaning exactly what they always have.
   `TextureAtlas`'s grid grew 5×5 → 8×8; the 32 dye tiles use real RGB
   values sampled directly from Craft's own shipped `texture.png` (its dye
   tiles are flat swatches there too, so there was no "procedural pattern"
   to invent — matching the real asset was both easier and more faithful).
   Also ported Craft's real flower color-pick noise (`world.c`) so world
   generation actually scatters all 6 colors, not just the hotbar. Fixed a
   test bug caught by the suite itself while writing this: an early version
   of the new Chest/flower BlockDef test reused one `World` across sub-checks
   and picked up Chest's opaque geometry in a plant's "should be empty"
   mesh assertion — split into isolated `World` instances per sub-check.
   226 checks in `worlds_smoke_test` (up from 207), persistence suite
   unchanged at 30. Verified against a real EasyGL build under Xvfb: cycled
   the hotbar through all 54 slots via `E`, confirmed the HUD showed
   `#21/54 WhiteFlower` through `#35/54 Dye12` correctly, and a screenshot
   showed naturally-generated orange/yellow and purple flower blooms side
   by side, confirming the color-pick noise actually varies.

9. **Chat/slash world-editing commands** (plan.md §12.1 item 17,
   CRAFT_PARITY.md §4.5) — picked up immediately after item 25, closing the
   last real gap in the priority queue. Planned via `EnterPlanMode` (same
   scope class as the chunk redesign) after reading Craft's real
   `parse_command` and every function it calls directly from the checkout.
   New `src/CnaCraft/Worlds/WorldEditor.hpp/.cpp`: 7 engine-agnostic
   geometry primitives (`PaintBlock`=`builder_block`, `FillCuboid`=`cube()`,
   `FillSphere`=`sphere()` — covers both `/sphere` and `/circlex|y|z` via
   flatten flags, exactly like Craft reuses it — `FillCylinder`=`cylinder()`
   (a stack of flattened `FillSphere` calls along one axis), `FillArray`=
   `array()`, `GrowTree`=`tree()`, `PasteRegion`=`paste()`) plus
   `Worlds::ExecuteCommand`, a direct port of `parse_command`'s dispatch
   chain for every world-editing command. The 5 multiplayer-auth commands
   and the plain-chat fallback are deliberately not ported (no networking,
   no one to talk to). `CnaCraftGame`'s sign-only `isTypingSign_` bool
   generalized to `TypingMode{None,Sign,Command}`; `/` (`Keys::
   OemQuestion`) opens Command typing pre-seeded with `"/"`. New `mark0_`/
   `mark1_` (mirrors Craft's `g->block0`/`g->block1`) updated via a new
   `RecordMark` at both break and place. `radii_` (`Worlds::CommandRadii`)
   replaces the compile-time `kCreateRadius`/`kDeleteRadius` as the mutable
   value `/view` mutates (clamped `1..24`); fog now derives from
   `radii_.createRadius` every frame instead of a compile-time constant.
   `Render::Hud` gained a real 4-line scrolling message log (`PushMessage`)
   for command feedback — **user decision (2026-07-10)**: full
   Craft-accurate multi-line log over a simplified single-line flash
   message, when asked to choose. Caught and fixed one real test-authoring
   bug via the suite itself: an early `PasteRegion` test accidentally chose
   source/destination coordinates that triggered a genuine, faithfully-
   reproduced Craft quirk (pasting a region whose destination Y range
   overlaps not-yet-read source rows can clobber source data mid-copy,
   since Craft's own `paste()` reads/writes the same live map in one
   ascending-y sweep) — not a bug in the port, fixed by choosing
   non-overlapping test coordinates instead, with the quirk itself now
   documented in `WorldEditor.cpp`. 41 new checks — 267 in
   `worlds_smoke_test` (up from 226), persistence suite unchanged at 30.
   Verified against a real EasyGL build under Xvfb: `/` opens the
   "Command: /_" typing box correctly, `/view 20` updates both the console
   log and the on-screen message log ("Viewing distance set to 20."), an
   invalid `/view 99` and an unrecognized `/nonsense` both produce
   Craft-accurate feedback that stacks correctly (oldest on top, persists
   after the typing box closes), and Sign typing (backtick) is unaffected.
   The world-editing paint commands themselves (`/cube`, `/sphere`, etc.)
   couldn't be verified live end-to-end — they need mouse-driven
   break/place to set real marks first, and synthetic mouse clicks into
   this sandbox's relative-mouse-mode SDL window remain unreliable (same
   documented flakiness as every other click-dependent feature in this
   project's history) — covered instead by the exhaustive `WorldEditor`/
   `ExecuteCommand` unit tests, arguably better regression coverage than a
   one-off manual screenshot would have been anyway.

10. **Player-vs-Craft parity audit + 4 small follow-up fixes** (plan.md
    §12.1 item 26, CRAFT_PARITY.md §1.9/§3.7/§4.3/§4.1). A full comparison
    against real Craft (two parallel audit forks, then an artifact listing
    12 differences) was reviewed with the user one item at a time via
    `AskUserQuestion`. The four decided as small/safe were done first, each
    its own commit: arrow-key look speed (`1.6f*dt` → Craft's real
    `1.0f*dt`), per-instance plant billboard rotation (was: every plant of
    a type shared one orientation; now: `Simplex2`-seeded per-(x,z)
    rotation, matching Craft), sign billboard winding (both-windings → one
    correct winding, to remove a ghost-text backface artifact — **shipped
    without live re-verification**, since synthetic keyboard text input
    broke in this sandbox mid-session; see §4), and player-position
    persistence across sessions (new `state` table, `SavePlayerState`/
    `LoadPlayerState`, verified end-to-end via a real kill-and-relaunch
    Xvfb test). See item 26's plan.md writeup for full detail.

11. **Light toggle, Ctrl+right-click** (plan.md §12.1 item 27,
    CRAFT_PARITY.md §2.7/§5.1) — the one genuine (non-deliberate) gap from
    the same audit. Porting Craft's real light-propagation system turned
    out to require new CNA-engine-level work (all three graphics backends
    dispatch shader/pipeline selection by hardcoded vertex-buffer byte
    stride, not by declared vertex elements — no existing combo supports
    texture + normal-lighting + vertex-color together). This tradeoff was
    shown to the user rather than silently worked around; the user asked
    for an alternative, and a proposed **separate additive glow pass**
    design (toggle mechanic/persistence 100% Craft-faithful; the lit
    block's own faces get an unlit warm-white-tinted pass instead of real
    propagation into neighbors) was accepted and planned via
    `EnterPlanMode` before implementation. New: `Chunk::lightSources_`
    overlay + `IsLightSource`/`SetLightSource` (`Chunk`+`World`), a third
    `glow` mesh in `ChunkMeshData`/`ChunkMesher::Build`, a `light(p,q,x,y,z,w)`
    SQLite table matching Craft's real schema shape, `Ctrl+right-click`
    wired into `CnaCraftGame`'s right-click branch, a third `ChunkRenderer`
    buffer pair + `DrawGlow`, and a third render pass in `CnaCraftGame::Draw`.
    21 new checks. Verified via clean builds + a real Xvfb regression
    screenshot (glow pass exists but draws nothing when unused, no visual
    change to ordinary terrain); **actually toggling a light and seeing it
    glow was not live-verified** — needs a working synthetic mouse click,
    unreliable in this sandbox — covered by unit tests + code review
    instead, the same tradeoff already accepted for item 17's paint
    commands. See item 27's plan.md writeup for the full technical
    rationale.

12. **Fix invisible window on startup + a glow-pass performance regression**
    (plan.md §12.1 item 28) — two real bugs the user hit immediately after
    trying item 27 live ("invisible window on startup, both EasyGL and
    Vulkan" / "also somehow slowed down"). Root-caused by reproducing
    directly: `Initialize()`'s spawn-area force-load measured ~14 seconds
    wall-clock (169 columns, fully synchronous, before the first frame ever
    presented — several compositors render that as a blank/invisible
    window), identical on both backends since the code is engine-agnostic.
    Removed the force-load entirely in favor of the already-existing
    backgrounded streaming pipeline (item 19 phase 4) — `Draw()` now
    presents a real frame within under 1 second, terrain pops in
    progressively instead of blocking. Also found and fixed: item 27's glow
    render pass unconditionally iterated every loaded chunk with a frustum
    test every frame even though almost no world ever has a lit block — new
    `glowChunkCount_` lets `Draw()` skip that entire pass when nothing is
    lit. See item 28's plan.md writeup for full detail, including a
    documented-but-unchanged pre-existing characteristic found along the
    way (each streamed column/chunk spawns a real new OS thread via
    `sharp-runtime`'s `TaskT`, not a pooled one — not the root cause of
    either report, left alone).

13. **Switch flight/look controls to Minecraft-style** (plan.md §12.1 item
    29) — the user tried item 24's earlier Craft-exact flying/arrow-look
    changes live and asked for Minecraft-style controls instead, a
    deliberate reversal, not a bug fix. `PlayerController`'s pitch-coupled
    flying replaced with Space=ascend/Shift=descend independent of pitch
    (new `PlayerInput::descendPressed`), holding both cancels to zero;
    horizontal fly speed no longer scales by look angle. Arrow-key look
    removed entirely — mouse is now the only way to turn. Fixed a knock-on
    conflict along the way: `Draw()`'s existing Left-Shift-zoom feature now
    gates on `!IsFlying()` so descending doesn't also zoom the camera.
    Hotbar scroll-wheel cycling turned out to already exist (matches both
    Craft's `on_scroll` and Minecraft) — confirmed by reading the code, no
    change needed. 4 unit tests rewritten for the new flight physics — 289
    checks (up from 288). `README.md` §5 and CRAFT_PARITY.md §1.6/§1.9
    updated, the latter preserving the full same-day back-and-forth history
    so a future reader sees two deliberate decisions, not drift. **Key
    bindings could not be live-verified** — synthetic keyboard input,
    including previously-reliable movement keys, stopped reaching the app
    window in this sandbox partway through this session (see §4) — coverage
    relies on the rewritten `PlayerController` unit tests (real physics, not
    mocked) plus code review.

14. **Fix a real rendering-corruption bug from an exact-π/2 pitch clamp**
    (plan.md §12.1 item 30) — user-reported ("cna-craft je zcela rozbity",
    screenshot: terrain shredded into diagonal streaks, both EasyGL and
    Vulkan) within moments of trying item 29's new flying controls. Traced
    to CNA's `Matrix::CreateLookAt` (`cna/src/Microsoft/Xna/Framework/
    Matrix.cpp`): it computes `Cross3(Up, forward)` first, and at pitch
    exactly ±π/2 `LookDirection()` is exactly parallel to `Up`, collapsing
    that cross product toward zero and producing a numerically garbage view
    matrix — a latent bug made trivially reachable by item 24's earlier
    same-day exact-π/2 pitch clamp, exposed by item 29's flying controls
    making "look straight up while ascending" the natural first move. Fix:
    `kPitchLimit` backed off to `π/2 - 0.01f`. New
    `TestPlayerControllerPitchClampAvoidsDegenerateLookDirection` (2 checks)
    confirms `LookDirection()`'s horizontal component stays well above
    float32 noise at the clamp — 291 checks (up from 289). **Not
    re-verified against the user's original repro** (their machine, not
    this sandbox) — grounded in reading the exact engine mechanism plus
    independently-confirmed numerics, but ask them to confirm after pulling.
    (Follow-up: the user was still broken after this — item 30's fix was
    real but their dominant symptom turned out to be item 15's entombment,
    whose poisoned `world.db` state survived code fixes.)

15. **Fix player entombment at spawn + per-frame SQLite fsync stutter**
    (plan.md §12.1 item 31) — the user's "still torn, WASD/arrows don't
    walk" report after pulling everything above. Two compounding
    regressions from this same day: (a) item 28 removed ALL synchronous
    spawn loading, so the player free-fell through not-yet-loaded ground
    from frame 1 and got entombed inside the terrain when the column
    materialized around them ~a second later (raster-order streaming
    dispatch loaded far corners before the player's own column, widening
    the race) — axis-separated collision rejects every move from inside
    solid blocks (WASD dead) and the camera sits inside geometry (torn
    rendering); the every-frame position save then wrote the entombed
    position into `world.db`, making the stuck state survive restarts AND
    all subsequent code fixes. (b) That same every-frame `SavePlayerState`
    was itself the stutter: DELETE+INSERT as two implicit SQLite
    transactions = up to 120 disk fsyncs/second — invisible on the
    sandbox's fast disk, brutal on real hardware. Fixes: `Initialize()`
    synchronously loads exactly one column (the player's own, ~80ms;
    `LoadColumnSynchronously` re-added), `UpdateStreaming` dispatches
    nearest-first (chebyshev-sorted), new `PlayerController::IsEmbedded` +
    `CnaCraftGame::HealPlayerIfEmbedded` detect-and-heal at `Initialize()`
    and `PollGenerationJobs`' column-apply (the only two entombment
    moments), `SavePlayerState` throttled to once per second and wrapped
    in a single transaction. 4 new checks — 295 total. **Verified
    end-to-end including the poisoned-data path**: a `world.db`
    deliberately corrupted to an inside-terrain position healed itself on
    launch (message printed, player on surface, row self-corrected within
    a second); fresh-world launch screenshot shows a normal ground-level
    view. Users do NOT need to delete `world.db`.

16. **Port both halves of Craft's cloud guards** (plan.md §12.1 item 32) —
    user asked why blocks can be built on clouds. Two real gaps, both
    verified against the Craft checkout before changing anything:
    `tryPlaceBlock` was missing the `is_obstacle(hw)` half of Craft's
    `on_right_click` guard (fixed with one line — `IsCollidable` on the
    targeted block, which is already false for Cloud AND plants, exactly
    like Craft's own predicate), and Cloud's `BlockDef` wrongly left
    `breakable` at its default `true` (Craft: `is_destructable(CLOUD)==0`),
    which also auto-corrected the light-toggle and `/cube`-erase paths
    (both route through `IsBreakable`). This was a prior-session audit
    error, owned explicitly in CRAFT_PARITY.md §2.6's notes: the entry
    quoted both halves of Craft's guard, ported one, and still claimed
    complete. 4 new checks — 299 total.

17. **Textured sky dome** (plan.md §12.1 item 33) — user picked it from the
    remaining-work list. New `Render::SkyTexture` (64×16 gradient sampled
    directly from Craft's own `sky.png` — real dawn/dusk orange bands,
    embedded as constants like the dye colors were), `Render::SkyDome`
    rewritten as a full textured UV sphere (`VertexPositionColorTexture`,
    time-of-day baked into vertex U per frame since BasicEffect has no
    custom-uniform slot), fog + clear color now sample the same gradient's
    horizon band, and the game clock starts at `day_length/3` (mid-morning,
    Craft's `glfwSetTime` — previously literal midnight, why every prior
    screenshot looked dark). New `Worlds::ComputeTimeOfDay` (unit-tested);
    also corrected the degenerate `dayLength<=0` daylight branch (returned
    0.5 brightness; Craft yields ≈1.0 noon daylight). 303 checks (up from
    299). Screenshot-verified at two time-axis points (midnight black sky
    pre-offset; light-blue day sky at launch post-offset); the dawn/dusk
    bands come from the same numerically-verified table but weren't
    directly screenshot-verified (the sandbox's software renderer runs the
    fixed-step game clock slower than wall time — see CRAFT_PARITY.md
    §5.3's note).

18. **F11 fullscreen toggle** (plan.md §12.1 item 34) — user asked how to
    go fullscreen; there was no way. One edge-triggered branch calling
    CNA's existing `GraphicsDeviceManager::ToggleFullScreen()`. Minecraft's
    binding (real Craft has only a compile-time FULLSCREEN flag, no runtime
    toggle). README §5 updated. Needs a real WM to verify visually — one
    keypress on the user's machine.

19. **Ambient occlusion** (plan.md §12.1 item 12, CRAFT_PARITY.md §5.1 —
    session of 2026-07-11, three committed phases + docs). The headline:
    shipped with **zero CNA engine changes, on all backends**, killing the
    audit-era "needs custom ShaderEffect, EASYGL-only" premise — planning
    research found CNA's 3D draw path ignores `ShaderEffect` entirely
    (2D-SpriteBatch-only on every backend), AND that with no torch light
    (item 27's pivot) Craft's whole block-lighting equation factors into
    `texel × (daylight*0.3+0.2) × (1+df)*aoBrightness`: a per-frame scalar
    (rides `BasicEffect::DiffuseColor`, so day/night stays a live uniform
    with zero rebaking) times a static per-vertex value (new
    `MeshVertex::shade`, uploaded as `shade/2` in a
    `VertexPositionColorTexture` grayscale color). `ChunkMesher::
    ComputeOcclusion` ports Craft's `occlusion()` exactly (27-neighborhood
    corner/side rule + curve, 8-block column shade, anti-anisotropy
    diagonal flip, plants' scalar `min_ao`, clouds' 20% contrast
    compression keyed on `BlockType::Cloud`); lookup cells are derived
    geometrically from the mesher's own face table rather than transcribed
    from Craft's differently-ordered ones, pinned by 38 hand-derived
    known-value checks. Knock-ons: mesh-job snapshots widened 7→27 chunks,
    `MarkNeighborColumnsDirty` 4→8 columns, `World::SetBlock` dirties the
    `[x-1,x+1]×[y-8,y+1]×[z-1,z+1]` cross product — all three prevent
    baked seams at chunk borders. Terrain no longer uses the
    `EnableDefaultLighting` 3-light rig (Craft never had specular/tinted
    lights — this is a parity WIN); signs are the rig's only remaining
    consumer. Verified live under Xvfb via a staged `world.db` scene
    (wall spanning the x=16 border: contact shadow continuous, no seam;
    dark overhang underside; lone-block contact ring; plant on stone
    fully bright; clouds mildly shaded; glow block bright and
    daylight-independent). **Day/night USER-VERIFIED on real hardware
    later the same day** ("soumrak a noc stridani dne a noci jsou ok") —
    the only remaining unverified surface is the Vulkan backend (EasyGL
    build only in the sandbox), grounded in code reading + tests.

20. **Multiplayer planning pass** (plan.md §12.1 item 18, CRAFT_PARITY.md
    §4.6 — same session, 2026-07-11). Deliverable: **`MULTIPLAYER_PLAN.md`**
    at repo root — no code written, per the standing planning-only
    decision. Built from three verified research passes (real Craft's
    client.c/server.py/db.c/auth.c; CNA + sharp-runtime capabilities;
    cna-craft's integration surface). Highlights a future implementer must
    not re-derive: (a) the complete Craft wire-protocol table, both
    directions, with the rowid-based `C,p,q,key`/`K` incremental chunk
    versioning and the optimistic-edit + server-revert model; (b)
    **sharp-runtime already ships a real `TcpClient`/`NetworkStream`/
    `StreamReader::ReadLine()` stack** (POSIX+Winsock) plus `Thread` +
    `ConcurrentQueue` — no engine work needed, and CNA's ENet `Net` layer
    (which older plan.md text suggested) is the WRONG transport for a TCP
    line protocol — plan.md §11.6/CRAFT_PARITY §4.6 amended accordingly;
    (c) the wire-vs-world compatibility analysis: real-Craft-server play
    is blocked by terrain-generation identity (Craft servers validate
    edits against terrain generated by the same C code as their clients),
    CHUNK_SIZE 32 vs 16, and mismatched block-ID ordinals — hence the
    recommended scope is cna-craft↔cna-craft with an own `CnaCraftServer`
    reusing `Worlds/`+`Persistence/` (validation coherent by
    construction), Craft's protocol kept shape-identical with two honest
    deltas (version 1001 + a `W,seed` world-info message); (d) phasing
    M0-M7 and §11's four open user decisions. Also recorded there: the
    latent single-player finding that `WorldStore`'s schema matches
    Craft's byte-for-byte but the `w` VALUES don't (cna-craft BlockType
    ordinals ≠ Craft item ids), so a real Craft `craft.db` would be
    misread — documented, not fixed.

21. **Fix upside-down flower sprites** (plan.md §12.1 item 35 — user
    report while playing, 2026-07-11: "ohledne kvetin - jsou obracene").
    `FlowerPattern` in `Render/TextureAtlas.cpp` drew its art assuming
    "pixel row 0 = top", but `MapAtlasUv` maps `localV=0` (a quad's
    BOTTOM vertices) to pixel row 0 — blooms rendered at the ground,
    stems up. One mirrored coordinate fixes it; the V convention is now
    documented at the flip site. The flower was the atlas's ONLY
    vertically-directional tile, so nothing else is affected (checked:
    all cube patterns vertically symmetric, tall-grass blades
    full-height). Present since the flower tile was first drawn — not an
    AO regression. Verified before/after via a staged-world.db lineup of
    all six flower colors under Xvfb. Same session, the user also
    confirmed dusk/night/day cycling looks right on their machine (see
    item 19's verification note).

## 4. Current blocker / main problem

**None.** Clean build (both `-DCNA_CRAFT_BUILD_GAME=OFF` and
`-DCNA_GRAPHICS_BACKEND=EASYGL`, built from scratch), 303/303 + 40/40 tests
passing, zero compiler warnings.

Carried over, still unresolved, still not urgent: mouse-look reliability
confirmation on the user's real (non-sandboxed) machine — `needs_human`,
unchanged for several sessions now (see `plan.md` §11.0/§12.1 for history).
The same underlying sandbox limitation (unreliable synthetic mouse clicks
into a relative-mouse-mode SDL window) is also why the new `/cube`-style
world-editing commands' actual painting couldn't be verified live (§3 item
9), and why item 11's light-toggle-actually-glowing couldn't be
live-verified either.

**Sandbox input status (re-checked 2026-07-11, the AO session)**: synthetic
keyboard **recovered** — both movement/action keys (W/A/Tab/Space via
`xdotool keydown/keyup --window <id>` with short sleeps; the instantaneous
`xdotool key` form did NOT register, use explicit keydown/keyup) and
typing-trigger keys (slash opened the Command box) reached the app window
reliably all session. **Mouse remains dead** (relative motion produced zero
pixel change; clicks untested and historically unreliable). Practical
consequence: anything mark-dependent (`/cube` painting needs mouse-set
marks) still can't be driven live, but a **staged `world.db`** works
beautifully as a substitute — the AO session verified its scene by
inserting `block`/`light`/`state` rows directly (the `state` table even
sets yaw/pitch, i.e. full camera aim without any input at all; see item
19). Re-check input at the start of each fresh session before assuming
either state persists.

**New, deliberate, documented behavior changes from these sessions** (not
bugs — read before "fixing"):
- Player-driven block edits and freshly-streamed-in terrain render 1-2
  frames later than the old same-frame synchronous rebuild (backgrounded
  meshing) — imperceptible at 60fps.
- A handful of trees right at a chunk-column boundary no longer spawn
  (Craft's own real per-column margin-check behavior, previously
  unreplicated since the whole world used to generate as one unit).
- No occlusion/frustum culling exists anywhere in the engine, and now the
  world is unbounded — every loaded column within `radii_.createRadius`
  draws every frame regardless of visibility. Not a problem at the current
  default radius (6 chunks), but worth remembering if `/view` is used to
  increase it a lot, or the default is raised.
- A mismatched-type `/cube`/`/array`/`/cylinder` (marks aren't the same
  block type) now shows a message instead of Craft's own silent no-op — a
  small, deliberate UX improvement, not a fidelity gap.
- `PasteRegion` can clobber source data mid-copy if the destination Y range
  overlaps not-yet-read source rows in the *same* World — a real,
  faithfully-reproduced Craft quirk (same loop shape as Craft's own
  `paste()`), not a bug. Harmless for the common case (pasting somewhere
  that doesn't Y-overlap the source).

## 5. Known bugs and limitations

See `CRAFT_PARITY.md` for the authoritative, cited, per-feature list — read
it before re-deriving anything from memory or re-reading Craft's source
from scratch. Highlights of what's NOT done yet:

- **Ambient occlusion is DONE (item 12, 2026-07-11)** — and NOT via the
  EASYGL-only ShaderEffect route this list used to describe (that premise
  was disproven; see CRAFT_PARITY.md §5.1's corrected entry). Two small
  deliberate residuals: a lit (glow-toggled) block's faces show no AO
  (matches Craft's own `lights[13]==15` saturation exactly), and
  world-bottom y=0 undersides bake dark because cna-craft emits faces
  Craft suppresses (`ey > 0`) — invisible in practice, documented not
  fixed.
- **Deliberate simplification (item 33)**: the sky dome is textured with
  Craft's real sky.png colors now, but `BasicEffect`'s fixed-function fog
  has one flat color per draw, so fog uses the gradient's horizon band for
  all elevations (Craft samples per fragment at each fragment's own
  elevation angle) — see CRAFT_PARITY.md §5.2.
- **Deliberately deferred, planning only**: multiplayer, per user decision.
- **Deliberate simplification (item 27)**: light toggle exists and persists
  faithfully, but a toggled light's glow doesn't propagate to neighboring
  faces or attenuate by distance — a self-contained glow pass on the lit
  block's own faces only, due to a CNA engine vertex/shader-dispatch
  constraint (see item 27's plan.md writeup, CRAFT_PARITY.md §5.1).
- **Not ported (out of scope)**: Craft's `/identity`/`/login`/`/logout`/
  `/online`/`/offline` commands (multiplayer auth) — no networking exists
  to authenticate against.
- **Nothing else identified**: the block roster (item 25), chat/commands
  (item 17), and the small audit follow-ups (item 26: arrow-key speed,
  plant rotation, sign winding, player-position persistence) are all done.

## 6. Architecture notes

**New this session** (see plan.md §12.1 item 19 for full per-phase detail —
this is just the "if you touch this again" summary):

- `Worlds/World` — storage is now `unordered_map<ColumnKey,
  array<unique_ptr<Chunk>, WORLD_CHUNKS_Y>> columns_`, keyed by
  `World::PackColumnKey(cx,cz)`. `GetBlock`/`SetBlock`/etc. degrade
  gracefully for an unloaded column exactly like they used to for an
  out-of-range coordinate (returns `Air`, writes are no-ops) — same
  pattern, new meaning. `World::Generate(seed)` is a **legacy wrapper**
  looping `GenerateColumn` over the old fixed region — still used by test
  code and any caller that just wants "a whole small world," don't remove
  it without checking callers first.
- `System::Threading::Tasks::TaskT<T>` (sharp-runtime, first use in this
  project) — **`T` must be copy-constructible**, not just movable
  (`getResultProperty()` returns via a `shared_ptr`-reached member, not a
  named local — doesn't qualify for implicit move-on-return). A captured
  lambda passed to `TaskT::Run` must itself be copy-constructible too
  (`std::function` requirement) — never capture a live/move-only `World` by
  value; capture plain copyable snapshot data and reconstruct a throwaway
  `World` **inside** the lambda body instead (see `World::CopyColumn`/
  `AdoptColumnCopy`/`InstallChunkCopy`, and `CnaCraftGame::
  DispatchColumnGeneration`/`DispatchMeshingForDirtyChunks`). Never erase an
  in-flight `TaskT` while `getIsCompletedProperty()` is false — the
  underlying `std::async` future's destructor blocks until the task
  finishes unless already awaited.
- **Apply-cap discipline** (`CnaCraftGame::PollGenerationJobs`/
  `PollMeshJobs`): a completed job that exceeds the per-frame apply cap
  must be **deferred to a later frame, never discarded** — discarding is
  only safe for a job that's genuinely no longer wanted (its column loaded
  some other way first). This is *load-bearing*, not just tidy, for
  meshing specifically: `DispatchMeshingForDirtyChunks` clears the dirty
  flag at *dispatch* time, so a discarded-but-completed mesh result leaves
  that chunk permanently un-meshed with nothing left to ever re-flag it.
  A real bug from exactly this mistake (permanent holes in distant
  terrain) was found and fixed this session — don't reintroduce it.
- `CnaCraftGame::chunkRenderers_` — now `unordered_map<ColumnKey,
  array<unique_ptr<ChunkRenderer>, WORLD_CHUNKS_Y>>`, keyed identically to
  `World`'s own column storage.
- `Persistence::WorldStore` — schema now has Craft's real `p,q` columns.
  **No migration path** from a pre-2026-07-10 `world.db` — it's detected
  and rejected at open (logged, falls back to the no-op store), not
  silently corrupted.
- `Worlds/BlockType` — **new values must always be appended after the last
  one (`Dye31` as of this note), never inserted earlier.**
  `Persistence::WorldStore` persists `BlockType` as its raw `static_cast
  <int>` ordinal — inserting mid-enum would silently reinterpret every
  existing `world.db`'s saved block types on next load, with no error or
  warning. `Hotbar::kSlots` follows the same append-only convention for the
  same reason (existing slot numbers are load-bearing for muscle memory and
  tests). If you add a new block type, add it at the end of the enum and
  the end of `kSlots`, not wherever feels topically appropriate.
- `Worlds/WorldEditor` (new, plan.md §12.1 item 17) — engine-agnostic
  world-editing primitives + `Worlds::ExecuteCommand`, the `/`-command
  parser/dispatcher. Pure functions of their inputs (a `World&` plus plain
  coordinates/marks/clipboard/radii) — no CNA dependency, fully
  unit-testable, same pattern as `ChunkMesher`/`VoxelRaycast`/
  `PlayerController`. If you add a new command, add its dispatch branch to
  `ExecuteCommand` (mirrors Craft's real `parse_command` order) and its own
  `WorldEditor::` primitive if it needs new geometry, not inline logic
  inside `CnaCraftGame`.
- `CnaCraftGame::TypingMode` (replaces the old `isTypingSign_` bool) — a
  3-state enum (`None`/`Sign`/`Command`). If you add a third typing trigger
  (Craft's own bare-Enter-opens-empty-chat-buffer, not ported here since
  there's no multiplayer to chat with), add a new enum value, not a second
  bool — a bool can't represent "not typing" vs "typing, but which mode."
- `CnaCraftGame::mark0_`/`mark1_`/`clipboard_`/`radii_` — session state a
  command reads, updated by `RecordMark` (called at both break and place)
  and by `/copy`/`/view` respectively. `radii_` is the actual mutable
  source of truth for streaming/fog now — `kCreateRadius`/`kDeleteRadius`
  (the anonymous-namespace constants) are only ever read once, to seed
  `radii_` in `Initialize()`. Don't read the constants directly anywhere
  else, or a runtime `/view` change won't take effect there.
- `Render::Hud::PushMessage` — Craft's own 4-line message ring buffer
  (`MAX_MESSAGES`). Rebuilds its texture on every call (matches
  `SetTyping`'s "small texture, low-frequency interaction, per-call rebuild
  is cheap enough" reasoning) — don't call it every frame, only when a new
  message actually needs to appear.
- **Baked AO (new, item 12)** — `MeshVertex::shade` is the ENTIRE
  time-independent half of Craft's block lighting (`(1+df)*aoBrightness`,
  range [0.3, 2.0], cloud-compressed for `BlockType::Cloud`), computed by
  `ChunkMesher::ComputeOcclusion` + `BakedShade`. The renderer uploads
  `shade/2` as an 8-bit vertex color; `CnaCraftGame::Draw` restores the ×2
  via `DiffuseColor = 2*(daylight*0.3+0.2)`. **If you touch either half,
  keep the ×0.5/×2 pair in sync** — the calibration points are noon
  unoccluded top face = 0.789×texel, night = 0.315×texel (both exactly
  Craft's shader output). Every Draw pass now sets
  VertexColorEnabled/Lighting/DiffuseColor explicitly — the sky dome draws
  FIRST, so a forgotten white diffuse there shows up as "sky dims with
  daylight twice". The diagonal-flip comparison in the mesher uses RAW ao
  (higher = darker), never baked shade (inverted ordering). The mesher's
  occupancy comes from a padded per-Build snapshot filled in chunk-aligned
  slabs — keep fills going through `Chunk::GetBlock` + `GetBlockDef`
  (`solid && !transparent`, `World::IsOpaque`'s exact predicate), not
  per-cell `World::GetBlock` hash walks (that variant measurably ~2x'd the
  test suite). AO reads cross chunk borders: mesh-job snapshots must stay
  the full 27-chunk set and `World::SetBlock`'s cross-product dirty rule
  (diagonals + `ly <= 7` reaching the chunk below) is load-bearing — trim
  either and you get baked seams that only show at chunk corners/edges.

**Everything from before these sessions** (module list, boundaries, data
flow, signs, cursor-capture, floor-catch, etc.) is unchanged — see `plan.md`
§2/§6/§8 and prior `NEXT.md` history in git log for that detail if needed.
**Flying is no longer pitch-coupled** — see item 29 below (Minecraft-style
Space/Shift, a deliberate reversal of an earlier same-day Craft-fidelity
change).

**Boundaries that must not be broken** — unchanged, still load-bearing:
`Worlds/` must never `#include` anything CNA/SDL; any new `BlockDef`
boolean flag with a non-`false` default must be AND'd with `solid`
wherever consumed; two-mesh-buffer convention; never erase an in-flight
`TaskT` before it completes (new this session, see above).

## 7. Useful commands

```bash
# Engine-agnostic tests only (no CNA/GPU/display needed):
cmake -S . -B build-worlds -DCNA_CRAFT_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-worlds -j"$(nproc)"
ctest --test-dir build-worlds --output-on-failure
```
Expect: `WorldsSmokeTest` (303 `ok:` lines) and `PersistenceSmokeTest`
(40 checks) both pass.

```bash
# Full graphical game (requires ../cna and ../sharp-runtime as siblings):
cmake -S . -B build-easygl -DCNA_GRAPHICS_BACKEND=EASYGL
cmake --build build-easygl --target CnaCraft -j"$(nproc)"
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft          # interactive
SDL_VIDEODRIVER=x11 DISPLAY=:0 ./build-easygl/CnaCraft --smoke 30
```
Note: `SDL_VIDEODRIVER=x11` matters in sandboxes where `WAYLAND_DISPLAY` is
also set — otherwise SDL silently creates a Wayland surface that X11
screenshot tooling (`import -window root`) can't see, even though the
process runs fine and `DISPLAY` is set correctly. Delete any pre-existing
`world.db` before running this build — see README.md's reset note.

There is no separate lint/format tooling configured in this repo.

## 8. Next smallest tasks

`plan.md` §12.1 is the authoritative ordered priority queue. **Everything
implementable is done, and the multiplayer PLANNING pass is done too**
(`MULTIPLAYER_PLAN.md`, §3 item 20). What's actionable next:

- **Multiplayer implementation — ONLY with an explicit user go-ahead**:
  `MULTIPLAYER_PLAN.md` §11's four scope questions are now DECIDED
  (2026-07-11: cna-craft↔cna-craft only; C++ `CnaCraftServer` in this
  repo; no auth, guest + `/nick`; M7 = PIP observation + `@nick` PMs;
  real-Craft compatibility dropped). Everything is ready to start at
  phase M0 (protocol module + tests) the moment the user says start. Do
  NOT begin from the old plan.md §11.6 transport suggestion — the plan
  doc supersedes it.

Also legitimately available if the user prefers polish over planning:
one-keypress verifications on their real machine (F11 fullscreen, item 34;
Vulkan AO rendering — day/night was already user-verified 2026-07-11, see
§3 item 19), or the §11 backlog's remaining nice-to-haves (greedy meshing,
occlusion culling — neither user-requested yet).

If the user asks "what's next" with nothing else specified, offer this
short list rather than picking silently.

## 9. Do not do yet

Everything from prior sessions still applies (no multiplayer implementation
— planning only, per §8 — no broad `Worlds/`/`Render/` refactor, no
`PlayerController` public-API changes without re-running the full suite, no
`Hotbar::kSlots` reordering, no rushed Chat/slash-commands implementation,
no `SkyDome`/`SelectionOutline` winding "cleanup" without real screenshot
verification, no `Sign.hpp` face-convention change to match Craft's
asymmetric scheme) — **plus**:

- **Don't revert flying or arrow-key look back to Craft's exact scheme** —
  item 29 deliberately replaced Craft's pitch-coupled flying with
  Minecraft-style Space/Shift, and removed arrow-key look, per a direct,
  informed user request after trying the Craft-accurate version. This
  reverses item 24's earlier same-day choice; item 29's decision is the
  current one — don't "fix" it back toward Craft parity citing item 24 or
  CRAFT_PARITY.md's general fidelity goal, and don't re-add a dedicated
  Ctrl-descend-style binding either (Shift already owns fly-descend now).

- **Don't remove `Initialize()`'s single synchronous spawn-column load, and
  don't restore the full-radius one either** — this is the settled midpoint
  of a real back-and-forth: 169 columns synchronously = item 28's
  ~14-second invisible window; zero columns synchronously = item 31's
  player entombed in terrain with WASD dead. Exactly one column (~80ms) is
  deliberate. Similarly, don't "simplify away" `HealPlayerIfEmbedded` or
  the nearest-first ordering in `UpdateStreaming` — all three parts of
  item 31's fix are load-bearing, and the heal also repairs `world.db`
  files poisoned while the regression was live (which outlive any code
  fix).

- **Don't set `kPitchLimit` back to the literal exact `π/2`** — item 30
  found and fixed a real rendering-corruption bug this caused (`Matrix::
  CreateLookAt`'s `Cross3(Up, forward)` degenerates when they're exactly
  parallel). The small epsilon backoff (`π/2 - 0.01f`) is load-bearing, not
  a rounding artifact to "clean up" toward a nicer-looking constant or
  tighter Craft-parity match.

- **No re-running the chunk-system redesign, terrain-formula, fly-speed,
  pitch-clamp, reach-distance, arrow-key-speed, plant-rotation, or
  player-position-persistence changes** — all already done (see §3); don't
  re-investigate these as if they were still open.
- **AO is DONE — don't re-implement it, and don't "restore" the old
  terrain lighting.** Specifically: don't re-add normals/`EnableDefaultLighting`
  to chunk terrain (the 3-light rig with specular was NEVER Craft-accurate;
  its removal was a deliberate parity win, and signs still use it — that
  split is intentional); don't "fix" plants being unshaded by the ground
  they stand on (Craft's own scalar `min_ao` behavior); don't swap the
  flip comparison to baked shade (it must use raw ao — see §6); and the
  old "EASYGL-only custom ShaderEffect" scope decision is SUPERSEDED, not
  pending — the shipped approach beats it on every axis (see CRAFT_PARITY.md
  §5.1's correction notes).
- **`SignBillboard`'s single-winding fix (item 26) shipped without live
  screenshot re-verification** (sandbox keyboard-text-input broke
  mid-session) — if you're back in an environment where text input works,
  a real verification screenshot (place a sign, look at both sides) would
  be genuinely useful and closes a real gap, it's not busywork. Don't
  re-derive or re-guess the winding itself, though — the fix (single
  correct winding, 6 indices) is believed correct on strong indirect
  evidence (identical convention proven for every other face all session).
- **Light toggle's glow does not propagate to neighbors or attenuate by
  distance — this is intentional** (item 27, CRAFT_PARITY.md §5.1's engine
  constraint), not an unfinished feature. Don't "fix" it into a
  neighbor-propagating system without a new engine-level vertex-format/
  shader-dispatch capability being added first (a much larger change) and
  a fresh user decision to pursue it.
- **Don't discard a completed-but-over-cap background job in
  `PollGenerationJobs`/`PollMeshJobs`** — defer it to a later frame
  instead. See §6's apply-cap note; this exact mistake caused a real,
  hard-to-spot visual bug this session (permanent holes in distant
  terrain) that only showed up under real interactive play, not in any
  unit test.
- **Don't capture a live `World` (or anything move-only) by value in a
  lambda passed to `TaskT::Run`** — it won't compile (`std::function`
  needs the closure copy-constructible), and even if it did, background
  code must never touch the live `World` concurrently with the main
  thread. Capture plain copyable snapshot data instead.
- **Don't increase `kCreateRadius` without also addressing culling** — see
  §4's note; render cost scales with loaded-column count and nothing
  culls off-screen chunks today.
- **Never insert a new `BlockType` value earlier than the last one, and
  never reorder `Hotbar::kSlots`** — see §6's note; both are append-only
  for real persistence-compatibility and test-stability reasons, not just
  tidiness.
- **Don't add a bare-Enter "chat" typing trigger** — Craft has one
  (opens an empty typing buffer sent as multiplayer chat), deliberately not
  ported since there's no multiplayer to talk to. Don't add it "for
  completeness" without a real reason (e.g. multiplayer actually landing).
- **Don't read `kCreateRadius`/`kDeleteRadius` directly outside
  `Initialize()`** — everything else must read `radii_` (`Worlds::
  CommandRadii`), the actual mutable value `/view` changes at runtime. The
  constants are seed values now, not the live source of truth.

## 10. Resume prompt

```
Read CRAFT_PARITY.md first (the authoritative Craft-vs-cna-craft parity
audit), then plan.md §12.1 (the ordered priority queue derived from it) —
as of the last session (2026-07-11), **33 of 34 items are completed and
nothing is blocked**; the only remaining item is multiplayer, whose
DESIGN is done and fully scoped (`MULTIPLAYER_PLAN.md` — read it before
any multiplayer work; its §11 records the user's four scope decisions of
2026-07-11) but whose implementation requires an explicit user go-ahead
(standing decision — don't write networking code without one; when it
comes, start at phase M0). Highlights: item 19 chunk streaming, item 25
the 54-block roster, item 17 chat/slash commands, items 26-34 the parity
audit's follow-ups (controls, light-toggle glow pass, sky dome, F11), and
item 12 **ambient occlusion** — baked per-vertex Craft AO via
`MeshVertex::shade` + vertex-color terrain, shipped with zero engine work
on ALL backends after planning research disproved the recorded
"EASYGL-only ShaderEffect" premise (see CRAFT_PARITY.md §5.1's correction
notes, and §6's baked-AO invariants before touching the mesher/renderer:
the ×0.5/×2 shade-diffuse pair, raw-ao flip comparison, 27-chunk
snapshots, cross-product dirty rule). **If you touch
`PlayerController::kPitchLimit`, read item 30 first — don't set it back
to the literal exact π/2. If you touch Initialize()'s spawn loading,
UpdateStreaming's ordering, or HealPlayerIfEmbedded, read item 31 first —
the one-synchronous-column arrangement is the settled midpoint of two real
user-reported bugs in opposite directions.** If the user doesn't specify
what to work on, offer §8's short list rather than guessing. Before
implementing anything that cites Craft's source code, re-verify the
citation against the real checkout at
/rv/data/development/github.com/other/Craft — CRAFT_PARITY.md was
carefully audited but has twice now contained real errors (owned in §2.6
and §5.1's notes). **Don't revert item 29's controls back toward Craft
parity** — a deliberate, informed reversal; see CRAFT_PARITY.md §1.6. Make
one small, verified improvement at a time: implement, build (cmake --build
build-worlds, or the full EasyGL build if it touches Render/ or
CnaCraftGame), run the relevant test/smoke command, confirm it actually
passes — and for anything touching CnaCraftGame/Render, verify visually
via a real Xvfb/xdotool/ImageMagick screenshot cycle where possible
(SDL_VIDEODRIVER=x11 required; as of 2026-07-11 synthetic KEYBOARD works
again — use explicit `xdotool keydown/keyup --window <id>`, not the
instant `key` form — while MOUSE stays dead; a hand-crafted `world.db`
with `block`/`light`/`state` rows is the proven substitute for
mouse-dependent scene setup, including exact camera aim via `state`'s
yaw/pitch — see §3 item 19 and §4). If you touch the
chunk-streaming/threading code (World column storage, CnaCraftGame's
Dispatch*/Poll* methods), re-read §6's TaskT/apply-cap notes first —
there's a real, subtle, already-found-once bug class there (discarding
instead of deferring completed-but-over-cap jobs). If you touch
WorldEditor/ExecuteCommand, re-read §6's PasteRegion note first — a
same-World overlapping paste can clobber source data mid-copy, a real
Craft quirk, not a bug to "fix." If you add any new combined vertex
format, re-read item 27's plan.md writeup and CRAFT_PARITY.md §5.1 first —
CNA backends dispatch shaders by hardcoded vertex-buffer byte stride, and
the 3D path ignores ShaderEffect entirely; that's why real torch-light
propagation (whose `min(1, daylight+light)` does NOT factor like AO did)
still needs genuine engine work. When finished, update plan.md §12.1's
status, update CRAFT_PARITY.md's corresponding entry, and update this
file's "Current status"/"Recent changes".
```
