# 28. UI, HUD, menus, accessibility, and input rebinding

[Back to master plan](../plan.md)

Replace debug title text with a complete readable and accessible presentation layer.
Input rebinding covers the primary gameplay actions on keyboard and gamepad for v1; a
full every-action rebinding matrix with cross-device conflict detection is explicit
later polish, not a v1 dependency. Dialogue subtitle timing/content lives in group 25;
this group owns how the HUD presents it.

## Core UI data and flow

- [x] **IS-28-001 P0** — Replace window-title mission text with a SpriteBatch/SpriteFont HUD. *(Gate M10: the window title stays too (window-manager/taskbar visibility), but a real on-screen `SpriteBatch`/`SpriteFont` HUD is now the primary display, drawn each frame in `IronShadowsGame::Draw()`. The `SpriteFont` is hand-built (`include/`/`src/UI/BitmapFont.hpp`/`.cpp`, `BuildBitmapFont8x8()`) from the public-domain "font8x8" bitmap font (8x8 monochrome glyphs, printable ASCII only) since CNA has no XNB font content pipeline and this project avoids vendoring a new TTF-rasterization dependency or sourcing an external font license for a first pass. Verified: `TestBitmapFontGlyphAtlas` (hand-derived bit-pattern check against the 'A' glyph, independent of any GraphicsDevice), the full `ctest` suite, `./scripts/check-syntax.sh`, and a `--smoke` run with no crash while the HUD drew every frame -- confirmed real (CPU-rasterizer) text drawing has a measurable per-frame cost in this environment's software backend (30 frames went from ~10s to ~15s wall-clock), not yet profiled against `docs/performance-targets.md` (see `plan_35`).)*
- [x] **IS-28-002 P0** — Display current objective, interaction prompt, dialogue subtitle, and transient save status. *(Same HUD: shows the current objective + driving speed, "Enter: continue"/"Enter: skip" prompts during dialogue/cutscenes, the dialogue speaker/line as a subtitle, wanted status ("Police dispatched..."/"WANTED"), and the existing transient save/load/interaction status messages -- all in `IronShadowsGame::Draw()`. No interaction-prompt-specific text yet beyond dialogue/cutscene prompts (e.g. no "Press E to enter" on-screen prompt near the vehicle) -- that's a small follow-up, not done in this pass.)*
- [ ] **IS-28-003 P0** — Create keyboard and gamepad navigation for menus.
- [ ] **IS-28-004 P0** — Create pause, settings, save/load, restart, and quit menus.
- [ ] **IS-28-005 P1** — Create UI layout scaling for aspect ratio, resolution, and DPI.
- [ ] **IS-28-006 P1** — Create safe-area support.
- [ ] **IS-28-007 P1** — Create basic rebinding (rebind the primary on-foot/vehicle/menu actions) for keyboard and gamepad; a full every-action matrix with cross-device conflict detection is deferred (see IS-28-024).
- [ ] **IS-28-008 P1** — Create separate on-foot, vehicle, menu, and cinematic input contexts.
- [ ] **IS-28-009 P1** — Create mouse sensitivity, gamepad sensitivity, inversion, and dead-zone settings.
- [ ] **IS-28-010 P1** — Create color-blind-safe objective and marker presentation.
- [ ] **IS-28-011 P1** — Create reduced motion/camera shake options.
- [ ] **IS-28-012 P1** — Create hold/toggle options where relevant.
- [ ] **IS-28-013 P1** — Create readable focus and hover states.
- [ ] **IS-28-014 P1** — Create UI sound events.
- [ ] **IS-28-015 P1** — Create loading/district-transition status presentation without exposing technical noise.
- [ ] **IS-28-016 P1** — Create wanted/vehicle/health UI once those systems exist (groups 17/22).
- [ ] **IS-28-017 P1** — Create UI state tests and screenshot references.
- [ ] **IS-28-018 P2** — Create a simple map/route UI for the current district once its road graph exists.
- [ ] **IS-28-019 P2** — Create mission log and dialogue history.
- [ ] **IS-28-020 P2** — Create accessibility presets (bundles of the options above).
- [ ] **IS-28-021 P2** — Create a basic UI style/theme pass (consistent fonts, colors, spacing) once the functional screens above exist.
- [ ] **IS-28-022 P3** — Create photo mode UI only after core presentation works and only if time remains.
- [ ] **IS-28-023 P3** — Run a screen-reader feasibility study for menus as a research spike, not a committed feature.
- [ ] **IS-28-024 P3** — Design the full every-action input-rebinding matrix with cross-device conflict detection only as later polish once basic rebinding (IS-28-007) ships.

## HUD root

- [ ] **IS-28-025 P0** — Define the scope and public API of the HUD root, covering objective/interaction-prompt/subtitle display (IS-28-002).
- [ ] **IS-28-026 P0** — Implement the smallest deterministic reference path: show one objective and one interaction prompt at once.
- [ ] **IS-28-027 P1** — Add input validation and actionable failure reporting for missing HUD data.
- [ ] **IS-28-028 P1** — Add unit tests and one integration scenario covering layout scaling (IS-28-005) and safe areas (IS-28-006).
- [ ] **IS-28-029 P1** — Define save/checkpoint serialization and restoration for transient HUD state (e.g. save-status indicator).
- [ ] **IS-28-030 P2** — Add debug logging/inspection and document usage.

## Menu system (pause, settings, save/load)

- [ ] **IS-28-031 P0** — Define the scope and public API of the menu system (pause/settings/save/load/restart/quit from IS-28-004).
- [ ] **IS-28-032 P0** — Implement the smallest deterministic reference path: open pause menu, adjust one setting, resume.
- [ ] **IS-28-033 P1** — Add input validation and actionable failure reporting for malformed settings data.
- [ ] **IS-28-034 P1** — Add unit tests and one integration scenario covering keyboard/gamepad menu navigation (IS-28-003).
- [ ] **IS-28-035 P1** — Define save/checkpoint serialization and restoration for menu-driven settings.
- [ ] **IS-28-036 P2** — Add debug logging/inspection and document usage.

## Input rebinding (basic scope)

- [ ] **IS-28-037 P1** — Define the scope and public API of basic input rebinding (primary actions only, per IS-28-007), including input contexts (IS-28-008).
- [ ] **IS-28-038 P1** — Implement the smallest deterministic reference path: rebind one action on keyboard and one on gamepad.
- [ ] **IS-28-039 P1** — Add input validation and actionable failure reporting for conflicting/invalid bindings.
- [ ] **IS-28-040 P1** — Add unit tests and one integration scenario covering context switching (on-foot/vehicle/menu/cinematic).
- [ ] **IS-28-041 P1** — Define save/checkpoint serialization and restoration for rebound controls.
- [ ] **IS-28-042 P2** — Add debug logging/inspection and document usage.

## Accessibility settings

- [ ] **IS-28-043 P1** — Define the scope and public API of accessibility settings (color-blind-safe presentation, reduced motion, subtitle options, hold/toggle, presets from IS-28-010/011/012/020).
- [ ] **IS-28-044 P1** — Implement the smallest deterministic reference path: toggle one accessibility option and see it take effect immediately.
- [ ] **IS-28-045 P1** — Add input validation and actionable failure reporting for malformed accessibility data.
- [ ] **IS-28-046 P1** — Add unit tests and one integration scenario covering an accessibility preset bundle.
- [ ] **IS-28-047 P1** — Define save/checkpoint serialization and restoration for accessibility settings.
- [ ] **IS-28-048 P2** — Add debug logging/inspection and document usage.

## Screen transition

- [ ] **IS-28-049 P1** — Define the scope and public API of screen transitions (menu-to-menu and district-loading transitions from IS-28-015).
- [ ] **IS-28-050 P1** — Implement the smallest deterministic reference path: fade between two screens without a visible pop.
- [ ] **IS-28-051 P1** — Add input validation and actionable failure reporting for interrupted transitions.
- [ ] **IS-28-052 P1** — Add unit tests and one integration scenario covering a district-load transition.
- [ ] **IS-28-053 P2** — Add debug logging/inspection and document usage.

## Settings persistence

- [ ] **IS-28-054 P1** — Define the scope and public API for persisting settings (video/audio/input/accessibility) separately from campaign save data.
- [ ] **IS-28-055 P1** — Implement the smallest deterministic reference path: change a setting, restart the game, confirm it persisted.
- [ ] **IS-28-056 P1** — Add input validation and actionable failure reporting for corrupted settings files.
- [ ] **IS-28-057 P1** — Add unit tests and one integration scenario covering settings persistence across all menus above.
- [ ] **IS-28-058 P2** — Add debug logging/inspection and document usage.
