# CNA Editor — Implementation Plan

> The reasoning behind this plan is in [`ANALYSIS.md`](ANALYSIS.md). Read that first: it records
> what was verified against the CNA codebase, where the original architecture discussion was wrong,
> and the thirteen decisions (D-01 … D-13) every task below rests on.
>
> **Task ids** use an `ED-NNN` scheme, banded by phase: `ED-0xx` Phase 0, `ED-1xx`/`ED-2xx` Phase 1,
> `ED-3xx` Phase 2, `ED-4xx` Phase 3, `ED-5xx` Phase 4, `ED-9xx` cross-cutting. Ids are stable and
> never reused.

## Status legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Done and verified |
| 🔄 | In progress |
| ⬜ | Not started |
| ⛔ | Deferred by explicit decision (reasoning recorded on the task) |
| 🔬 | Blocked on a spike or a decision |

---

## Current state

**Phase −1 (foundation) is complete.** This repository contains a working, dependency-free C++23
skeleton that builds and passes its tests with no CNA checkout, no GPU and no window:

- 8 modules, an application, and 69 passing tests across 5 CTest suites
- clean at `-Wall -Wextra -Wpedantic -Werror`
- `./build/cna-editor --headless --project=examples/HelloSprites/HelloSprites.cnaproject` opens a
  project, scans its assets, loads a three-entity scene and draws a headless frame

What it does *not* have: a window, a UI toolkit, rendering, or a player process. Those are Phase 0
and Phase 1.

---

## Module map

| Module | Depends on | CNA? | Purpose |
|--------|-----------|:----:|---------|
| `cna-editor-core` | — | no | Uuid, JSON, `PropertyValue`, `ComponentDescriptor`, `EditorCommand`, `CommandHistory` |
| `cna-editor-scene` | core, assets, project, ui | no | `SceneDocument`, `EditorEntity`, scene commands, built-in components |
| `cna-editor-assets` | core | no | `AssetDatabase`, `.cnaasset` sidecars, importers |
| `cna-editor-project` | core | no | `.cnaproject`, the backend capability table |
| `cna-editor-ui` | core | no | `EditorUi` abstraction, `NullEditorUi`; later the ImGui binding |
| `cna-editor-runtime-bridge` | core | no | `EditorProtocol`, `MessageStreamDecoder`, player process control |
| `cna-editor-plugins` | core | no | Manifest discovery, validation, dynamic loading |
| `cna-editor-context` | scene, assets, project, ui | no | `EditorContext` — the composition layer |
| `cna-editor-viewport` | scene | **yes** | The only module that links CNA (D-01, D-03) |
| `cna-editor` | context, plugins, bridge, viewport | via viewport | The application and its panels |

The layering is enforced by the build graph, not by review: a stray `#include <Microsoft/Xna/...>`
outside `cna-editor-viewport` fails to compile.

---

## Phase −1 — Foundation ✅

Complete. Everything below is implemented, tested and building.

| Id | Task | Status |
|----|------|:------:|
| ED-001 | Repository, CMake, C++23, MS-PL SPDX headers, Doxygen conventions | ✅ |
| ED-002 | `Uuid` — v4 generation, canonical parse and format, nil sentinel | ✅ |
| ED-003 | Dependency-free JSON reader and writer, order-preserving, diff-stable (D-12) | ✅ |
| ED-004 | `EditorMath` — the editor's own POD vector/quaternion/colour/rectangle types (D-13) | ✅ |
| ED-005 | `PropertyValue` — 13-alternative tagged union with JSON round-trip (D-05) | ✅ |
| ED-006 | `ComponentDescriptor`, `PropertyDescriptor`, `ComponentRegistry` (D-05) | ✅ |
| ED-007 | `EditorCommand` and `CommandHistory` — undo, redo, merging, dirty tracking, bounded retention (D-06) | ✅ |
| ED-008 | `EditorEntity`, `EditorComponent`, editor-only state (D-04, D-07) | ✅ |
| ED-009 | `SceneDocument` — hierarchy, cycle rejection, recursive delete, JSON round-trip | ✅ |
| ED-010 | Seven scene commands, all undoable, `SetProperty` merging on a stable key | ✅ |
| ED-011 | Six built-in component descriptors | ✅ |
| ED-012 | `AssetDatabase` — stable ids, `.cnaasset` sidecars, move and missing detection (D-08) | ✅ |
| ED-013 | `Project` — `.cnaproject`, `ProjectKind`, the 14-backend capability table (D-10, F-02) | ✅ |
| ED-014 | `EditorUi` abstraction and `NullEditorUi` (D-02) | ✅ |
| ED-015 | `EditorViewport` abstraction and `NullEditorViewport` | ✅ |
| ED-016 | `EditorProtocol` and `MessageStreamDecoder` (D-09) | ✅ |
| ED-017 | `PluginManifest` and `PluginHost` discovery and validation (D-11) | ✅ |
| ED-018 | `EditorContext` — the composition layer | ✅ |
| ED-019 | `EditorApplication`, argument parsing, six panels, headless frame loop | ✅ |
| ED-020 | Test harness and 69 tests; CTest registration | ✅ |
| ED-021 | `examples/HelloSprites` — a real project the editor opens end to end | ✅ |

