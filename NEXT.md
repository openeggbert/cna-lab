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
| Unit tests | ✅ 346 / 346 (also under Clang Release) |
| CTest (standalone) | ✅ 8 / 8 |
| CTest (CNA config) | ✅ 12 / 12 |
| CI | ✅ Linux, GCC Debug + Clang Release, `-Werror` |
| **Phase 1** | ✅ **complete** — all 23 tasks |
| **Phase 2** | 🔄 10 of 12 done; only ED-302 and ED-311 remain, and both are half done and blocked on something real |
| **Phase 3** | 🔄 1 of 11 — ED-401 only, built early because it is not a 3D task in a 2D viewport |
| **Phase 5** | 🔄 ED-510, ED-511 and ED-513 done — the backend comparison mode, end to end |
| **Owner priorities** | ✅ **all four closed**: robustness and data safety; live editing into the player; production 2D tools; backend comparison |

---

## Start here

You are picking up a branch with nothing half-finished on it: the working tree is clean, every
commit is pushed, and all 346 tests pass in three configurations. There is no rescue work to do
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

## What landed in the most recent session

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
```

Do **not** run `cmake --build build-cna` without a target list: CNA's own examples fail to compile
in this environment (`examples/common/PixelTestGame.hpp` cannot find `SDL3/SDL.h`). That is a CNA
build issue, unrelated to the editor, and naming the editor targets sidesteps it entirely.

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
3. **Production 2D tools** ← *current* — ED-300, ED-301, ED-303, ED-305 ✅; ED-302 and
   ED-311 🔄. Next: ED-309 backend diagnostics, ED-304 audio, ED-308 the build dialog.
   Note that `NestedStructure` (the open half of ED-311) turned out **not** to be needed by
   prefabs: ED-300 computes overrides rather than storing them, so nothing needs a nested schema
   yet. Leave it unbuilt until something real asks for one.
4. **ED-510** backend comparison mode.

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

**Every priority the owner named is closed.** Robustness and data safety; live editing into the
running player; the production 2D tools; and the backend comparison mode, which now runs from a
panel (ED-510), from the command line with an exit code (ED-511), and against players the build
produces itself (ED-513). Phase 1 is complete, Phase 2 is ten of twelve, and the two open rows are
each half built and each blocked on something outside this repository.

What follows is a judgement call rather than a queue.

**Blocked, not forgotten:**

- **ED-302's glyph preview** needs a public way to build a `SpriteFont` from a `.spritefont`. CNA
  has none; recorded as gap G-04.
- **ED-311's `NestedStructure`** has no consumer. ED-300 computes prefab overrides by comparison
  rather than storing them, so nothing needs a nested schema. Designing one against no consumer is
  how you get it wrong.
- **The rest of Phase 3** -- ED-400's perspective camera, ED-402's model rendering, ED-404's lights
  -- waits on CNA's 3D API, which is the precondition `plan.md` states for the phase.

**The owner has chosen the next two** (see the 2026-08-04 decisions above): **ED-400**, the 3D
viewport camera, and **input forwarding into the running player**. Take ED-400 first — it is the
one with a CNA-free half that can be built and tested before any window is involved, and the
2D camera (`EditorCamera2D`) is the pattern to follow.

**Then, small and unblocked, in the order I would take them.** The first is written out in enough detail
to start on without re-deriving anything.

1. **A snap *step* the project can set.** Ctrl currently snaps to the visible grid, 15 degrees and
   tenths, all three fixed in `ViewportPanel::getSnap`. A project laying out on a 16-pixel tile grid
   wants to say so once. The shape, following `layers` exactly:
   - `Project` gains `gridSnap` (a float, 0 meaning "use the visible grid"), serialised as an
     *additive* field in `.cnaproject` — **no `formatVersion` bump**, and a test that a project file
     written before it existed still opens, like `layers` has.
   - A `SetProjectGridSnapCommand` beside `SetProjectLayersCommand` in `cna-editor-context`, so the
     edit undoes like everything else (D-06).
   - The idle Inspector already edits project settings; the field goes there. `getSnap` then reads
     the project and falls back to `chooseGridSpacing` when it is zero.
   - Tests: the fallback still snaps to the visible grid; a set step wins over it; the round trip
     through a file keeps it.
2. **Hear the audio preview on a machine with a sound device.** It was rewritten to hold a
   `SoundEffectInstance` so Stop actually stops, but this container has none, so that path has been
   compiled and reasoned about rather than heard.
3. **A second look at `findSelectionRoots`.** It is a *selection* utility living in
   `TransformGizmos.hpp` because the gizmo needed it first; `deleteSelection` now uses it too. If a
   third caller turns up, move it somewhere it belongs rather than adding a fourth include of the
   gizmos.

The one behaviour to preserve throughout: every format is at version 1, and ED-902's migration
chains are empty on purpose. Adding a property type must not change what an existing scene file
serialises to; keep it that way, and do not bump a `formatVersion` without the owner's say-so.
