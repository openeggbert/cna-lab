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
| Unit tests | ✅ 220 / 220 (also under Clang Release) |
| CTest (standalone) | ✅ 7 / 7 |
| CTest (CNA config) | ✅ 10 / 10 |
| CI | ✅ Linux, GCC Debug + Clang Release, `-Werror` |
| **Phase 1** | ✅ **complete** — all 23 tasks |

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

1. **Robustness and data safety** ← *current*
   - ~~**ED-310** scene validation~~ ✅
   - **ED-905** undo history panel. `CommandHistory` already exposes everything it needs.
   - **ED-903** never lose an unsaved document on a crash.
   - **ED-902** format migration. Reads old versions; does **not** bump `formatVersion`. Last of
     the four because it is the only one that touches a file format, where the standing constraint
     applies.
2. **Live editing into the running player** — ED-306 asset hot-reload, ED-307 live properties. The
   bridge and protocol exist and are tested; this is mostly wiring.
3. **Production 2D tools** — ED-311 `PropertyType::List` first, because it unblocks others; then
   ED-300 prefabs, ED-305 layers and tags, ED-301 tilemap.
4. **ED-510** backend comparison mode.

---

## Known problems and limitations

- **Rotate and scale gizmos do not exist.** Pressing `E` or `R` selects the mode and says so in the
  console instead of leaving the gizmo silently absent. ED-401.
- **The inspector's merge boundary is per-property, not per-interaction.** Two separate drags of
  the same slider collapse into one undo entry. The gizmo works around this by opening a new entry
  on the first edit of a drag (`ViewportPanel::updateGizmoDrag`); the inspector does not. A general
  fix would give `CommandHistory` an explicit interaction boundary.
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

---

## Where to start next

Read this file, then `plan.md`'s *Current state* section. The next task is the first still-open
item under **In progress** — **ED-905**, the undo history panel. `CommandHistory` already exposes
the count, the cursor and each entry's description, so the panel is a view over state that exists
rather than new bookkeeping; the interesting decision is what clicking an entry does, and the
honest answer is *undo or redo to that point*, not *remove that one entry from history*.
