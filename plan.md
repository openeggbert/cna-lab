# CNA Tamagotchi Development Plan

## Scope and principles

This plan describes an original C++ virtual-pet game powered by the sibling
CNA framework. It uses the 32 × 16 logical monochrome display chosen in
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
  - [x] Persist care-scheduler state with backward-compatible version-1 save loading.
  - Add persisted UTC timestamps and clock-rollback behaviour with the save layer.
  - [x] Add unit tests for care effects, offline clamping, and life-stage progression.
  - [x] Add scheduled attention, care-mistake rules, waste, sleep/light requests,
    and data-driven evolution.
  - [x] Add basic illness and Farewell transitions for excessive snacks and
    neglected waste/health.

- [ ] **4. 1-bit display and device renderer**
  - Complete the independent 32 × 16 framebuffer with sprites, text, and
    clipped drawing helpers.
  - [x] Draw an original egg-shaped shell, LCD, eight persistent shell icons,
    and three physical buttons as the first visible prototype.
  - Add a palette selector and renderer tests.
  - Scale LCD pixels with nearest-neighbour rendering and no anti-aliasing.

- [ ] **5. First playable care loop**
  - [x] Implement A/B/C keyboard mapping, icon selection, and feeding, play,
    sleep, cleaning, medicine, and discipline actions.
  - Add menus, a named status screen, attention requests, and an in-device
    presentation for discipline.
    - [x] Add the first Food (meal/snack) and two-page Status LCD screens;
      display the attention call on the home screen.
  - [x] Add a first original two-choice Peek mini-game with seed-backed outcomes.
  - Add feedback animations and a second game profile.
  - [x] Add the first original egg line: 11 creature forms plus an egg, with
    care-driven teen/adult resolution and tested 1-bit sprites.

- [ ] **6. Persistence and resilience**
  - [x] Implement version-1 JSON save/load with field validation.
  - [x] Use a temporary file and backup before replacing an existing save.
  - [x] Integrate an automatic relative save slot and bounded UTC offline-time
    catch-up; prevent clock rollback from moving the saved timestamp backward.
  - Add platform user-data path selection, atomic-replace support on every
    target, migrations, and corruption-recovery UI.

- [ ] **7. Polish and release readiness**
  - Add sound options, keyboard/controller accessibility, pause/help, and
    export/import.
  - Add original pixel art, testing/smoke-test coverage, CI, and platform
    packaging documentation.
  - Profile real-time updates and document known limitations.

## Immediate next task

Complete the first playable care loop with an in-device status screen and
food submenu. This will let the existing A/B/C controls expose meal versus
snack behaviour without adding modern UI controls outside the device.
