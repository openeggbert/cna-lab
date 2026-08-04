# NEXT — continuity notes

> Short-term state for whoever picks this up next, human or otherwise. The long-lived plan is
> [`plan.md`](plan.md); the reasoning behind the architecture is [`ANALYSIS.md`](ANALYSIS.md).
> This file records what is true *right now*: what just landed, what is half-done, what is known
> broken, and where to start.

**Branch:** `claude/cna-editor-architecture-plan-l4jza7` — push only, no pull request (owner's call).

---

## Status at a glance

| | |
|---|---|
| Build (standalone, no CNA) | ✅ clean at `-Wall -Wextra -Wpedantic -Werror` |
| Build (`-DCNA_EDITOR_WITH_CNA=ON`) | ✅ clean |
| Unit tests | ✅ 413 / 413 (also under Clang Release) |
| CTest (standalone) | ✅ 12 / 12 |
| CTest (CNA config) | ✅ 17 / 17 |
| CI | ✅ Linux, GCC Debug + Clang Release, `-Werror` |
| **Phase 1** | ✅ **complete** — all 23 tasks |
| **Phase 2** | 🔄 10 of 12 done; only ED-302 and ED-311 remain, and both are half done and blocked on something real |
| **Phase 3** | 🔄 5 of 13 done — ED-400 (3D camera and wireframe view), ED-401 (2D gizmos), ED-408 (3D translate), ED-409 (3D rotate **and scale**), ED-405 (**glTF importer, and the 3D view drawing what it imports**). The eight that remain wait on ED-402 rather than on the pipeline, which now exists |
| **Phase 5** | 🔄 ED-510, ED-511 and ED-513 done — the backend comparison mode, end to end |
| **Owner priorities** | ✅ **all four closed**: robustness and data safety; live editing into the player; production 2D tools; backend comparison |

---

## Start here

You are picking up a branch with nothing half-finished on it: the working tree is clean, every
commit is pushed, and all 413 tests pass in three configurations. There is no rescue work to do
first.

1. Read the rest of this file, then `plan.md`'s *Current state*. `ANALYSIS.md` only when a
   *why* is unclear — its decisions (D-01 … D-15, findings F-01/F-02) are cited by id everywhere.
2. Rebuild and run the checks in *Validation commands* below before changing anything, so a later
   failure is yours rather than inherited. The standalone build needs no CNA checkout at all.
3. Pick from *Where to start next* at the bottom. Nothing there is blocked on anything in this
   repository except where it says so.

**Standing constraints** (owner's, still in force): develop and push only on
`claude/cna-editor-architecture-plan-l4jza7`; **no pull request**; everything in this repository is
written in English; do not bump a `formatVersion` without the owner's say-so; do not open issues or
pull requests against `openeggbert/cna` — the CNA gaps G-01…G-04 stay documented here.

---

## What landed in this session

**ED-405 is closed: the editor imports glTF, and the 3D view draws what it imports.** That is the
task the owner named after the wireframe screenshots — *make the 3D view look like a 3D view* — and
the honest scope of it is that a `ModelRenderer` is now its own geometry instead of a badge. It is
not yet lit, shaded or textured; that is ED-402, and it is next.

**1. The seam, before the parser — which is what plan.md's ED-405 row insisted on.**
`CNA/Editor/Core/MeshData.hpp` is the interface between the importer that produces geometry and the
two things that will draw it. Three decisions are recorded there, and every consumer inherits each
one rather than deciding for itself:

- **It lives in `cna-editor-core`, which is neither party's module.** The producer is
  `cna-editor-assets` and the first consumer is `cna-editor-scene`; those two link nothing but core
  and cannot see each other. Core is the only place both can reach — the same reason `EditorMatrix`
  is in core while the camera that uses it is in scene.
- **The geometry arrives already in the editor's world convention.** glTF is Y-up; this editor's
  world is Y-down so the 2D and 3D views agree about which way is down. The mirror is applied
  **once, into the data**, at import. Mirroring at draw time instead would mean the wireframe path
  and the CNA path each have to remember, and the day one of them forgets is the day models are
  upside down in one view and not the other.
- **No node hierarchy.** glTF has one; the importer bakes it into vertex positions. A hierarchy is
  worth keeping only if something animates it, nothing does, and a schema designed against no
  consumer is exactly the mistake ED-311's `NestedStructure` is parked to avoid. It comes back as a
  field beside `parts` when skeletal animation is a real task, and nothing already written changes.

A vertex matches XNA's `VertexPositionNormalTexture` field for field. That is the whole of the
concession this CNA-free file makes to CNA, and it costs nothing: ED-402's upload becomes a copy
rather than a translation.

**2. The consequence of the mirror that is easy to miss, and why the importer got a test suite
rather than a smoke test.** Mirroring one axis reverses the handedness of every triangle in the
file. Left alone, every model is inside-out the moment ED-402 turns backface culling on — and looks
perfectly correct until then, in wireframe, where winding does not matter. So the importer reverses
the winding back, and `meshWindingMatchesNormals` states the property that reversal exists to keep.
A node that is *already* mirrored — a negative scale, which authoring tools produce routinely — has
flipped once already and the two cancel, so the test is on the sign of the whole transform rather
than on the mirror alone. Both cases are pinned, and both were **confirmed to fail with the fix
removed**: a test that still passes when the code is deleted is not a test.

**3. cgltf, vendored — and its symbols prefixed, which is not decoration.** `third_party/cgltf/`
holds the same version CNA vendors, copied rather than reached for across the sibling checkout: the
default build has no CNA in it (D-03), and CNA's own reader is `CNA::Internal::GltfImport`, which
D-01 forbids. cgltf declares its whole API inside `extern "C"`, so a build linking both copies fails
outright with *multiple definition of `cgltf_parse`*; `cgltf_prefixed.h` renames ours.

**That failure appears only in the CNA configuration, and only at the final link** — the standalone
build and CI both link cleanly with the clash present. It was found by building `build-cna` before
committing, which is the argument for that step rather than a story about it. Anyone vendoring a
second library CNA also vendors should expect the same and check the same way.

**4. The seam is proved by a consumer, not by assertion.** `SceneWireframe` draws a
`ModelRenderer`'s real mesh through `MeshProvider` — each edge once, since an interior edge shared
by two faces drawn twice makes a dense model a solid blob. A model too dense for the segment budget
is sampled at a **stride** rather than cut off, so what appears is the whole shape drawn sparsely;
stopping at the budget would show a model with a bite out of it, which reads as broken geometry
rather than as a full view.

This matters more than one more feature. It means ED-402 inherits a `MeshData` already known to
survive the trip, checked in CI with no GPU, instead of discovering the seam's problems while also
writing a `VertexBuffer` pass. `MeshCache` sits in `EditorContext` rather than the viewport for the
same reason — a mesh needs no CNA, so the **standalone** build draws real models too.

**5. Things found by using it, rather than designed in.**

- **A remembered "no" must not outlive its reason.** `MeshCache` caches failures so a broken file is
  not re-parsed every frame — but *not* the case where the database has never heard of the id. That
  answer changes on its own (an asset mid-import, a scene opened before the scan finished), and
  caching it would leave a model permanently invisible after the scan that would have found it. A
  test asking for an entry count is what surfaced the distinction.
- **`--orbit=YAW,PITCH`.** The same argument that put `--view=3d` here, one step further: that flag
  makes the 3D view reachable from a script, this one makes it reachable from an angle. A 3D feature
  photographed head-on is photographed in the one pose where a mesh and a flat sprite look alike, so
  it could not show whether the models had arrived. The screenshot verifying this session used it.
- **The asset watcher drops cached meshes too.** It already dropped cached *textures* on an
  external edit, for the reason written beside it — otherwise the editor reports the change and
  goes on drawing the art from before it. A mesh is the same bargain, and was noticed while writing
  up what was left undone rather than while writing the cache. Editing a `.gltf` with the editor
  open now refreshes the 3D view; a model whose file is deleted stops being drawn.
- **The example project has a crate in it.** `Assets/Models/Crate.gltf` and an entity in `Level01`:
  the 3D view had nothing to draw otherwise, and a pipeline demonstrated only by its own tests is
  one nobody can look at. It cost the scene-loader demo its hardcoded "three entities" — updated,
  and given a check that a model renderer carries no sprite, which is what a loader assuming
  otherwise would get wrong.

---

## What landed in the previous session

**ED-409 is closed and the 3D view is a complete editor**, plus two of the smaller items that were
queued behind it. Five commits, each validated in the standalone, CNA and Clang-Release
configurations before it was pushed.

**1. A bug in last session's rotate gizmo, found by writing the selection-wide half.**
`RotateGizmo3DDrag::update` composed the turn as `start * delta`, which applies the angle about the
entity's *own* axes; the header, the comment beside the line and the 2D gizmo it was written to
mirror all say the turn happens in world space. The two orders differ by exactly the rotation the
entity already has, so the existing test -- which turns an entity at identity -- could not see it.
Anything already lying on its side turned about a ring nobody had drawn. The new test pins the
*property* rather than an angle: a turn about world Z leaves anything pointing along world Z where
it is, whatever angle the drag came out as.

**2. ED-409's scale gizmo**, the last of the three manipulators and the one whose problem the other
two do not have. The resolution is that it does not need the world at all. Translate needs a *line*
to slide along and refuses when the arm points at the camera; rotate needs a *plane* to measure an
angle in and drops a ring seen edge-on; a scale factor is a **ratio of screen distances**, and the
screen always has one.

So an arm here is never dropped and never refuses. Its *direction* is the true projection of the
axis, clipped against the near plane through the wireframe's own `projectSegment`; its *length* was
a chosen constant to begin with, so it is bounded at both ends -- floored at thirty pixels so an
edge-on handle stays clear of the centre one, capped so an arm pointing nearly at the eye does not
throw its handle off the panel. Foreshortening fades toward a floor rather than to nothing, which is
the honest thing to say about an axis that is still there and has little room left to be precise in.
The one case an arm *is* dropped is an axis pointing exactly through the eye, where it projects onto
its own origin; the centre handle (`GizmoAxis3D::All`, which scales all three axes) covers those
pixels, and it is what a press in the middle of a gizmo means anyway.

The panel's 3D interaction was turned inside out on the way: hover and grab now ask one question of
one layout in `beginGizmo3DDrag`, instead of one layout per manipulator computed in parallel and two
near-identical begin-a-drag blocks.

**Verified by screenshot on EASYGL**: red, green and blue arms with square handles, the white centre
square, and the near-edge-on Z arm sitting at its floor instead of vanishing under the middle.

**3. All three 3D manipulators act on a whole selection**, about its shared pivot. `MultiTranslate3D`
is now `MultiTransform3D` and answers all three gestures, as `MultiTransformDrag` has in 2D: the
single-entity drags compute the gesture and this turns one gesture into the edits a selection needs.
Rotate and scale carry their members *around* the pivot as well as changing them. Scale does that in
the **gizmo's** frame rather than in world components, because the offsets have to grow along the
same arms the sizes do.

That needed the gizmo to move too, so the pivot is an argument to the three layout functions rather
than something the drawing does afterwards -- where a gizmo is drawn is where it must be grabbed.
While a drag runs the pivot is the one captured at the press, not a fresh average, which would chase
the entities and bend a steady drag into a spiral.

**4. A grid the 3D view can put on the floor.** `WireframeOptions::gridPlane`, offered from the View
menu and only in the 3D view. The default is unchanged and stays the scene's own XY plane, because
that is where everything this editor can currently place lives; the floor is what ED-402 will want.
One function either way -- the two planes differ by which pair of axes is in the plane and nothing
else, so a second function would be the same loop twice, free to drift.

**5. A key for the 2D/3D view**, on the digits that name it: `2` and `3`, two keys rather than one
toggle, because a key named for the view it selects is safe to press when the user is already there.
Digits are also free while flying, which W, E and R are not. `UiKey` gained `Digit2` and `Digit3`,
appended rather than inserted.

---

## The session before that

**Both of the owner's choices were done, and three more things with them.**

**1. ED-400, the 3D viewport.**

- **`EditorMatrix` and `EditorCamera3D`.** The repository had no matrix type and not one vector
  operation before this -- an orthographic view of a plane gets by on scalars. Both mirror XNA's
  conventions exactly (row-major, row vectors on the left, right-handed, depth to [0, 1]), because
  a camera that agreed with itself but not with the runtime would show the scene mirrored the
  moment anyone pressed Play. Orbit and fly are two ways of moving one state rather than two modes.
- **`SceneWireframe.hpp`**: which lines the 3D view draws, decided CNA-free and tested in CI, with
  the renderer reduced to stroking segments. Near-plane clipping shortens a segment rather than
  dropping it, the grid spacing is the 2D viewport's own `chooseGridSpacing`, and a truncated
  wireframe says so. 3D picking (ray against bounds, nearest wins) came with it.
- **The view, wired in**: a 2D/3D button in the viewport toolbar, a View menu item, `--view=3d`,
  navigation (middle-drag orbits, right-drag turns in place, Shift pans, wheel dollies, WASD+QE fly
  while the right button is held), picking through the 3D projection, and Frame Selected following
  whichever camera is on screen.

**Verified by screenshot on EASYGL**: perspective ground grid to a horizon, red X and blue Z axes,
and boxes around all three entities of HelloSprites.

**Two bugs the screenshot found**, neither of which any test would have:

1. `setViewport()` carried the 2D camera across a viewport swap and dropped the 3D one. The
   windowed host installs its real viewport *after* `initialize()`, so `--view=3d` aimed the camera
   at the scene and then threw the aim away. Three screenshots of an empty grid before it was
   understood.
2. Entering the 3D view left the camera at its constructed default, which shows empty grid for any
   scene laid out away from the origin. It now frames the scene on first entry -- only the first,
   so a later toggle does not throw away an orbit.

**One convention to hold on to**, recorded in `plan.md` too, and *changed late in the session at
the owner's instruction* ("ano, ať 3D odpovídá 2D vizuálně"): **the 3D camera is Y-down**, like the
2D one and like `SpriteBatch`, and its grid is the scene's own XY plane rather than a ground plane
under it. So an entity below another in the 2D viewport is below it in the 3D one, and a 3D camera
at yaw and pitch zero shows exactly what the 2D camera shows -- which is also why the default pitch
is now zero: the user orbits away from a picture they recognise.

The arithmetic stays Y-up and right-handed; the *projection's* Y is mirrored, which is the actual
conversion between the two frames. Mirroring the view's up vector instead would have been a
180-degree roll -- vertical fixed, world +X on the left. The mirror is inside the one
view-projection that `worldToScreen` and `screenToRay` both use, so picking, the gizmo and the
wireframe cannot disagree with what is drawn; `TheThreeDimensionalViewAgreesWithTheTwoDimensionalOneAboutWhichWayIsDown`
pins all of it.

**This is a bill ED-402 will have to pay.** XNA's 3D side is Y-up -- `CreateLookAt`, `BasicEffect`,
every loaded model -- so the model pass must apply the same mirror or models will disagree with
everything around them. A ground-plane grid probably becomes the right one then; `buildSceneGrid`
is where that choice lives.

**2. ED-408, a 3D translate gizmo** — added because a view you can select in but not move things in
is a viewer rather than an editor. `TransformGizmos3D.hpp`, separate from the 2D manipulators
because almost nothing is shared: those lay out in screen space, this one lays out in the world and
asks *where along this world line is the cursor pointing*. Arms are sized in pixels and converted at
the entity's own depth; an arm pointing at the camera is refused rather than answered; a child
stores a parent-relative position (`worldDeltaToLocal3D`). A whole selection drags as one undo
entry, through `MultiTranslate3D` beside the single-entity drag, exactly as the 2D viewport does it.
**Rotate followed it** (ED-409, half): three rings, *sampled and projected* rather than described,
because a circle in the world is an ellipse on screen and the grab has to answer in pixels — one
polyline serves both the drawing and the hit-test, so what a user sees is what they can take hold
of. A ring seen edge-on is dropped: it would project to a line through the centre, overlap the
other two, and have no plane a drag could measure an angle in. The drag turns in world space and
stores in the parent's frame, wraps its delta into (-pi, pi], and snaps the *turn* rather than the
absolute angle. Scale and the selection-wide halves landed this session, along with a fix to the
order that turn was composed in.

**Icons, so the 3D view is not ten identical cubes.** A camera, a light and an audio source all
draw nothing and were all the same wire box. Each has a badge now — camera body and lens, point
with four rays, cone with a wavefront, flat-drawn cube for a model — sized in *pixels*, like the 2D
viewport's icons and for the same reason, and keyed off the same `getEditorIconKind` so the two
views cannot disagree about what an entity is.

**3. Input forwarding into the running player** (the owner's second choice). State, not events,
because an XNA game polls and because a lost snapshot is corrected by the next one while a lost
key-down leaves a key stuck forever. The pointer travels with the surface it was measured against;
the *player* maps it into its own window, and the reply carries the player's own view, which the
Diagnostics panel shows. What the player does with it is deliberately nothing: `PlayerHost::getInput()`
is where a game would read it once there is a way to attach game code to an entity, and that
question stays closed.

**4. A project-settable snap step.** `gridSnap` in `.cnaproject`, additive exactly as `layers` was,
with `SetProjectGridSnapCommand` beside `SetProjectLayersCommand` and the field in the idle
Inspector. Zero means "use the visible grid", which is what an older project means.

**5. Documentation, smoke tests and two loose ends.** `docs/FORMATS.md` gained `gridSnap`; the README gained a
command-line options table (it had none, despite using the flags in its own examples).
`EditorMatrix::transpose` was removed as dead code, and `PlayerInputSnapshot::middleButton` -- on
the wire, compared, printed, and never set -- is now filled from the interaction. ctest gained
three cases for the 3D path: headless, against a real device, and `--view=isometric`, which must
fail rather than start quietly in 2D.

---

## Before that

Fourteen commits, each validated in three configurations before it was pushed:

`ED-401` rotate and scale gizmos with a local/world toggle · `ED-246` **`cna-player` draws the scene**
· `ED-510` backend comparison · `ED-511` the same from the command line, with an exit code ·
`ED-513` per-backend player builds from one configure · gizmo snapping and Ctrl+click selection ·
an interaction boundary on `CommandHistory` · an audio preview that can actually be stopped · a
gizmo that manipulates a whole selection · JPEG dimensions · delete and duplicate as one undo entry
· the gizmo mode and space shown in the viewport toolbar · two validation rules for states that
otherwise fail in silence.

The two that matter most to anyone picking this up: **the player draws now**, so play mode and hot
reload are things you can look at rather than read about in a log; and the **backend comparison
works against two real backends**, which is what finding F-01 was always supposed to buy.

Nothing in that list changed a file format. Every format is still at version 1.

---

## Validation commands

```bash
# Standalone: no CNA checkout, no GPU, no window.
cmake -S . -B build && cmake --build build -j8
./build/tests/cna-editor-tests
ctest --test-dir build

# With CNA. Needs ../cna, ../sharp-runtime, ../easy-gl, ../meta-gl and CNA's SDL submodules
# (git -C ../cna submodule update --init --depth 1).
cmake -S . -B build-cna -DCNA_EDITOR_WITH_CNA=ON -DCNA_DEVICES=ON -GNinja
cmake --build build-cna -j8 --target cna-editor cna-editor-tests cna-player
ctest --test-dir build-cna -R "CnaEditor|CnaPlayer|CnaScene"

# The CNA-config ctest has one test labelled needs-display. Configure the display it should use:
#   cmake -S . -B build-cna -DCNA_EDITOR_TEST_DISPLAY=:99
# and have a server there:  Xvfb :99 -screen 0 1600x900x24 &
# `xvfb-run -a` works for one-off runs but picks its own display, which that test will not see.

# Two backends, and the comparison between them. The second player is a full CNA build, so this
# is minutes rather than seconds.
cmake -S . -B build-cna -DCNA_EDITOR_WITH_CNA=ON -DCNA_EDITOR_PLAYER_BACKENDS="SOFTWARE" -GNinja
cmake --build build-cna -j8 --target cna-player-backends
DISPLAY=:99 ./build-cna/cna-editor --compare-backends \
    --project=examples/HelloSprites/HelloSprites.cnaproject
# Exit 5 when they differ, 0 when they agree. --tolerance=N sets how close counts as the same.

# The player, drawing the game on its own.
DISPLAY=:99 ./build-cna/cna-player-easygl --project=examples/HelloSprites/HelloSprites.cnaproject \
    --frames=20 --screenshot=/tmp/player.png

# Headless smoke, and a screenshot of the real window.
./build/cna-editor --headless --project=examples/HelloSprites/HelloSprites.cnaproject
DISPLAY=:99 ./build-cna/cna-editor --project=examples/HelloSprites/HelloSprites.cnaproject \
    --frames=40 --screenshot=/tmp/editor.png

# The 3D viewport (ED-400). --view=3d exists so this picture can be taken at all, and
# --orbit=YAW,PITCH (ED-405) so it can be taken from an angle -- head-on is the one pose in which
# a real mesh and a flat sprite look alike, so it cannot show whether models arrived.
DISPLAY=:99 ./build-cna/cna-editor --project=examples/HelloSprites/HelloSprites.cnaproject \
    --view=3d --orbit=35,25 --frames=40 --screenshot=/tmp/editor3d.png
# The example's Crate entity is the model to look for: a cube with visible triangle edges, quite
# unlike the flat rectangles the two sprites draw.
```

Do **not** run `cmake --build build-cna` without a target list: CNA's own examples fail to compile
in this environment (`examples/common/PixelTestGame.hpp` cannot find `SDL3/SDL.h`). That is a CNA
build issue, unrelated to the editor, and naming the editor targets sidesteps it entirely.

---

## Decisions taken by the owner (2026-08-04, later — after seeing the 3D screenshots)

The owner's reaction to the wireframe screenshots was "a v tomto mám vyvíjet 3D AAA hry???". The
answer given, and not softened: no. CNA is XNA-class, the reference titles are Terraria and
Stardew, and no amount of work here turns it into Unreal. What *was* conceded is that the 3D view
showing wireframe boxes is a consequence of the model pipeline being unbuilt, not of the framework's
ceiling. The decisions below follow from that.

| Question | Answer |
|----------|--------|
| Target | **Still XNA class, but make the 3D view look like a 3D view.** The priority is that it draws real models instead of wire boxes. |
| Next task | **ED-402 — model rendering.** |
| Where the meshes come from | **ED-405 first: the glTF importer**, built on the `cgltf` CNA already integrates. The longer road, and the one that does not depend on the content pipeline. So the order is ED-405, then ED-402. |
| Branch, CI, CNA gaps, additive fields | All unchanged: same branch and no pull request; CI standalone only; no issues against `openeggbert/cna`; additive fields in the persisted formats need no permission. |

**The one precondition to get right on day one**, already in `plan.md`: the model pass must apply
the same Y mirror the rest of the 3D view uses. XNA's 3D side is Y-up, this camera is Y-down, and a
model pass that skipped the mirror would put models upside down relative to the grid, the gizmos
and every sprite around them.

**Where to start**, concretely: ED-405 is an *importer*, so it lands in `cna-editor-assets` beside
the existing ones and produces an asset record plus a sidecar, exactly as textures do. Read
`AssetImporters.hpp` first. The mesh data it produces then needs a home the CNA-linking viewport
can read — that is the seam to design before writing the parser, because getting it wrong means
writing ED-402 twice.

---

## Decisions taken by the owner (2026-08-04, for the next session)

| Question | Answer |
|----------|--------|
| Phase 3, given that CNA ships `BasicEffect`, `VertexBuffer` and the rest | **Start ED-400**: perspective/orthographic viewport camera with orbit and fly navigation. Do the maths CNA-free first, as `EditorCamera2D` was done — it needs no model pipeline. ED-402/ED-404 still wait. |
| Additive fields in `.cnaproject` / `.cnascene` / `.cnaasset` | **Allowed without asking**, exactly as `layers` was added: no `formatVersion` bump, old files still open, and a round-trip test proving it. Changing or removing an existing field, or bumping a version, still waits for the owner. |
| How far play mode goes | **Add input forwarding**: keyboard and mouse reach the running player and the editor reports what it sees. No scripting or behaviour hook — that question shapes the document model and stays closed. |
| Branch and pull request | Unchanged: `claude/cna-editor-architecture-plan-l4jza7`, **no pull request**. |
| CI | Unchanged: standalone configuration only — no sibling checkouts, no Xvfb, no backend-comparison job. |
| CNA gaps G-01…G-04 | Unchanged: documented here, **no issues or pull requests against `openeggbert/cna`**. |
| Verification | Keep screenshot-verifying UI work on EASYGL under Xvfb. |

---

## Decisions taken by the owner (2026-08-03)

| Question | Answer |
|----------|--------|
| **ED-250 / Q-02** — how a game consumes a compiled scene | **Header-only loader shipped from this repository.** The game includes a header that reads `.cnascene` through CNA's public API. CNA stays untouched, which keeps the D-01/D-03 boundary intact, and the choice is reversible: if a CNA module turns out to be better later, the loader moves into it without the format changing. |
| Priorities after Phase 1 | In order: robustness and data safety → live editing into the running player → production 2D tools → backend comparison mode. |
| CI | GitHub Actions, Linux, standalone configuration only. No sibling checkouts, no submodules. |
| Pull request | None. Push to the branch. |
| CNA gaps G-01/G-02/G-03 | Leave documented here. Do not open issues or pull requests against `openeggbert/cna`. |

---

## Standing assumptions

- **Everything that changes a document is a command** (ANALYSIS.md D-06). This now includes the
  asset database, not only the scene: `SetImporterSettingCommand` goes through the same history.
  An editor where some edits undo and others quietly do not is worse than one where nothing does.
- **Asset identity is a Uuid, never a path** (D-08). Moving or renaming an asset must not touch a
  single scene file. Anything in ED-220 that moves an asset has to keep that true.
- **File formats stay backward compatible.** `formatVersion` is not bumped without the owner's
  say-so; ED-902's migration work reads old versions rather than writing new ones. If a breaking
  change ever looks necessary, it goes here and in `plan.md` prominently, and waits.
- **`cna-editor-viewport` is the only module that may link CNA** (D-03). The build graph enforces
  it: a stray `#include <Microsoft/Xna/...>` anywhere else fails to compile.

---

## Recently completed

Newest first. Each is a single commit on the branch.

- **A key for the 2D/3D view.** `2` and `3`, two keys rather than one toggle: a key named for the
  view it selects is safe to press when the user is already there, and digits stay free while flying
  where W, E and R do not. `UiKey` gained `Digit2` and `Digit3`, appended rather than inserted.
- **A grid the 3D view can put on the floor.** `WireframeOptions::gridPlane`, on the View menu and
  only while the 3D view is on -- the 2D viewport has one plane and no decision to make about it.
  The default is unchanged. The test pins it through the property that made that default worth
  having: a camera with no pitch lies *in* the ground plane and sees one line where the scene's own
  plane fills the view, and tipping it over the floor swaps the two round.
- **All three 3D manipulators act on a whole selection**, about its shared pivot. `MultiTranslate3D`
  became `MultiTransform3D`; rotate and scale carry their members *around* the pivot as well as
  changing them, and scale does that in the gizmo's frame rather than in world components, since the
  offsets have to grow along the same arms the sizes do. The pivot is an argument to the layout
  functions rather than something the drawing does afterwards: where a gizmo is drawn is where it
  must be grabbed.
- **ED-409's scale gizmo**, the last of the three. An arm is never dropped and never refuses,
  because a scale is a ratio of screen distances and the screen always has one; it shortens to a
  thirty-pixel floor and fades instead. Verified by screenshot on EASYGL.
- **The 3D rotate gizmo turns in the world, as it always said it did.** `start * delta` applies the
  angle about the entity's own axes; the two orders differ by exactly the rotation the entity
  already has, which is why a test at identity could not see it.

- **Two validation rules for states that fail silently.** A tilemap with a tile size of zero draws
  nothing *and* swallows every brush stroke, saying neither; an animation with no sheet leaves the
  sprite beside it drawing a placeholder, which looks exactly like a broken asset reference and is a
  different problem with a different fix. Both are warnings, since a component added a moment ago
  and not yet configured is a normal state -- what is not normal is finding out by wondering why
  painting does nothing.
- **The viewport toolbar shows the gizmo mode and space.** `W`/`E`/`R` and `X` could change them
  and the menu could set them, but nothing *displayed* them -- a user who could not see which
  manipulator was active had to press a key to find out what it had been. The space button is
  labelled with the space it is in rather than the one pressing it selects, because a toolbar
  reports state; the menu still says "Use Local Space", because a menu item is an instruction.
- **Delete and duplicate a selection as one undo entry**, plus JPEG dimensions. Both already acted
  on the whole selection; what they did not do was undo as one action. `CompositeCommand` (new, in
  core) holds several commands and undoes them in reverse, which is also what makes it safe to build
  out of commands that depend on each other's effects. Delete takes the selection's *roots* -- the
  same helper the gizmo uses -- since a delete carries the subtree with it and asking again for a
  child would push a command that finds nothing.
  Fixed on the way: `TwoRealPlayersAreComparedAgainstEachOther` was flaky. It told the two captures
  apart by looking for "-2" anywhere in the path, and the scratch directory's name contains a Uuid
  -- so the test passed or failed depending on random hex. It matches the filename stem now.
- **A gizmo on a multi-selection.** With Ctrl+click able to build one, the gizmo being stuck on the
  primary selection had become the obvious gap. It now sits on the selection's **shared pivot** --
  the average of the members' world positions, not the bounding box's centre, which would move when
  an entity was merely rotated -- and a drag moves, turns or scales all of them about it. Rotation
  and scale carry the members *around* the pivot as well as changing them, which is the difference
  between rotating an arrangement and spinning each of its parts in place.
  Two things make it correct rather than merely working. The whole drag is one
  `TransformEntitiesCommand`, so one Ctrl+Z undoes it: a command per entity would undo them one at a
  time, through arrangements the scene was never in. And descendants of selected entities are
  excluded (`findSelectionRoots`), because a child is already carried by its parent and transforming
  both moves it twice.
  The single-entity drags still compute the *gesture* -- how far, what angle, what factor -- and the
  new `MultiTransformDrag` turns one gesture into the edits a whole selection needs, so twenty
  entities cannot disagree about how far the cursor went.
- **An interaction boundary on `CommandHistory`**, which closes the oldest known problem in this
  file: two separate drags of one inspector slider collapsed into a single undo entry. The merge key
  answers "is this the same *edit*" -- entity, component, property -- and cannot answer "is this the
  same *gesture*", because two drags of one field are identical by everything it can see and differ
  only in that the user let go in between.
  `endInteraction()` marks that boundary, and the application calls it once per frame on every frame
  where no widget is active. The signal is new: `EditorUi::isAnyItemActive()`, answered by ImGui's
  own `IsAnyItemActive()` -- which already covers viewport drags, since the viewport image is an
  ImGui item and stays active for the whole of one. The gizmos have always opened a new entry on the
  first edit of a drag; this is the same rule for every other continuous control, including ones
  nobody has written yet.
- **Gizmo snapping and Ctrl+click selection**, both of which were waiting on the same missing piece:
  `UiImageInteraction` carried no modifier state. It now carries `control` and `shift`, filled by the
  ImGui backend from `ImGuiIO` and settable by tests.
  Holding Ctrl rounds a drag -- translation to the **visible** grid (the same `chooseGridSpacing` the
  renderer draws with, moved into the scene module so there is one answer to "how far apart are the
  lines"), rotation to 15 degrees, scale to tenths. A modifier rather than a mode, because "line this
  up with that, now nudge it" happens seconds apart. The *result* is snapped rather than the movement
  -- except for rotation, where the *turn* is, since snapping the absolute angle would straighten
  whatever it touched -- and only on the axes the handle allows. That last one was a real bug the
  test caught: an X-constrained drag was snapping Y as well, moving the entity along the one axis the
  user had just excluded, where they were not looking.
  Ctrl+click on the scene adds and removes from the selection; Ctrl on empty space does nothing,
  since clearing a half-assembled selection is the one thing that cannot have been meant.
- **ED-513** per-backend player builds from one configure. `CNA_EDITOR_PLAYER_BACKENDS="SOFTWARE"`
  now builds the second player itself instead of leaving it to a hand-run CMake in a scratch
  directory. A *nested* build per backend, because `CNA_GRAPHICS_BACKEND` is a cache variable of
  CNA's own build: one value per build tree, and no arrangement of targets in this one can change
  that. Each child is told to build nothing but the player -- no ImGui, no tests, and an empty
  backend list so it cannot spawn children of its own. Empty by default and minutes per entry when
  it is not, since each one compiles CNA again; but without it ED-510 has nothing to compare and
  play mode's backend picker has exactly one entry.
- **ED-511** the conformance harness. `cna-editor --compare-backends` runs *exactly* what the
  Backends panel runs -- `ComparisonPanel::startComparison`, called directly, not a second path that
  could pass while the panel was broken -- prints one line per backend, and **exits non-zero when
  they disagree**. The exit code is the assertion; a build server reads none of the output.
  `--tolerance=N` is on the command line because the threshold is a policy a project sets, not a
  constant this editor should choose for it. A headless run is refused up front: comparing means
  decoding captures, decoding needs a device, and "every capture was unreadable" is a confusing way
  to say "wrong configuration". Verified against the two real player builds: exit 5 with the diff
  reported, and exit 0 at `--tolerance=64`, which is exactly the largest difference the two backends
  actually produce.
- **ED-510** backend comparison, the last item on the owner's priority list. `BackendComparison`
  launches one player per installed build, waits for each handshake, asks all of them for the same
  frame over the bridge and compares what comes back against the first to answer. It needed no new
  architecture, exactly as `plan.md` predicted: play mode already spawns and supervises a player, so
  this is that, several times over.
  The pixel arithmetic is `ImageDiff` in `cna-editor-core` -- CNA-free and tested against images the
  test builds itself. Decoding a capture is *injected* (`ImageReader`/`ImageWriter`, supplied by the
  viewport), because turning a PNG back into pixels needs a graphics API and one module may have one.
  **The tolerance is the load-bearing detail.** Two backends drawing the same scene are not required
  to be bit-identical and never will be -- different rasterisation rules, different filtering
  precision -- so a comparison with no tolerance reports every backend as different from every other,
  which is true and useless. What is reported is how many pixels differ, by how much, and *where*:
  the bounding box is usually the whole diagnosis, and a difference image is written beside the
  captures with the matching picture dimmed and the differing pixels in magenta.
  **Verified against two real backends**, not just at the seams: a `cna-player-software` built into
  the scratch directory and installed beside `cna-player-easygl`. They disagree on 496 of 921600
  pixels (0.05%), largest channel difference 64, inside a 103x64 box -- and the difference image
  shows why at a glance: it is the anti-aliased outline of the two sprites and nothing else. That is
  a real, explainable difference between CNA's backends, found by pressing a button.
- **ED-246** `cna-player` draws. It has spoken the whole protocol since ED-240 and shown nothing
  the whole time, which quietly made three finished rows half true -- play mode ran a game with a
  blank window, and a live edit or a hot reload was observable in a log rather than in a picture.
  The window host mirrors the editor's exactly: a `Game` subclass hidden behind a free function, so
  CNA stays a private link dependency of one module. It draws through the **same**
  `CnaSceneRenderer`, via a new `renderGameView` that runs the content pass and skips the grid and
  overlay ones -- the separation was already structural, so this is not running two passes rather
  than filtering anything. A second renderer would have been a second answer to "what does this
  look like when it runs", which is the property the editor exists to guarantee.
  The view is the scene's own: `computeGameView` finds the primary camera, reads `orthographicSize`
  as the visible **height** (so a wider window shows more world instead of stretching it), takes the
  camera's world transform so a camera on a rig works, and clears to that camera's `clearColor`.
  Screenshots changed shape: `PlayerHost` **queues** them and the frame loop that owns the device
  takes them, so `screenshotReady` now means pixels are on disk. A failed or impossible capture
  answers `written: false` with a reason, because an editor waiting for a reply that never arrives
  is worse off than one told no. No window to be had degrades to running with nothing drawn and says
  so -- which is exactly what the bridge tests, which have no display, now exercise.
  This is also ED-510's missing half: comparing backends means capturing frames from N players, and
  until now there were no frames.
- **ED-401** rotate and scale gizmos, and the local/world space toggle (`X`). Built out of phase
  order because it is not a 3D task in a 2D viewport: `E` and `R` had been selecting a manipulator
  that did not exist since Phase 1. `TranslateGizmo.hpp` became `TransformGizmos.hpp` and now holds
  all three -- one layout, one hit-test, one drag each, all CNA-free, so what a user can grab is
  tested in CI and only the pixels need a GPU.
  Rotate turns in **world** space and stores the result in the parent's frame, or a child of a
  rotated parent would turn by a rotated fraction of the angle the cursor described; the delta is
  wrapped into (-pi, pi] so dragging across the seam does not spin it. Scale is a **ratio in screen
  space**, since scale is unitless and only a ratio is zoom-independent; dragging through the pivot
  flips the entity (negative scale is legitimate and `SpriteBatch` honours it) but never lands on
  zero, which would make it invisible *and* unclickable.
  **Scale deliberately has no space toggle**: a non-uniform scale in world space needs a shear,
  which a position/rotation/scale transform cannot express, so its arms are always the entity's own
  axes. The translate gizmo's arms are now stored directions rather than assumed ones, which is what
  lets the drawing, the hit-test and the drag read the same two vectors in either space; the
  renderer grew a rotated-quad `drawLine`, since SpriteBatch has no line primitive and an arm that
  is not axis-aligned cannot be a rectangle. Verified by screenshot on EASYGL: ring, rotated arms
  and arrowheads, and the scale gizmo's square handles.
- **ED-308** the build panel. Shells out to `cmake` rather than writing a script, because that
  gives the editor the exit code and the output; drives the *project's own* `CMakeLists`,
  contributing only the backend and the output directory; and reports a missing toolchain before
  offering the button, since CMake's own message for that says nothing a user can act on. The
  commands are shown before they run. `planBuild` is pure, so the part that carries the knowledge
  is tested without a compiler; the process half is exercised against a real `cmake` over a
  `project(... NONE)` probe, which needs no compiler either.
  `findCMake()` is resolved once by the panel and cached -- it walks every directory on the PATH,
  and the panel draws every frame.
- **ED-304** audio. The preview plays through CNA's *public* `SoundEffect(path)` constructor, so
  unlike the sprite-font preview there was nothing forbidden in the way -- worth checking before
  assuming a second G-04. `EditorAudio` sits beside `EditorViewport` in the one CNA-linking module.
  The component preview uses the component's own volume, pitch and pan; the asset preview uses
  neutral ones, because that is the file as imported rather than as some entity plays it.
  `CNA.AudioListener` takes its position from the Transform rather than repeating it, and a second
  enabled listener is an error for the same reason a second primary camera is.
  The preview now holds a **`SoundEffectInstance`** rather than calling the fire-and-forget
  `SoundEffect::Play()`, so Stop stops the clip and `isPlaying()` asks the device instead of
  reporting a remembered flag. **Not exercised here:** this container has no audio device, so the
  change is verified by compiling and by reading CNA's API, not by hearing it.
- **ED-309** backend diagnostics. Capabilities are asked of the *device* rather than derived from
  the backend's name -- several vary by driver within one backend. EASYGL correctly reports no
  wireframe fill mode (OpenGL ES has no `glPolygonMode`), which a name-keyed table would have got
  wrong. They cross the module boundary as strings, so nothing outside the CNA-linking module knows
  `GraphicsCapability` exists.
- **Tilemap eyedropper and rectangle fill**, and the `ViewportPanel::handleInteraction` split
  that had been marked due. The split came first, deliberately: the *ordering* between interaction
  modes is load-bearing -- a drag in progress outranks everything, a tool outranks the gizmo, the
  gizmo outranks the picker -- and adding two more modes to one long function would have buried
  three real rules. Each is now one line with its reason beside it. The eyedropper hands the brush
  back to the paint tool, because picking a tile is never the goal; an empty cell is refused rather
  than taken as -1, which would silently turn it into an eraser. A fill applies on release, so the
  rectangle can be adjusted before it lands, and is one undo entry however large.
- **ED-303** sprite animation, complete. A frame is an index into a sheet, not a rectangle -- the
  same arithmetic the tilemap already does, and far smaller to author. The frame list is an
  ordinary `List<Integer>`, so reordering and adding frames needed no new widget. Playback is a
  plain value the inspector owns and throws away, never the document's (D-07), and the clock is
  passed in so the test steps it exactly. `EditorUi::imageRegion` is new: it draws one sub-rectangle
  of a sheet, in texels, because everything on this side of the boundary already speaks texels.
  The viewport draws the previewed frame too: the inspector publishes the *result* through
  `EditorActions` while keeping the playback itself, which is the same shape the selection already
  uses and for the same reason. An animated sprite is sized by its frame rather than by its sheet,
  or a sixteen-frame walk cycle would be sixteen times too wide to click and Frame Selected would
  zoom out to fit a strip nobody is looking at.
  Per-frame durations close the row: an **optional** parallel list, ignored unless it is exactly
  as long as the frame list, so a scene written before it existed plays identically. Playback walks
  one frame at a time now rather than dividing by a single rate, bounded so a list of near-zeroes
  cannot spin it.
- **ED-302 (part)** sprite fonts. The `.spritefont` description is read and reported in the
  inspector, all of it **read-only** -- the file is the content pipeline's own input, so an
  editable copy in the sidecar would be a second answer to a question the build asks the file.
  Read by a targeted tag scan, not an XML parser, and guarded by a structural check so an ordinary
  XML file with a `<Size>` element is not read as a font. **The glyph preview is not done** and
  ED-302 is 🔄: it needs a built `SpriteFont`, and CNA exposes no public way to make one from a
  `.spritefont` -- recorded as gap G-04.
- **ED-301** tilemaps. Flat `List<Integer>` grid on an ordinary component, so no new serialised
  structure and nothing else had to learn a new type. A stroke is one undo entry and the merge key
  carries the *stroke id* -- entity + property alone cannot tell two drags apart. Painting is an
  `EditorTool`, not a `GizmoMode`, and while a brush is active it suppresses both the gizmo and
  click-to-select: a tilemap's gizmo sits over its own first tiles, so the first stroke would
  otherwise drag the map instead of painting it. That was a real bug the test caught.
  Rendering shares the sprite pass, with viewport culling -- a 200x200 map is forty thousand draw
  calls a frame otherwise. Verified by screenshot on EASYGL.
- **ED-300** prefabs, complete. Create Prefab from a hierarchy row, drop one from the browser to
  instantiate, and an inspector block that reports what an instance has changed with Revert and
  Apply. Every step undoes, file writes included.
  Two things worth knowing about Apply, because both were bugs before they were tests: it maps the
  instance's entity ids *back* through the links before writing (an instantiated instance has fresh
  ids, and writing those verbatim would leave every link naming an entity the file no longer has),
  and it strips the instance bookkeeping from what it writes (or every future instance would be
  born claiming to be an instance of something else). `findPrefabOverrides` compares through the
  *descriptor* on both sides, because "unset" and "set to the default" are deliberately
  indistinguishable in the document model and a round trip through a file turns one into the other.
- **ED-300 (part)** prefabs: the document model and the scene operations. `.cnaprefab` reuses the
  scene's entity encoding through the newly extracted `EntityJson.hpp` -- one codec, because an
  instantiated prefab and a hand-authored entity must be indistinguishable once they are in a scene.
  **Overrides are computed, not stored**, so prefabs added no field to the scene format at all.
  `InstantiatePrefabCommand`, `RevertPrefabInstanceCommand`, `findPrefabOverrides` and the link keys
  are in and tested. **The UI is not**: nothing yet creates a prefab from a selection or drops one
  into a scene, so ED-300 is 🔄. See *Where to start next*.
- **ED-305** layers and tags. Layers live in the `.cnaproject` (a property of the game, not of one
  level) and drive `CNA.Layer`'s choices by re-registering the descriptor. Renaming a layer leaves
  entities holding the old name deliberately -- which of the remaining layers they meant is the
  user's decision -- and the new `unknown-enum-value` validation rule reports it. Tags are their own
  component holding a `List<String>`. `SetProjectLayersCommand` lives in `cna-editor-context`
  because it has to touch both the project and the registry, and neither of those modules may
  depend on the other. The idle Inspector now edits project settings instead of saying "Nothing
  selected". No `formatVersion` bump: `layers` is an additive field, and there is a test that a
  project file written before it existed still opens.
- **ED-311 (part)** `PropertyType::List`. Element type declared on the descriptor, never
  inferred. `"list"` appended to the type-name table, not inserted, because those names are on the
  wire. The inspector draws a collapsible block; every change returns the whole new list, so add,
  remove, move and edit are all plain `SetPropertyCommand`s, and structural ones take their own
  undo entry. **`NestedStructure` is not done** and the plan row is 🔄, not ✅.
  Fixed on the way: an array of non-numbers on an *unregistered* component used to be inferred as a
  vector, so `["ground", "solid"]` became `(0, 0)` and was written back that way -- silently
  emptying a field in the one case the descriptor system promises to survive. Inference now only
  calls a short array a vector when every element is a number.
- **ED-306 / ED-307** live editing into the running player. One hook does the property half:
  every document change goes through a command (D-06), so `EditorContext`'s new command observer
  sees all of them and the application mirrors the `SetPropertyCommand`s. Undo and redo mirror too,
  and the value is read from the *document* rather than from the command, because after an undo the
  live value is the old one. Assets are sent by id, and the player rescans before looking the id up.
  Verified end to end against a real `cna-player` process, not just at the seams.
  `PlayerHost::takeReloadedAssets()` is the seam the graphics half drains, and since ED-246 it does:
  a texture changed on disk is visible in the running game rather than only in its log.
- **ED-902** format migration. A chain of single-version steps, run on every load of a
  `.cnascene`, `.cnaproject` and `.cnaasset`. The gate and the upgrade are one piece of code,
  because refusing a file from the future and upgrading one from the past both answer "what version
  is this?". **No `formatVersion` was bumped** and every chain is empty -- that is the intended
  state, and registering a step is not a licence to bump one. `loadFromJson` takes an optional
  migrator so a test can prove the loader reads the *upgraded* document, not the original.
- **ED-903** crash recovery. No signal handler, on purpose -- one serialising a document from
  inside `SIGSEGV` calls `malloc` and the filesystem with a corrupted heap. A `.cnarecovery`
  snapshot instead, written by ordinary code every `--autosave=SECONDS` while the document differs
  from its file, atomically by rename, in the user's state directory. Offered on reopen, never
  applied: while the offer stands, autosave for that scene is suspended, because the snapshot file
  is keyed by scene id and the current session's unsaved seconds are worth less than the previous
  session's unsaved hours. Format documented in `docs/FORMATS.md`.
- **ED-905** undo history panel. Rows are *positions*, not entries -- one more row than there are
  commands, and the extra one is the document as opened, which is what someone asking to put it
  back is aiming at. Clicking navigates through `EditorApplication::undo/redo`, so a jump prunes
  the selection the way Ctrl+Z does.
- **ED-310** scene validation. `SceneValidation.hpp` holds the structural rules -- duplicate
  primary cameras, inverted camera planes, zero scale, empty entities, a missing Transform, a
  second copy of a unique component, an unregistered component type, a sprite with no texture.
  Missing references stayed where they were, in `MissingReferences.hpp`, because they need the
  asset database and the rules do not; both now report into one **Validation** panel, since a user
  whose scene misbehaves does not know in advance which of the two is at fault. Every rule
  describes a *legal* state, so nothing refuses to save and nothing is repaired automatically, and
  each rule's test asserts both halves: the offending scene is reported and the nearest legitimate
  scene is not.

- **ED-221 / ED-220 thumbnails.** Reuse the scene renderer's texture cache, so a preview costs
  nothing once the sprite using it has been drawn. Needed a *keyed* borrow in the UI renderer —
  the existing one owns a single slot, right for the render target and wrong for anything there
  can be many of. Invalidating an asset now releases the UI's borrowed entry first, because the
  watcher makes a dangling pointer there a routine event rather than a theoretical one.
- **ED-250** header-only scene loader, and `docs/DESIGN-SCENE-LOADER.md`. Q-02 resolved.
  `examples/SceneLoaderDemo` is both the documentation and the integration test.
- **ED-220** asset browser: derived folder tree, filter, rename, move. A move keeps the asset's id
  and is asserted not to change a scene file by a single byte.
- **ED-900 (partial)** Linux CI, and this file.

- **ED-223** asset watcher. Polling rather than a native watcher, deliberately — three platform
  implementations with three failure modes, each still needing a polling fallback for the network
  and container mounts where a team's assets often live. The clock is passed in, so tests advance
  time exactly and never sleep. A change drops the viewport's cached texture via
  `EditorViewport::invalidateAsset`, which also clears the failed-load memory so a file that comes
  back gets another attempt.
- **ED-222 / ED-221 (part)** importer settings. An importer's settings are declared as a
  `ComponentDescriptor`, so the inspector needed no new code. Texture dimensions are read from the
  file header (PNG and BMP state theirs at fixed offsets); an unmeasurable format reports unknown
  rather than zero.
- **ED-224** missing-reference report and relink. Answers a different question from
  `AssetDatabase::getMissingAssets()`: which *entities* point at something that will not load.
  `RelinkAssetCommand` rewrites every reference as one undo entry.
- **ED-210** panel split. `EditorApplication.cpp` went from 1207 lines to 523; six panel classes
  under `src/panels/`, reaching the editor through the narrow `EditorActions` interface.
- **ED-200 / ED-208 / ED-114** hierarchy editing, asset drag-and-drop onto slots, console filter.
- **ED-118** rotations as Euler degrees, in XNA's own `CreateFromYawPitchRoll` convention.
- **ED-204** icons for entities the viewport cannot draw.
- **ED-209 / ED-122** keyboard shortcuts and Frame Selected.
- **ED-205** translate gizmo.

---

## In progress / immediately next

Phase 1 closed. Working through the owner's priority order:

1. ~~**Robustness and data safety**~~ ✅ — ED-310, ED-905, ED-903 and ED-902 are all done. No
   `formatVersion` was bumped; `.cnarecovery` is a *new* format at version 1, not a change to an
   existing one.
2. ~~**Live editing into the running player**~~ ✅ — ED-306 and ED-307 are done.
3. ~~**Production 2D tools**~~ ✅ — ED-300, ED-301, ED-303, ED-304, ED-305, ED-308 and ED-309 are
   done; ED-302 and ED-311 🔄 and both blocked, see below.
   Note that `NestedStructure` (the open half of ED-311) turned out **not** to be needed by
   prefabs: ED-300 computes overrides rather than storing them, so nothing needs a nested schema
   yet. Leave it unbuilt until something real asks for one.
4. ~~**ED-510** backend comparison mode~~ ✅ — with ED-511 and ED-513 beside it.

Everything on that list is closed, and so is the 3D work the owner opened afterwards — including
ED-405, the model pipeline the owner named after seeing the wireframe screenshots. **ED-402 is the
next task**, and for the first time it is not blocked on anything: the meshes exist. See *Where to
start next*.

---

## Known problems and limitations

- **The backend comparison needs two player builds, and a default build produces one.** ED-513 makes
  the second one a configure option (`CNA_EDITOR_PLAYER_BACKENDS`) rather than a hand-run CMake, but
  it is off by default because each entry compiles CNA again -- minutes, not seconds. Anyone who
  presses Compare on a default build will be told it needs another build, which is correct and still
  worth knowing before it happens.
- **A red `needs-display` ctest usually means Xvfb died, not that the editor did.** Twice in one
  session `CnaEditorWindowSmoke` and `CnaSceneLoaderDemo` failed together with "Subprocess aborted"
  in hundredths of a second; both times the X server was simply gone (`pgrep Xvfb`), and restarting
  it made all twelve pass. Check that before reading the diff.
- **Image dimensions are read for PNG, BMP and JPEG only.** Anything else reports unknown, and the
  inspector shows 0×0 with a tooltip saying why. A format that needs a decoder to measure is one
  the importer should measure when it loads the file for real.
- **`--headless` writes to the project.** Opening a project applies importer facts and may rewrite
  a sidecar. This is intended (it corrected a stale stamp in the example) but worth knowing before
  running the editor against a repository you want left alone.

### Gaps found in CNA — documented only, not filed upstream

| Id | Gap | Effect here |
|----|-----|-------------|
| G-01 | `Microsoft::Xna::Framework::Color` has no default constructor | `std::vector<Color>::resize(n)` does not compile. XNA's `Color` *is* default-constructible, so this is a real behavioural difference. Worked around with `assign`. |
| G-02 | `CNA::Devices::Clipboard` sits inside `#ifdef CNA_DEVICES`, default OFF | An editor built against a default CNA has no clipboard. Degrades cleanly and is reported. |
| G-03 | `RenderTarget2D` sampled as a texture is not origin-normalised across backends | EASYGL renders it flipped, SOFTWARE does not. Worked around by `EditorViewport::isRenderTextureFlippedVertically()`, which is a compile-time constant per backend. |
| G-04 | No public way to build a `SpriteFont` | `SpriteFont`'s constructor takes an already-built glyph atlas, and the only reader that produces one is `CNA::Internal::Xnb::SpriteFontReader` -- which D-01 forbids the editor from touching. So the editor can describe a `.spritefont` but not preview its glyphs (ED-302). A public `ContentManager::Load<SpriteFont>` specialisation, or a public font builder, would close it. |

---

## Where to start next

Read this file, then `plan.md`'s *Current state* section.

**ED-402 — model rendering. It is next, it is unblocked, and that is new.** Every previous version
of this section had to say that the rest of Phase 3 waited on a model pipeline. It does not any
more: ED-405 built one, and what ED-402 needs from this repository now exists and is tested.

**What is already paid for**, so that the task is not re-derived:

- **The geometry, cached.** `EditorContext::makeMeshProvider()` hands out a `MeshProvider`;
  `MeshCache` imports each model once and holds it. A `MeshData` needs no CNA, which is why the
  cache is in the context rather than the viewport.
- **The Y mirror, applied at import.** A `MeshData` is already in the editor's Y-down world, so the
  model pass feeds the same mirrored view-projection everything else goes through and does *not*
  mirror again. `TheThreeDimensionalViewAgreesWithTheTwoDimensionalOneAboutWhichWayIsDown` still
  guards the projection; `TheGltfImporterMirrorsGltfsYUpIntoTheEditorsYDownWorld` guards the data.
- **Triangle winding that survives that mirror.** This is the one to keep in mind while writing the
  draw call, because it is invisible until it is not: the importer already reverses the winding, so
  the meshes are wound counter-clockwise-from-outside and `CullMode::CullCounterClockwiseFace` is
  the setting that matches them. If models come out inside-out, suspect the cull mode rather than
  the importer — `meshWindingMatchesNormals` is asserted over every fixture.
- **A vertex laid out as `VertexPositionNormalTexture`.** Field for field, deliberately, so the
  upload is a copy. CNA's `Model`, `ModelMesh`, `ModelMeshPart` and `VertexBuffer` all have public
  constructors (`Model`'s is `NOXNA` but public), so a model can be built by hand — no content
  pipeline and no `CNA::Internal` needed. That was checked before ED-405 chose this shape.
- **Materials, reduced to what a `BasicEffect` can express.** `MeshMaterial` carries diffuse,
  emissive, specular, specular power, alpha and a base-colour texture *path*. Resolving that path
  to a `Uuid` is the caller's job: the importer has no asset database and must not pretend to.
- **Somewhere to look.** The example project has `Assets/Models/Crate.gltf` and a `Crate` entity in
  `Level01`, and `--orbit` takes the picture.

**The decision ED-402 still has to make**, and it is worth making deliberately: *what a `.cnascene`
says about a model*. Today `CNA.ModelRenderer` holds one asset reference and one optional material
override. A per-mesh material list is ED-410, which needs ED-311's `NestedStructure` — and a
material list is exactly the first real consumer that schema has ever had, which is what makes it
designable now when it was not before. Whether that lands with ED-402 or after it is the owner's
call, since it touches the document model.

**Blocked, not forgotten:**

- **ED-302's glyph preview** needs a public way to build a `SpriteFont` from a `.spritefont`. CNA
  has none; recorded as gap G-04.
- **ED-311's `NestedStructure`** still has no consumer *in this repository* — but see above: ED-410's
  material list is the one that would justify it, and it is now one task away rather than several.
- **Animation.** `CNA.ModelImporter` declares `importAnimations` and `loadModel` ignores it, which
  is stated in the header rather than left to be discovered. An animation needs a skeleton to drive
  and `MeshData` has no node hierarchy on purpose. When it becomes a task it arrives as fields
  beside `MeshData::parts`; `loadModel`'s signature does not change.

**Smaller, unblocked, in the order I would take them:**

1. **Hear the audio preview on a machine with a sound device.** Still compiled and reasoned about
   rather than heard; this container has none.
2. **A second look at `findSelectionRoots`.** Unchanged from last session: move it to the scene
   module when a *fourth*, non-gizmo caller appears. Until then the include is honest.
3. **The 3D view has no tile tools and no animation preview**, deliberately: both are 2D ideas.
   `beginGizmo3DDrag` and `handleInteraction3D` are where that would be decided.

The one behaviour to preserve throughout: every format is at version 1, and ED-902's migration
chains are empty on purpose. ED-405 added only *additive* sidecar fields — the model facts — which
is the permission the owner already gave. Adding a property type must not change what an existing
scene file serialises to; keep it that way, and do not bump a `formatVersion` without the owner's
say-so.