**Two real bugs were found and fixed during this phase**, both recorded in ANALYSIS.md:

- The asset modification stamp used the filesystem clock's native tick count (~4.6 × 10¹⁸ ns),
  which is outside `double`'s exact-integer range. Since JSON numbers are doubles, it round-tripped
  through the sidecar *changed*, and every asset would have looked modified on every scan. Now
  stored in seconds; guarded by `AssetSidecarStampSurvivesAJsonRoundTrip`.
- `EditorContext` sat in `cna-editor-core` while depending on scene, assets and project — a
  dependency cycle. It now lives in its own `cna-editor-context` module above them.

---

## Phase 0 — Technical prototype ⬜

**Goal.** Prove CNA can host an editor UI. One window, docked panels, one CNA-rendered viewport.

**Explicitly out of scope.** Asset pipeline work, plugins, gizmos, the player process.

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-100 | **Spike: can Dear ImGui render through CNA's *public* API?** | 🔬 | Blocks ED-110. See Q-01. An ImGui renderer written against `SpriteBatch`/`Texture2D` would be backend-independent by construction — far better than seven per-backend renderers. Needs scissor rects, dynamic vertex buffers, texture binding by handle. Whatever is missing is a CNA gap worth filing, which is the point of D-01 |
| ED-101 | Vendor Dear ImGui (docking branch) into `third_party/`, matching CNA's vendoring convention | ⬜ | New dependency owned by this repository (F-06) |
| ED-102 | `CNA_EDITOR_WITH_CNA=ON` build verified against a real `../cna` + `../sharp-runtime` checkout | ⬜ | Also proves the `FATAL_ERROR` guidance in `CMakeLists.txt` is accurate |
| ED-110 | `ImGuiEditorUi` — implements every `EditorUi` method | ⬜ | Blocked by ED-100. No panel changes: that is the D-02 payoff |
| ED-111 | Window and event loop; `--ui=imgui` becomes real | ⬜ | |
| ED-112 | Dock space with the default five-panel layout, persisted between sessions | ⬜ | |
| ED-113 | `propertyField` widgets for all 13 property types | ⬜ | Sliders where `minimum < maximum`; combo boxes for enums; pickers for references |
| ED-114 | Console panel: severity filter, scroll-lock, copy | ⬜ | |
| ED-120 | `CnaEditorViewport::renderScene` draws sprites through `SpriteBatch` | ⬜ | Skeleton and build wiring already in place |
| ED-121 | Viewport renders into an offscreen target composited into the dock | ⬜ | |
| ED-122 | Editor camera: pan, zoom, frame-selection | ⬜ | |
| ED-130 | Report every CNA public-API gap ED-100 and ED-120 uncover | ⬜ | The deliverable D-01 exists to produce |

**Exit criterion.** `cna-editor --project=examples/HelloSprites/HelloSprites.cnaproject` opens a
real window, shows five docked panels, and renders the three-entity scene in the viewport.

---

## Phase 1 — Usable 2D MVP ⬜

**Goal.** The minimum milestone from the original discussion, in full.

### Editing

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-200 | Hierarchy panel: rename in place, drag-to-reparent, context menu, multi-select | ⬜ | Every operation through a command (D-06) |
| ED-201 | Sprite rendering resolves textures through `AssetDatabase`, ordered by layer depth | ⬜ | |
| ED-202 | Grid with adaptive spacing | ⬜ | |
| ED-203 | Selection outline as an overlay pass | ⬜ | Never scene geometry |
| ED-204 | Billboarded icons for cameras, lights and audio sources | ⬜ | They have no geometry and would otherwise be unclickable |
| ED-205 | Translate gizmo, with merged undo across the drag | ⬜ | The merge machinery is already built and tested |
| ED-206 | Ray-cast picking against entity bounds | ⬜ | GPU picking deferred to ED-320 |
| ED-207 | Inspector: add and remove components, respecting `unique` and `required` | ⬜ | |
| ED-208 | Asset drag-and-drop from the browser onto a sprite slot | ⬜ | |
| ED-209 | Keyboard shortcuts: Ctrl+Z/Y/S/N/D, Delete, F to frame, W/E/R for gizmo modes | ⬜ | |
| ED-210 | Split the panels out of `EditorApplication` into their own classes | ⬜ | Deliberately deferred until there is enough of a panel to be worth separating |

