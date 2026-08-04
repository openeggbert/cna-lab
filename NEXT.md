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
| Unit tests | ✅ 290 / 290 (also under Clang Release) |
| CTest (standalone) | ✅ 7 / 7 |
| CTest (CNA config) | ✅ 10 / 10 |
| CI | ✅ Linux, GCC Debug + Clang Release, `-Werror` |
| **Phase 1** | ✅ **complete** — all 23 tasks |
| **Phase 2** | 🔄 9 of 12 done (ED-300, 301, 303, 304, 305, 306, 307, 309, 310); ED-302 and ED-311 half; ED-308 open |
| Owner priorities 1 and 2 | ✅ closed (robustness and data safety; live editing into the player) |

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

# Headless smoke, and a screenshot of the real window.
./build/cna-editor --headless --project=examples/HelloSprites/HelloSprites.cnaproject
DISPLAY=:99 ./build-cna/cna-editor --project=examples/HelloSprites/HelloSprites.cnaproject \
    --frames=40 --screenshot=/tmp/editor.png
```

Do **not** run `cmake --build build-cna` without a target list: CNA's own examples fail to compile
in this environment (`examples/common/PixelTestGame.hpp` cannot find `SDL3/SDL.h`). That is a CNA
build issue, unrelated to the editor, and naming the editor targets sidesteps it entirely.

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

- **ED-304** audio. The preview plays through CNA's *public* `SoundEffect(path)` constructor, so
  unlike the sprite-font preview there was nothing forbidden in the way -- worth checking before
  assuming a second G-04. `EditorAudio` sits beside `EditorViewport` in the one CNA-linking module.
  The component preview uses the component's own volume, pitch and pan; the asset preview uses
  neutral ones, because that is the file as imported rather than as some entity plays it.
  `CNA.AudioListener` takes its position from the Transform rather than repeating it, and a second
  enabled listener is an error for the same reason a second primary camera is.
  **Known limitation:** `stop()` only changes the editor's belief. CNA's fire-and-forget `Play()`
  hands back no handle; tracking a `SoundEffectInstance` is the shape to reach for when someone
  asks to actually cut a clip short.
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
  **Caveat worth knowing:** `cna-player` still draws nothing, so a hot-reload today is observable in
  its log and asset database rather than on screen. `PlayerHost::takeReloadedAssets()` is the seam
  its graphics half will drain.
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

- **Rotate and scale gizmos do not exist.** Pressing `E` or `R` selects the mode and says so in the
  console instead of leaving the gizmo silently absent. ED-401.
- **The inspector's merge boundary is per-property, not per-interaction.** Two separate drags of
  the same slider collapse into one undo entry. The gizmo works around this by opening a new entry
  on the first edit of a drag (`ViewportPanel::updateGizmoDrag`); the inspector does not. ED-311
  narrowed it -- a *structural* edit (add, remove, move a list element, drop an asset) now opens a
  new entry via `InspectorPanel::PropertyEdit::structural` -- but a continuous drag still merges
  with the previous one. A general fix would give `CommandHistory` an explicit interaction
  boundary.
- **JPEG dimensions are not read.** `readImageSize` handles PNG and BMP; anything else reports
  unknown, and the inspector shows 0×0 with a tooltip saying why.
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

Read this file, then `plan.md`'s *Current state* section. Priorities 1 and 2 are closed and
`PropertyType::List` is in, which was the thing blocking the rest of priority 3.

Phase 2's remaining work is now mostly *finishing* rather than starting. In rough order of value:

1. **ED-308** the build and publish dialog, the last open Phase 2 item and the one most likely to
   need decisions rather than typing. Three to settle before writing code:
   - **Shell out to `cmake`, or write a script the user runs?** Shelling out gives the editor the
     output to show and the exit code to report; writing a script is honest about the fact that a
     real build has options the editor does not model. `PlayerProcess` already spawns and pumps a
     child process, so the machinery for the first exists.
   - **Where a build goes.** A directory beside the project is the obvious answer and the one that
     needs a `.gitignore` entry the editor cannot add for the user.
   - **What happens when the toolchain is missing.** This is the common case for anyone who
     installed the editor and not a compiler, and "nothing happened" is the worst answer.
2. **ED-311's `NestedStructure`** if anything ever needs it, and **ED-302's glyph preview** if CNA
   closes G-04. Both are blocked on something real rather than on effort here.
3. **Audio `stop()` cannot actually stop a clip** (see ED-304 above). Small, and worth doing the
   moment anyone previews something long enough to want it cut short.

Nothing above changes a file format. Every format is at version 1 and ED-902's chains are empty on
purpose; a new component type is something the descriptor system handles without a bump.

The one behaviour to preserve throughout: every format is at version 1, and ED-902's migration
chains are empty on purpose. Adding a property type must not change what an existing scene file
serialises to; keep it that way, and do not bump a `formatVersion` without the owner's say-so.
