# 28. UI, HUD, menus, accessibility, and input rebinding

[Back to master plan](../plan.md)

Replace debug title text with a complete readable and accessible presentation layer.
Input rebinding covers the primary gameplay actions on keyboard and gamepad for v1; a
full every-action rebinding matrix with cross-device conflict detection is explicit
later polish, not a v1 dependency. Dialogue subtitle timing/content lives in group 25;
this group owns how the HUD presents it.

## Core UI data and flow

- [x] **IG-28-001 P0** — Replace window-title mission text with a SpriteBatch/SpriteFont HUD. *(Gate M10: the window title stays too (window-manager/taskbar visibility), but a real on-screen `SpriteBatch`/`SpriteFont` HUD is now the primary display, drawn each frame in `IronGangGame::Draw()`. The `SpriteFont` is hand-built (`include/`/`src/UI/BitmapFont.hpp`/`.cpp`, `BuildBitmapFont8x8()`) from the public-domain "font8x8" bitmap font (8x8 monochrome glyphs, printable ASCII only) since CNA has no XNB font content pipeline and this project avoids vendoring a new TTF-rasterization dependency or sourcing an external font license for a first pass. Verified: `TestBitmapFontGlyphAtlas` (hand-derived bit-pattern check against the 'A' glyph, independent of any GraphicsDevice), the full `ctest` suite, `./scripts/check-syntax.sh`, and a `--smoke` run with no crash while the HUD drew every frame -- confirmed real (CPU-rasterizer) text drawing has a measurable per-frame cost in this environment's software backend (30 frames went from ~10s to ~15s wall-clock), not yet profiled against `docs/performance-targets.md` (see `plan_35`).)*
- [x] **IG-28-002 P0** — Display current objective, interaction prompt, dialogue subtitle, and transient save status. *(Same HUD: shows the current objective + driving speed, "Enter: continue"/"Enter: skip" prompts during dialogue/cutscenes, the dialogue speaker/line as a subtitle, wanted status ("Police dispatched..."/"WANTED"), and the existing transient save/load/interaction status messages -- all in `IronGangGame::Draw()`. No interaction-prompt-specific text yet beyond dialogue/cutscene prompts (e.g. no "Press E to enter" on-screen prompt near the vehicle) -- that's a small follow-up, not done in this pass.)*
- [ ] **IG-28-003 P0** — Create keyboard and gamepad navigation for menus.
- [ ] **IG-28-004 P0** — Create pause, settings, save/load, restart, and quit menus. *(Partial: **pause and quit** exist. Escape used to quit the game outright -- a debug affordance, not a pause -- and now toggles a paused state where the world does not advance; quitting moved behind that screen (Q). The HUD and the window title both say `PAUSED` and list the keys, so a paused game is obvious even from the taskbar. Pausing is a **safe** save moment (the world is frozen and consistent), so F5/F9 keep working there. Physics is skipped entirely while paused rather than stepped with a zero delta -- the world should not be advanced at all, not advanced by nothing. Still missing: an actual navigable menu (this is a key-driven screen, not IG-28-003's navigation), settings, and restart.)*
- [ ] **IG-28-005 P1** — Create UI layout scaling for aspect ratio, resolution, and DPI.
- [ ] **IG-28-006 P1** — Create safe-area support.
- [ ] **IG-28-007 P1** — Create basic rebinding (rebind the primary on-foot/vehicle/menu actions) for keyboard and gamepad; a full every-action matrix with cross-device conflict detection is deferred (see IG-28-024).
- [x] **IG-28-008 P1** — Create separate on-foot, vehicle, menu, and cinematic input contexts. *(`InputContext` (`include/IronGang/Gameplay/InputContext.hpp`): Paused, DistrictTransition, Cutscene, Dialogue, VehicleTransition, Driving, OnFoot, resolved from one `GameplaySignals` struct in a documented precedence order. This replaces the same question being answered separately at half a dozen sites (`!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning`), each free to drift from the others -- and it absorbed the `SaveConditions`/`FindSaveBlockReason` pair added under plan_29, which was a second independent answer to the same question: `SaveBlockReasonForContext` now derives it. `ContextAdvancesWorld`/`ContextAllowsMovement`/`ContextAllowsInteraction` express what each context permits; only Paused stops the world, because ambient traffic and the police do not wait for a conversation. Covered by `TestInputContextResolvesByPrecedence`.)*
- [ ] **IG-28-009 P1** — Create mouse sensitivity, gamepad sensitivity, inversion, and dead-zone settings.
- [ ] **IG-28-010 P1** — Create color-blind-safe objective and marker presentation.
- [ ] **IG-28-011 P1** — Create reduced motion/camera shake options.
- [ ] **IG-28-012 P1** — Create hold/toggle options where relevant.
- [ ] **IG-28-013 P1** — Create readable focus and hover states.
- [ ] **IG-28-014 P1** — Create UI sound events.
- [ ] **IG-28-015 P1** — Create loading/district-transition status presentation without exposing technical noise.
- [ ] **IG-28-016 P1** — Create wanted/vehicle/health UI once those systems exist (groups 17/22).
- [ ] **IG-28-017 P1** — Create UI state tests and screenshot references.
- [ ] **IG-28-018 P2** — Create a simple map/route UI for the current district once its road graph exists. *(Partial: `Tab` now toggles a real top-down current-district overlay built from authored `WorldBox` footprints, with player, vehicle, mission target, district exit, north indicator, legend, and a direct player-to-exit guide. The projection is deterministically unit-tested. The guide is intentionally straight-line rather than road-aware because the prototype still has only a fixed traffic `WaypointPath`, not the road graph this task requires for completion.)*
- [ ] **IG-28-019 P2** — Create mission log and dialogue history.
- [ ] **IG-28-020 P2** — Create accessibility presets (bundles of the options above).
- [ ] **IG-28-021 P2** — Create a basic UI style/theme pass (consistent fonts, colors, spacing) once the functional screens above exist.
- [ ] **IG-28-022 P3** — Create photo mode UI only after core presentation works and only if time remains.
- [ ] **IG-28-023 P3** — Run a screen-reader feasibility study for menus as a research spike, not a committed feature.
- [ ] **IG-28-024 P3** — Design the full every-action input-rebinding matrix with cross-device conflict detection only as later polish once basic rebinding (IG-28-007) ships.

## HUD root

- [ ] **IG-28-025 P0** — Define the scope and public API of the HUD root, covering objective/interaction-prompt/subtitle display (IG-28-002).
- [ ] **IG-28-026 P0** — Implement the smallest deterministic reference path: show one objective and one interaction prompt at once.
- [ ] **IG-28-027 P1** — Add input validation and actionable failure reporting for missing HUD data.
- [ ] **IG-28-028 P1** — Add unit tests and one integration scenario covering layout scaling (IG-28-005) and safe areas (IG-28-006).
- [ ] **IG-28-029 P1** — Define save/checkpoint serialization and restoration for transient HUD state (e.g. save-status indicator).
- [ ] **IG-28-030 P2** — Add debug logging/inspection and document usage.

## Menu system (pause, settings, save/load)

- [ ] **IG-28-031 P0** — Define the scope and public API of the menu system (pause/settings/save/load/restart/quit from IG-28-004).
- [ ] **IG-28-032 P0** — Implement the smallest deterministic reference path: open pause menu, adjust one setting, resume.
- [ ] **IG-28-033 P1** — Add input validation and actionable failure reporting for malformed settings data.
- [ ] **IG-28-034 P1** — Add unit tests and one integration scenario covering keyboard/gamepad menu navigation (IG-28-003).
- [ ] **IG-28-035 P1** — Define save/checkpoint serialization and restoration for menu-driven settings.
- [ ] **IG-28-036 P2** — Add debug logging/inspection and document usage.

## Input rebinding (basic scope)

- [ ] **IG-28-037 P1** — Define the scope and public API of basic input rebinding (primary actions only, per IG-28-007), including input contexts (IG-28-008).
- [ ] **IG-28-038 P1** — Implement the smallest deterministic reference path: rebind one action on keyboard and one on gamepad.
- [ ] **IG-28-039 P1** — Add input validation and actionable failure reporting for conflicting/invalid bindings.
- [ ] **IG-28-040 P1** — Add unit tests and one integration scenario covering context switching (on-foot/vehicle/menu/cinematic).
- [ ] **IG-28-041 P1** — Define save/checkpoint serialization and restoration for rebound controls.
- [ ] **IG-28-042 P2** — Add debug logging/inspection and document usage.

## Accessibility settings

- [ ] **IG-28-043 P1** — Define the scope and public API of accessibility settings (color-blind-safe presentation, reduced motion, subtitle options, hold/toggle, presets from IG-28-010/011/012/020).
- [ ] **IG-28-044 P1** — Implement the smallest deterministic reference path: toggle one accessibility option and see it take effect immediately.
- [ ] **IG-28-045 P1** — Add input validation and actionable failure reporting for malformed accessibility data.
- [ ] **IG-28-046 P1** — Add unit tests and one integration scenario covering an accessibility preset bundle.
- [ ] **IG-28-047 P1** — Define save/checkpoint serialization and restoration for accessibility settings.
- [ ] **IG-28-048 P2** — Add debug logging/inspection and document usage.

## Screen transition

- [ ] **IG-28-049 P1** — Define the scope and public API of screen transitions (menu-to-menu and district-loading transitions from IG-28-015).
- [ ] **IG-28-050 P1** — Implement the smallest deterministic reference path: fade between two screens without a visible pop.
- [ ] **IG-28-051 P1** — Add input validation and actionable failure reporting for interrupted transitions.
- [ ] **IG-28-052 P1** — Add unit tests and one integration scenario covering a district-load transition.
- [ ] **IG-28-053 P2** — Add debug logging/inspection and document usage.

## Settings persistence

- [ ] **IG-28-054 P1** — Define the scope and public API for persisting settings (video/audio/input/accessibility) separately from campaign save data.
- [ ] **IG-28-055 P1** — Implement the smallest deterministic reference path: change a setting, restart the game, confirm it persisted.
- [ ] **IG-28-056 P1** — Add input validation and actionable failure reporting for corrupted settings files.
- [ ] **IG-28-057 P1** — Add unit tests and one integration scenario covering settings persistence across all menus above.
- [ ] **IG-28-058 P2** — Add debug logging/inspection and document usage.