### Assets

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-220 | Asset browser: folder tree, thumbnails, filtering, rename, move | ⬜ | A move must not touch any scene (D-08) |
| ED-221 | Texture importer: dimensions, mipmaps, premultiplied alpha, thumbnails | ⬜ | |
| ED-222 | Importer settings surfaced in the inspector, reusing the descriptor system | ⬜ | |
| ED-223 | Filesystem watcher; reimport on external change | ⬜ | |
| ED-224 | "Missing references" report, with a relink dialog | ⬜ | |

### Play mode

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-240 | `cna-player` host: loads a project and a scene, speaks the protocol | 🔬 | Q-04: which repository owns this binary |
| ED-241 | Process spawn and lifetime; a player crash is reported, never fatal to the editor | ⬜ | |
| ED-242 | Socket transport over `MessageStreamDecoder` | ⬜ | Codec already built and tested |
| ED-243 | Play / Pause / Step / Stop, with the player's log routed into the console | ⬜ | |
| ED-244 | Player discovery: find installed `cna-player-<backend>` binaries and offer only those | ⬜ | Direct consequence of F-01 |
| ED-250 | **Design: how a game consumes a compiled scene** | 🔬 | Q-02. The decision most likely to pull the editor toward being an engine. Needs a written design before this phase closes |

**Exit criterion.** The original minimum milestone, verbatim: *open a project, show docked panels,
load a JSON scene with three sprites, select an object in the viewport or hierarchy, change its
position in the inspector, undo, save the scene, and run it in a separate CNA Player process.*

---

## Phase 2 — Production 2D editor ⬜

| Id | Task | Status |
|----|------|:------:|
| ED-300 | Prefabs: create, instantiate, override, apply | ⬜ |
| ED-301 | Tilemap component and tile-painting tool | ⬜ |
| ED-302 | `SpriteFont` preview and importer settings | ⬜ |
| ED-303 | Sprite animation editor with a timeline | ⬜ |
| ED-304 | Audio source and listener editing, with preview playback | ⬜ |
| ED-305 | Layers and tags | ⬜ |
| ED-306 | Asset hot-reload into a running player over the bridge | ⬜ |
| ED-307 | Live property editing into a running player | ⬜ |
| ED-308 | Build and publish dialog driving CNA's own CMake targets | ⬜ |
| ED-309 | Backend diagnostics: report the current build's `GraphicsCapability` set | ⬜ |
| ED-310 | Scene validation: missing references, duplicate primary cameras, zero scale, empty entities | ⬜ |
| ED-311 | `PropertyType::List` and `NestedStructure`, with inspector support | ⬜ |
| ED-320 | GPU picking through an id render target | ⛔ |

**ED-320 deferred.** Ray-cast picking (ED-206) is correct and needs no render target or GPU
read-back. This is an optimisation to reach for when profiling says so, not before — and it would
have to be written per backend, which is exactly the kind of cost F-01 makes worth avoiding.

---

## Phase 3 — Basic 3D ⬜

**Precondition.** CNA's 3D API is stable and the glTF conformance work is done. This phase comes
*after* that, as the original discussion argued — not before.

| Id | Task | Status |
|----|------|:------:|
| ED-400 | Perspective and orthographic viewport camera, orbit and fly navigation | ⬜ |
| ED-401 | Rotate and scale gizmos; local/world space toggle | ⬜ |
| ED-402 | `ModelRenderer` rendering | ⬜ |
| ED-403 | Material editing and preview | ⬜ |
| ED-404 | Light components with viewport visualisation | ⬜ |
| ED-405 | glTF importer built on CNA's own `cgltf` integration | ⬜ |
| ED-406 | Mesh preview in the asset browser | ⬜ |
| ED-407 | Environment and fog settings | ⬜ |
| ED-410 | Per-mesh material lists (needs ED-311) | ⬜ |
| ED-411 | **Plugin dynamic loading**: `dlopen`/`LoadLibrary`, `extern "C"` entry, unload, hot-reload | ⬜ |
| ED-412 | Plugin extension points: importers, component types, panels, menu commands, gizmos, exporters | ⬜ |

