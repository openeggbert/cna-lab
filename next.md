# CNA Tamagotchi — Next Work

## Status at the start of this backlog

The active product target is the international English Tamagotchi P1 (1997),
implemented as a clean, data-driven C++ behaviour engine. The LCD framebuffer
is exactly 32 × 16 and one bit. The home renderer now uses a centred 16 × 10
cell with three independent idle frames, so it no longer fakes motion by
shifting a static creature around the LCD. A Mametchi idle sequence and the
first asymmetric egg silhouette have been visually transcribed from P1
reference traces; the remaining egg phases and other character redraws remain
provisional.

The project must never ship a P1 ROM, a ROM-derived binary asset, TamaLIB, or
another emulator core. A reference program may be viewed externally only to
write and verify the clean implementation.

## Context handoff — 2026-08-22

- The CMake integration was updated for the current sibling `../cna` and its
  modular `../sharp-runtime`. `CNA_GRAPHICS_RENDERER` accepts only
  `SDL_RENDERER` or `HEADLESS`; the application links `CNA::Runtime` plus the
  chosen renderer rather than CNA's compatibility umbrella. The explicit
  sharp-runtime closure and `CNA_ENABLE_DRACO=OFF` are intentional.
- The SDL renderer preset configures, builds, and passes all nine CTest tests
  with `--parallel 2`. Keep the two-job ceiling. An initial two-job build saw
  a transient static-library archiving failure, but an unchanged incremental
  rerun succeeded; investigate only if that failure recurs.
- A clean SDL application run was checked on Xvfb with
  `SDL_VIDEODRIVER=x11`, `WAYLAND_DISPLAY` unset, and an isolated
  `XDG_DATA_HOME` under `/tmp`. The initial clock setup needs a held virtual
  `C` input; an instantaneous synthetic key can be missed by the polling loop.
  Root-window captures can alternate with an empty GL backbuffer under Xvfb;
  capture again before treating a frame as absent.
- TamaTool v0.1 was used only as an external visual reference in a separate
  Xvfb display. Do not add its executable, ROM, screenshots, extracted data,
  TamaLIB, or any other emulator artefact to this repository. The egg rows in
  `P1SpriteCatalog.cpp` were manually written from the visible LCD grid, not
  imported or algorithmically extracted.
- The next concrete visual task is to observe and transcribe the remaining
  egg phases, then build a per-form reference ledger before changing further
  provisional sprites. Retain a focused test for each manually verified row.

## Priority 1 — Make the home LCD visually faithful

1. Complete the egg's remaining idle phases, then create a visual-reference
   ledger for each P1 home form: egg, Babytchi,
   Marutchi, Tamatchi, Kuchitamatchi, Mametchi, Ginjirotchi, Maskutchi,
   Kuchipatchi, Nyorotchi, Tarakotchi, and Bill.
2. For each form, identify the stable 32 × 16 cell origin, its true idle-frame
   count, and the pixel changes between frames. Record uncertainty rather than
   inventing a source value.
3. Replace the provisional redraw of one form at a time with independently
   written 16 × 10 one-bit frame data. Keep the public sprite catalogue free
   from source-ROM data and make no use of frame translation as animation.
4. Extend `P1SpriteCatalogTests` for every verified sequence: all rows must be
   sixteen pixels wide, all frames must remain inside the centred LCD cell,
   and the expected frame differences must be explicit.
5. Compare the rendered result against the P1 reference at normal LCD scale,
   not only a magnified bitmap. Verify that the character stays centred within
   the 32 × 16 field and does not overwrite the physical face-icon bands.

**Acceptance condition:** the entire home roster has reference-compared idle
frames, with known uncertainty stated in `docs/p1-specification.md`; no form
uses the previous translated-sprite bobbing behaviour.

## Priority 2 — Add P1-specific action and transition visuals

1. Capture separate frame sequences for egg cracking/hatching, eating Bread,
   eating Candy, Character game play, sleeping, unhappy/refusal, illness,
   medicine, waste, attention, discipline, evolution, death, and the
   angel-and-stars ending.
2. Add a rendering state key to the programme/UI boundary. The renderer must
   select a named P1 action sequence; it must not infer an action by mutating
   or replacing the persistent pet state.
3. Define action duration, frame cadence, interruption rules, and what A, B,
   and C do while each action is on screen. Keep those rules separate from the
   one-bit drawing data.
4. Add deterministic display/controller tests for each finite animation:
   start frame, frame order, completion, cancellation where P1 permits it,
   and return to the expected screen.

**Acceptance condition:** every implemented care operation has a distinct P1
visual sequence instead of text-only feedback or a generic symbol.

## Priority 3 — Close the remaining P1 behaviour gaps

1. Verify and implement the P1 adult waste cadence, illness triggers,
   medicine recovery rules, refusal behaviour, neglect/death path, and exact
   life-span/end transition.
2. Replace the current wall-clock evolution approximations with verified P1
stage timing, including sleep and wake boundaries. Preserve deterministic
offline catch-up without writing a save every minute.
3. Resolve conflicting historical claims about care mistakes and discipline by
   adding an evidence entry before changing the evolution resolver. Do not
   silently combine P1 and P2 or modern rerelease rules.
4. Exercise qualified and rejected traces for all visible P1 adult branches
   and the Maskutchi → Bill condition.

**Acceptance condition:** every implemented P1 rule has an evidence label,
an executable trace, and a stated target revision.

## Priority 4 — Validate and document a usable release candidate

1. Keep all builds incremental and use at most two CPU jobs. Do not clean
   the existing build directory or generate large derived image sets.
2. Run the domain, persistence, display, sprite-catalogue, controller, and
   smoke tests after each cohesive change.
3. Update the English user tutorial with screenshots only after the matching
   visual flow has been reference-compared. Label illustrations honestly until
   they are exact.
4. Maintain a concise deviation list in `docs/p1-specification.md` until the
   reference comparison is complete.

**Release condition:** the README, tutorial, P1 specification, and observable
behaviour all describe the same scope; verified P1 facts are distinguished
from remaining work; the repository remains ROM-free and emulator-free.
