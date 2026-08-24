# CNA Tamagotchi — International P1 Development Plan

## Fixed target

The game is an unofficial, as-faithful-as-practical reimplementation of the
**international 1997 Tamagotchi Generation 1 (P1)** programme. It is not a
P1/P2 hybrid and it does not target a Japanese original or a modern reissue.
See [analysis.md](analysis.md) for the exact scope, legal boundary, and rule
capture method. The engine is data-driven: P1 is its first and only active
programme package, so a future P2 package can reuse the engine instead of
copying it.

This is a clean behavioural implementation: no original ROM, TamaLIB, or
similar emulation core is permitted in the application, build, or test suite.

## Milestones

- [x] **1. Project foundation**
  - Create CMake C++ project with `include/`, `src/`, tests, `.gitignore`, and
    a CNA application entry point.
  - Implement an independent 32 × 16 one-bit LCD model and a scalable desktop
    egg-device renderer.
  - Add low-write JSON persistence, backup/recovery, platform save locations,
    smoke tests, and a protected virtual reset control.
  - Update the integration for the current modular `../cna` and
    `../sharp-runtime`: link `CNA::Runtime` plus only the selected renderer,
    provide the required sharp-runtime component closure, and disable the
    unused networking, CNAEXT, device-extension, and Draco paths.

- [x] **2. P1 target decision**
  - Select international P1 (1997), rather than P2 or a hybrid.
  - Record the target, the excluded variants, and the required P1 roster,
    controls, food, game, visual treatment, and end screen in English.
  - Mark current Pipple/Budbit content as legacy prototype content rather than
    representing it as a faithful result.

- [ ] **3. Source-backed P1 rule ledger**
  - [x] Add `docs/p1-specification.md` with source/observation, target variant,
    inputs, expected outcome, and a test reference for every implemented rule.
  - [x] Record the boundary for visual reference work: a captured P1 Mametchi
    idle sequence may be manually transcribed into independent one-bit frame
    data, but no ROM, extracted asset, or emulator dependency may enter this
    repository.
  - Capture the international P1 stage timing, character sprites/animations,
    sleep schedules, minimum weights, life spans, attention timing, care
    mistakes, discipline, illness, medicine doses, and evolution matrix.
  - Explicitly distinguish verified facts from still-unverified behaviour.

- [ ] **4. Replace the legacy domain with P1 state**
  - [x] Add a tested P1 roster, unambiguous P1 stages, and bounded visible
    four-heart/four-bar state as the migration seam.
  - [x] Add generic programme-definition types and an `international-p1-1997`
    data package for the P1 roster, food, display, Character game, and ending.
  - [x] Drive the active Food and Character-game UI from that selected P1
    definition, removing the active P2 Number-game path.
  - [x] Introduce a shared programme simulator and drive the P1 egg → Babytchi
    → Marutchi trace from definition data (including the captured baby illness,
    medicine, and nap events).
  - [x] Move the active application to the shared programme state and simulator;
    retire the Pipple/Budbit state from the active renderer and save path.
  - Make schedules, evolution rules, menu/icon data, and sprite keys part of
    programme data before P1 behaviour is migrated into the shared engine.
  - [x] Remove the unused custom-species catalogue, two-line generation seed,
    Elder, generic Farewell, `0…100` needs, their source files, and their two
    obsolete test targets; only the P1 programme domain remains in the build.
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
  - [x] Replace the provisional translated home sprite with separately drawn
    P1 idle phases whose LCD origin and height are explicit frame data. One
    captured Mametchi idle trace is represented as one-bit frame data and is
    covered by a focused catalogue test.
  - [x] Manually transcribe both stable egg silhouettes from a fresh visual P1
    full-LCD trace, represent their true two-phase count and 0.70-second cadence,
    reject the earlier incomplete-crop geometry, and protect their corrected
    distinct geometry and rows with catalogue regression
    tests.
  - [x] Transcribe Babytchi's complete 36-phase home cycle from confirmed
    post-hatch 1× traces, including its alternating 6 × 6 full and 8 × 3
    compressed poses, horizontal path, 0.46-second cadence, and observed wrap;
    reject and replace geometry derived from an incomplete reference crop.
  - [x] Replace Marutchi's invented redraw with the two exact stable silhouettes
    spatially separated from waste in a full-LCD trace; preserve the clean-state
    origin/path as open evidence rather than inferring it from a care-state run.
  - [x] Replace the generic plus-sign waste marks with both exact observed 8 × 8
    phases, stack the first at `(24, 8)` and the second at `(24, 0)`, preserve
    the shared approximately one-second cadence, and verify the two-pile result
    at normal application scale. The finite Toilet/flush sequence remains open.
  - [x] Implement the observed Toilet wipe core as a transient framebuffer
    transform: 16 two-cell shifts, the exact repeating six-column water band,
    its measured cadence/final hold, and a blank closing phase. Block P1 input
    during it and regression-test the transform independently of CNA; keep the
    following form-specific celebration explicitly open.
  - [x] Replace the generic sickness plus with the exact fixed 7 × 7 skull and
    Babytchi's two observed 8 × 3 bottom poses at their real origin and cadence;
    keep later-form sick poses and illness/Medicine transitions open rather
    than reusing or inventing Babytchi art.
  - [x] Replace generic success feedback for the observed Marutchi Medicine
    action with its three exact full-LCD states, seven-phase order, sixteen-frame
    duration, input blocking, and deterministic framebuffer/timing tests; keep
    other-form treatment art explicitly open.
  - [x] Replace the generic Marutchi sleep symbol with the two exact observed Z
    arrangements and their 0.82-second cadence while preserving independent
    Marutchi-body and waste animation; keep every other form's sleep pose open.
  - [x] Replace the previous generic pink desktop shell treatment with a
    reusable CNA renderer and five independently authored P1 colour families;
    persist the selected shell without changing the LCD or P1 controls.
  - [x] Render held A/B/C caps as physically depressed for keyboard, mouse,
    and touch input; highlight the reset recess while held and restore a soft
    graduated contact shadow beneath the egg shell.
  - Compare and replace every remaining provisional home redraw before calling
    the character catalogue visually faithful; then capture its distinct care
    action frames rather than reusing home-idle art.
  - Add target sprite states and animations: idle, eating, sleeping, unhappy,
    ill, medicine, waste, attention, game, evolution, and end sequence.
  - Render P1’s checkerboard LCD field and exact permanent P1 icon placement;
    preserve the 32 × 16 logical resolution and one-bit pixels.