**ED-411** is where discovery-and-validation (ED-017, done) becomes real loading. Validation was
built first deliberately: an ABI mismatch that reaches `dlopen` is a crash, not an error message
(D-11).

---

## Phase 4 — Advanced tooling ⬜

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-500 | Animation timeline and curve editor | ⬜ | |
| ED-501 | Skeletal animation preview | ⬜ | |
| ED-502 | Particle system editor | ⬜ | |
| ED-503 | Material node graph | ⬜ | |
| ED-504 | Shader editor with live compilation | ⬜ | |
| ED-505 | Frame debugger over the bridge | ⬜ | |
| ED-506 | Profiler panel fed by `ReportFrameStats` | ⬜ | |
| ED-510 | **Backend comparison mode** | ⬜ | Spawn N players from N per-backend builds, capture screenshots, diff |
| ED-511 | Backend conformance harness built on ED-510 | ⬜ | Could feed CNA's own CI |
| ED-512 | Remote device preview (Android, browser) | ⬜ | |
| ED-520 | MC3 / Mesh-Craft plugin: import/export, primitive editor, CSG preview, glTF export | ⬜ | Lives outside the editor core, by design (D-11) |
| ED-521 | Terrain and world tools | ⬜ | |

**ED-510 is cheaper than it looks.** F-01 turned out to be an enabler here: once preview means
"spawn a player process", spawning four of them from four per-backend builds is the same mechanism
run four times. What it needs is a build matrix and image diffing, not new architecture.

---

## Cross-cutting

| Id | Task | Status | Notes |
|----|------|:------:|-------|
| ED-900 | CI: build and test on Linux, Windows and macOS, warnings as errors | ⬜ | `-Werror` already clean |
| ED-901 | Doxygen configuration matching CNA's | ⬜ | |
| ED-902 | Format migration framework for `.cnaproject`, `.cnascene`, `.cnaasset` | ⬜ | Version gates and rejection already implemented; migration is not |
| ED-903 | Crash handling: never lose an unsaved document | ⬜ | |
| ED-904 | Editor preferences, persisted separately from any project | ⬜ | |
| ED-905 | Undo history panel | ⬜ | `CommandHistory` already exposes everything needed |
| ED-906 | Localisation of the UI strings | ⛔ | Deferred: English-only until the panel set stops changing shape |
| ED-907 | Adopt GoogleTest if the suite needs fixtures or parameterised cases | ⛔ | Deferred: nothing outside `TestHarness.hpp` knows how assertions are spelled, so this stays a contained change (D-12) |
| ED-908 | Optional content-hash change detection for assets | ⛔ | Deferred: hashing every asset on project open is how an editor comes to take thirty seconds to start (D-08). Add as opt-in when a pipeline needs it |

---

## Deliberately not built

Taken from the original discussion, which was right about all of it. Each of these would have sunk
a first version:

| Not doing | Instead |
|-----------|---------|
| A custom GUI toolkit | Dear ImGui behind `EditorUi` (D-02); a custom toolkit can be its own project later |
| A full ECS in CNA runtime | An editor *document* model that compiles down to whatever the game wants (D-04) |
| Visual scripting | — |
| A material node graph | Phase 4 at the earliest (ED-503) |
| An embedded C++ IDE | — |
| Editor and game in one process | Separate `cna-player` — mandatory given F-01, not merely wise (D-09) |
| Editor UI on all 14 backends | Three support tiers, classified per backend (F-02) |
| Multi-user collaboration | — |
| Unity-style prefab variants | Plain prefabs first (ED-300) |
| A physics engine | — |
| A complete 3D editor | Phase 3 is *basic* 3D, and only after glTF conformance |

---

## Open questions

These block specific tasks and are stated in full in ANALYSIS.md §4.

| Id | Question | Blocks |
|----|----------|--------|
| Q-01 | Can Dear ImGui render through CNA's public API alone? | ED-100 → ED-110 |
| Q-02 | How does a game consume a compiled scene? | ED-250 |
| Q-03 | Which process owns the window in play mode? | ED-241 (Phase 1 answer: a separate top-level window) |
| Q-04 | Does `cna-player` live here or in `openeggbert/cna`? | ED-240 |
