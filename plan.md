# CNA Tamagotchi — International P1 Development Plan

## Fixed target

The game is an unofficial, as-faithful-as-practical reimplementation of the
**international 1997 Tamagotchi Generation 1 (P1)** programme. It is not a
P1/P2 hybrid and it does not target a Japanese original or a modern reissue.
See [analysis.md](analysis.md) for the exact scope, legal boundary, and rule
capture method.

## Milestones

- [x] **1. Project foundation**
  - Create CMake C++ project with `include/`, `src/`, tests, `.gitignore`, and
    a CNA application entry point.
  - Implement an independent 32 × 16 one-bit LCD model and a scalable desktop
    egg-device renderer.
  - Add low-write JSON persistence, backup/recovery, platform save locations,
    smoke tests, and a protected virtual reset control.

- [x] **2. P1 target decision**
  - Select international P1 (1997), rather than P2 or a hybrid.
  - Record the target, the excluded variants, and the required P1 roster,
    controls, food, game, visual treatment, and end screen in English.
  - Mark current Pipple/Budbit content as legacy prototype content rather than
    representing it as a faithful result.

- [ ] **3. Source-backed P1 rule ledger**
  - Add `docs/p1-specification.md` with source/observation, target variant,
    inputs, expected outcome, and a test reference for every implemented rule.
  - Capture the international P1 stage timing, character sprites/animations,
    sleep schedules, minimum weights, life spans, attention timing, care
    mistakes, discipline, illness, medicine doses, and evolution matrix.
  - Explicitly distinguish verified facts from still-unverified behaviour.

- [ ] **4. Replace the legacy domain with P1 state**
  - Remove custom species, two-line generation seed, Elder, generic Farewell,
    and `0…100` needs from the active simulation.
  - Model P1’s form, stage, four hunger hearts, four happiness hearts, four
    discipline bars, age, weight, clock, care-event history, waste, illness,
    sleep, light, attention, and game state.
  - Implement chronological offline event processing without a broad per-minute
    loop or unnecessary save writes.
  - Add domain tests for each P1 trace before declaring it faithful.

- [ ] **5. P1 character catalogue and LCD assets**
  - Replace Pipple/Budbit assets with Babytchi, Marutchi, Tamatchi,
    Kuchitamatchi, Mametchi, Ginjirotchi, Maskutchi, Kuchipatchi, Nyorotchi,
    Tarakotchi, and Bill.
  - Add target sprite states and animations: idle, eating, sleeping, unhappy,
    ill, medicine, waste, attention, game, evolution, and end sequence.
  - Render P1’s checkerboard LCD field and exact permanent P1 icon placement;
    preserve the 32 × 16 logical resolution and one-bit pixels.

- [ ] **6. P1 controls and care menus**
  - Implement exact A/B/C navigation, B clock view, A+C clock setting, and
    A+C fresh egg from the end screen.
  - Replace legacy menus with P1 bread/candy Food, Light, Toilet, Health Meter,
    Medicine, Discipline, and unselectable Attention behaviour.
  - Preserve the desktop reset hold/confirmation only as a safety guard; its
    visible outcome must enter the P1 reset and clock-setup flow.

- [ ] **7. P1 Character game and evolution**
  - Remove Number and all P2 game behaviour.
  - Implement the five-round Character game, including left/right choices,
    result rules, happiness effect, and weight effect.
  - Implement the exact P1 care-mistake/discipline evolution tree and hidden
    Bill path with exhaustive resolver tests.

- [ ] **8. Completion of real-time P1 behaviour**
  - Implement verified P1 timers, calls, sleep/light mistakes, waste, illness,
    medicine, refusal, weight, death/life span, and angel-and-stars end screen.
  - Process long offline periods faithfully and deterministically; test clock
    rollback, restart, and boundary moments.
  - Replace the generic current Farewell/new-egg flow.

- [ ] **9. Save migration and user documentation**
  - Version P1 saves with an explicit target identifier.
  - Archive incompatible legacy hybrid saves rather than converting them into
    fictitious P1 pets.
  - Rewrite the English static tutorial into detailed novice-friendly P1 parts,
    replacing all Pipple, Budbit, PEEK, and NUM material with P1 screenshots or
    accurately labelled illustrations.

- [ ] **10. Verification and release readiness**
  - Run domain, persistence, display, and smoke tests with at most three build
    jobs; avoid unnecessary clean builds and large generated files.
  - Compare all visible flows against the captured international P1 reference.
  - Document remaining desktop-only safeguards and known deviations clearly.

## Immediate next task

Create the source-backed international-P1 rule ledger, then introduce a
P1-specific domain seam and a legacy-save compatibility boundary before
replacing active gameplay assets.