- [ ] **6. P1 controls and care menus**
  - [x] Add P1 clock flow: an unconfigured new/reset egg does not run or save;
    A sets the hour, B sets the minute, C confirms, B views the clock with no
    selected icon, C does not leave Clock view, A+C from that view enters SET,
    C confirms back to Clock view, and B returns home. Initial SET starts the
    egg directly; A+C starts an end-screen egg while retaining the clock.
  - [x] Preserve the original P1 Clock SET pause workaround: B opens Clock,
    A+C enters SET, all simulation and presentation timers freeze there, and C
    resumes without applying elapsed absence time. Persist this paused state
    and its displayed time so a desktop process restart also remains frozen.
  - [x] Reproduce the measured nominal ten-second home-icon selection timeout
    as programme display data, restart it after each A, and keep menu/action
    timing independent from this non-persisted UI timer.
  - [x] Replace the generic Light screen with both exact full-frame ON/OFF menu
    selections, their nominal ten-second inactivity timeout, and the filled
    lights-out framebuffer whose two Z phases are transparent and shifted eight
    cells left. Keep wake-up and other-form confirmation open.
  - Finish exact A/B/C navigation and clock behaviour in every transient
    animation and Attention state.
  - Replace legacy menus with P1 bread/candy Food, Light, Toilet, Health Meter,
    Medicine, Discipline, and unselectable Attention behaviour.
  - Preserve the desktop reset hold/confirmation only as a safety guard; its
    visible outcome must enter the P1 reset and clock-setup flow.

- [ ] **7. P1 Character game and evolution**
  - [x] Remove Number, UFO ending, and all remaining P2-only discriminator
    values from the programme model; the selected programme exposes only the
    international P1 Character-game and angel-ending behaviour.
  - Implement the five-round Character game, including left/right choices,
    result rules, happiness effect, and weight effect.
  - [x] Add a programme-data evolution-rule schema and deterministic traces for
    the documented classic Marutchi, visible teen/adult, A/B lineage, and
    four-discipline-bar branches (not modern rerelease discipline mistakes).
  - [x] Connect genuine fifteen-minute hunger/happiness/lights-off calls to
    care mistakes while excluding missed classic-P1 false calls; preserve the
    baby-stage reset before Marutchi.
  - [x] Add per-character hunger/happiness decay, sleep pauses, and bounded
    false-discipline calls from programme data; preserve their timer phase in
    P1 saves.
  - [x] Implement the original-P1 hidden Maskutchi → Bill age-ten wake-up
    rule as programme data with qualified and rejected resolver traces.

- [ ] **8. Completion of real-time P1 behaviour**
  - Implement verified P1 timers, calls, sleep/light mistakes, waste, illness,
    medicine, refusal, weight, and the death/life-span transition into the
    already-rendered angel-and-stars end screen.
  - [x] Clamp Bread/Candy and completed Character-game weight changes to each
    form's P1 minimum and 99 oz maximum (Babytchi remains fixed at 5 oz).
  - Process long offline periods faithfully and deterministically; test clock
    rollback, restart, and boundary moments.
  - Replace the generic current Farewell/new-egg flow.

- [ ] **9. Save migration and user documentation**
  - [x] Version P1 saves with an explicit target identifier.
  - [x] Archive incompatible legacy hybrid saves rather than converting them into
    fictitious P1 pets.
  - Rewrite the English static tutorial into detailed novice-friendly P1 parts,
    replacing all Pipple, Budbit, PEEK, and NUM material with P1 screenshots or
    accurately labelled illustrations.

- [ ] **10. Verification and release readiness**
  - Run domain, persistence, display, and smoke tests with at most two build
    jobs; avoid unnecessary clean builds and large generated files.
  - [x] Configure and compile the selected modular framework closure with both
    `SDL_RENDERER` and `HEADLESS`; retain the renderer-specific targets rather
    than restoring CNA's compatibility umbrella.
  - Compare all visible flows against the captured international P1 reference.
  - Document remaining desktop-only safeguards and known deviations clearly.

## Immediate next task

Follow the ordered visual and behaviour backlog in [next.md](next.md). Next,
capture a waste-free Marutchi home cycle and compare its path. The
explicit-frame renderer removes the incorrect generic bobbing behaviour, but
the remaining provisional character redraws and all care-action animations
still need a frame-by-frame comparison before they can be described as exact.
