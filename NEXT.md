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
| Unit tests | ✅ 194 / 194 |
| CTest (standalone) | ✅ 7 / 7 |
| CTest (CNA config) | ✅ 9 / 9 |
| Phase 1 | 🔄 ED-220 and ED-221 thumbnails outstanding; ED-250 decided, not yet written |

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
ctest --test-dir build-cna -R "CnaEditor|CnaPlayer"

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

1. **CI** (ED-900, partial) — GitHub Actions workflow, Linux, standalone build plus the full test
   suite at `-Werror`.
2. **ED-220** — asset browser: folder tree, filtering, rename, move. The move must not touch any
   scene; that is the property to test hardest.
3. **ED-221** — thumbnails. Needs the CNA-backed renderer, so the CNA-free layer can only decide
   *which* assets want one and at what size.
4. **ED-250** — the header-only scene loader and its design document. Closes Phase 1.

---

## Known problems and limitations

- **ED-221 is partial.** Settings and dimensions are done; thumbnails are not. The row is marked 🔄
  in `plan.md` rather than ✅ on purpose.
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

Read this file, then `plan.md`'s *Current state* section. The next task is whichever of the four
items under **In progress** is still open — they are listed in dependency order, and ED-250 is last
because it closes the phase.
