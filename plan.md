# CNA Tamagotchi Development Plan

## Scope and principles

This plan describes an original C++ virtual-pet game powered by the sibling
CNA framework. It uses the 32 × 24 logical monochrome display chosen in
[analysis.md](analysis.md), while keeping simulation code independent from the
renderer. No commercial Tamagotchi characters, artwork, or device branding
will be copied.

## Milestones

- [x] **1. Product analysis and technical direction**
  - Record the visual, control, simulation, and save-data proposal.
  - Verify the reference LCD resolution and interaction pattern.
  - Define an original-art/content boundary.

- [x] **2. Buildable project skeleton**
  - Add root CMake configuration that consumes the sibling `../cna` checkout.
  - Add `include/` and `src/` layers plus a minimal CNA application entry point.
  - Add a basic CMake preset, `.gitignore`, and developer README.
  - Start a window with a warm, slowly transitioning background.
  - Exclude a licence file because it will be supplied separately.

- [ ] **3. Deterministic game domain**
  - [x] Define `PetState`, needs, life stages, and core care actions.
  - [x] Implement deterministic offline progression with a safe 12-hour clamp.
  - Add persisted UTC timestamps and clock-rollback behaviour with the save layer.
  - [x] Add unit tests for care effects, offline clamping, and life-stage progression.
  - Add scheduled events, care-mistake presentation, and data-driven evolution.

- [ ] **4. 1-bit display and device renderer**
  - Complete the independent 32 × 24 framebuffer with sprites, text, and
    clipped drawing helpers.
  - [x] Draw an original egg-shaped shell, LCD, eight persistent shell icons,
    and three physical buttons as the first visible prototype.
  - Add a palette selector and renderer tests.
  - Scale LCD pixels with nearest-neighbour rendering and no anti-aliasing.

- [ ] **5. First playable care loop**
  - Implement A/B/C input mapping, menu navigation, status screen, feeding,
    cleaning, sleep/light, medicine, attention, and discipline.
  - Add one small original mini-game and feedback animations.
  - Add basic original pet species and a data-driven evolution table.

- [ ] **6. Persistence and resilience**
  - Implement versioned JSON save/load in a user-data directory.
  - Use atomic replacement plus a backup save.
  - Add migration, validation, corruption-recovery, and offline-time tests.

- [ ] **7. Polish and release readiness**
  - Add sound options, keyboard/controller accessibility, pause/help, and
    export/import.
  - Add original pixel art, testing/smoke-test coverage, CI, and platform
    packaging documentation.
  - Profile real-time updates and document known limitations.

## Immediate next task

Begin milestone 3 by defining the domain data model and its tests. The visual
prototype is now in place, so the pet's long-lived behaviour can become
correct and testable before interactive menus are added.
