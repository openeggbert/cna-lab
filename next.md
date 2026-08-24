# CNA Tamagotchi — Next Work

## Status at the start of this backlog

The active product target is the international English Tamagotchi P1 (1997),
implemented as a clean, data-driven C++ behaviour engine. The LCD framebuffer
is exactly 32 × 16 and one bit. The home renderer uses explicit geometry and
an explicit per-sequence frame count, so it no longer fakes motion by shifting
a static creature around the LCD. Most provisional frames use the centred
16 × 10 cell. The egg's two stable silhouettes, Babytchi's complete 36-phase
home and two-phase sick cycles, two stable Marutchi silhouettes, the exact
two-phase Marutchi sleep overlay, and a Mametchi idle sequence have
been visually transcribed from P1 reference traces. The exact two-phase stacked
waste overlay is also implemented; Marutchi's clean-state path
and the other character redraws remain provisional.

The project must never ship a P1 ROM, a ROM-derived binary asset, TamaLIB, or
another emulator core. A reference program may be viewed externally only to
write and verify the clean implementation.

## Context handoff — 2026-08-24

- The CMake integration was updated for the current sibling `../cna` and its
  modular `../sharp-runtime`. `CNA_GRAPHICS_RENDERER` accepts only
  `SDL_RENDERER` or `HEADLESS`; the application links `CNA::Runtime` plus the
  chosen renderer rather than CNA's compatibility umbrella. The explicit
  sharp-runtime closure and `CNA_ENABLE_DRACO=OFF` are intentional. CNA's
  current `Runtime` target owns graphics/input/content/audio/media as one API
  closure; this game disables the genuinely unused network/ENet, CNAEXT, and
  device-extension branches before CNA is added.
- Both supported modular renderer presets configure and build: SDL also passes
  all ten CTest tests with `--parallel 2`, and a fresh HEADLESS application
  build completed on 2026-08-24. Keep the two-job ceiling. This runner can
  intermittently fail while `ar` replaces a static library (observed with both
  two and one job); retry the unchanged incremental build before attributing
  that message to a source or dependency change.
- A clean SDL application run was checked on Xvfb with
  `SDL_VIDEODRIVER=x11`, `WAYLAND_DISPLAY` unset, and an isolated
  `XDG_DATA_HOME` under `/tmp`. The initial clock setup needs a held virtual
  `C` input; an instantaneous synthetic key can be missed by the polling loop.
  Root-window captures can alternate with an empty GL backbuffer under Xvfb;
  capture again before treating a frame as absent.
- A dedicated 30 fps 1× trace measured the inactive-home icon timeout: Food
  stayed dark through frame 310 after becoming stable at frame 16 and cleared
  at frame 311, or 9.83 seconds of visible selection. The nominal 10.0 seconds
  now lives in `DisplayDefinition`; each home A resets the transient timer and
  a 30 fps application trace showed exactly 300 dark-marker frames. The timer
  is not persisted and does not close menus or interrupt named action visuals.
- A direct existing-pet clock session verified that C does not leave Clock view,
  A+C enters SET, C confirms that SET back to Clock view, and B alone returns
  home. The application now remembers whether SET came from Clock view so the
  initial/reset SET can still start the egg directly. A missed synthetic C let
  the attempted 08:59 wake boundary pass while SET was still open, so no wake
  transition frames are accepted from that attempt; a later 30-second home run
  contained only the already transcribed two stable Marutchi silhouettes.
  A six-state normal-scale CNA run confirmed Clock → C/Clock → SET → C/Clock
  → B/Home with the corrected implementation.
- The hand-drawn device treatment now uses the selected reference's turquoise
  shell family and yellow buttons. It was inspected on a separate Xvfb screen;
  its colours and geometry are authored C++ values, with no reference image
  added to the repository. Continue to treat this as an approximate shell
  treatment rather than a claim that every retail P1 shell is identical.
- TamaTool v0.1 was used only as an external visual reference in a separate
  Xvfb display. Do not add its executable, ROM, screenshots, extracted data,
  TamaLIB, or any other emulator artefact to this repository. The egg rows in
  `P1SpriteCatalog.cpp` were manually written from the visible LCD grid, not
  imported or algorithmically extracted.
- A freshly started, unaccelerated reference run was recorded at 30 fps. Its
  corrected stable egg silhouettes change every 21–22 host frames; the
  catalogue uses 0.70 seconds per phase. Variable-length home
  sequences are represented explicitly, so the egg wraps wide → tall → wide
  without an artificial A/B/A pause.
- The wide egg phase is a hand-read 12 × 10 cell at `(10, 3)` and the tall phase
  is a hand-read 10 × 11 cell at `(11, 2)`. Partial LCD writes visible across
  one or two host frames were excluded. Focused tests protect both bounds,
  every hand-read row, timing, active frame count, and direct wrapping.
- A fresh three-second same-display trace confirmed both corrected phases at
  normal LCD scale: they remain centred, wrap directly, and stay inside the
  32 × 16 game field without touching either permanent icon band. The working
  capture is outside the repository.
- A confirmed activation and post-hatch capture established Babytchi's real
  home motion. At 1× it repeats two full 6 × 6 poses at `y=9`, then two
  compressed 8 × 3 poses at `y=13`, while moving through observed horizontal
  origins. Its complete 36-phase cycle is hand-transcribed at an inferred
  0.46-second cadence; focused tests protect every row, origin, bound, count,
  and wrap. A separate clean 30-second trace showed the full origin sequence
  repeat and resolved the former phase-20 uncertainty.
- A new five-second normal-scale application trace using an isolated clean save
  confirms both corrected poses stay inside the LCD without touching the icon
  bands. Continue the per-form home ledger with Marutchi; capture Babytchi
  care-action sequences under Priority 2.
- A later layout audit found that the original 288 × 144 working crop did not
  cover TamaTool v0.1's 10-pixel-stride matrix. That crop's coordinates and
  lower rows were discarded. Accepted replacement traces cover the complete
  319 × 159 active matrix extent, sample the centre of every logical cell, and
  yield the corrected egg and Babytchi data above. Do not infer a 32 × 16 grid
  by merely resizing a reference crop; derive its stride and extent first.

- A confirmed Baby → Child run yielded Marutchi at 1×. Its spatially separate
  left-hand character alternates exact 10 × 9 and 10 × 8 silhouettes at
  `(11, 3)` every 27–28 host frames, represented as 0.92 seconds. Two right-hand
  waste piles were excluded from the hand-written rows. The clean-state
  horizontal path remains open because synthetic XTest release events do not
  reliably release P1 A in this TamaTool v0.1 session.
- A four-second normal-scale application trace with an isolated waste-free
  save confirms both transcribed Marutchi silhouettes fit and wrap cleanly.
- One- and two-pile reference traces establish an 8 × 8 waste glyph at
  `(24, 8)`, with a second copy stacked at `(24, 0)`. Both exact hand-read
  phases animate together at about 1.0 second. Catalogue tests protect their
  rows, geometry, cadence, and wrap; a six-second normal-scale two-pile
  application trace confirms placement. The Toilet/flush action remains open.
- A subsequent 30 fps Babytchi Toilet trace establishes the wipe core. It moves
  the complete pre-clean framebuffer left by two cells through 16 positions
  while the exact six-column water band `..##.# / .##.#. / ##.#.. / .##.#.`
  enters from the right. The implementation uses 0.10 seconds per moving phase,
  holds the complete band at the left for 0.30 seconds, then briefly blanks the
  LCD. Display tests protect the transform, clipping, all water cells, timing,
  and completion; a normal-scale two-pile Marutchi application run was checked.
  The character-specific post-flush celebration remains open evidence.
- A natural Babytchi illness on the retained 1× reference produced a repeating
  ten-second trace. The fixed skull is an exact 7 × 7 glyph at `(25, 1)`; the
  character alternates exact wide/narrow 8 × 3 poses at `(12, 13)` every
  27–29 host frames, represented as 0.93 seconds. Waste remains an independent
  overlay. Catalogue tests protect every row, bound, cadence, wrap, and the
  deliberate normal-pose fallback for unobserved sick forms; a five-second
  normal-scale application run with one waste pile was checked. Illness onset,
  Medicine/recovery, and later-form sick poses remain open.

- The naturally evolved reference later showed sleeping Marutchi. Its regular
  long/short body cycle continues while an independent two-phase Z overlay
  alternates every 24–25 host frames (0.82 seconds): a 7 × 6 arrangement at
  `(24, 0)` and a 4 × 6 Z at `(25, 2)`. Existing waste also continues on its
  own cadence. Exact catalogue tests and a six-second normal-scale application
  run cover this combination. Sleep poses for every other form remain open.
- A same-session sleeping-Marutchi Light trace established both complete ON/OFF
  menu frames, the 6 × 7 marker positions, A/B behaviour, and the nominal
  ten-second inactivity return. Confirming OFF fills all 512 field cells, then
  renders the existing 11/12-cell Z phases as transparent holes at `(16, 0)`
  and `(17, 2)`, eight cells left of the light-on overlay; character and waste
  disappear. `P1LightScreen` keeps this presentation independent of the
  persistent light flag. Exact display tests and a normal-scale 30 fps trace
  showing exactly 300 menu frames before timeout were checked. Other-form
  lights-out placement and wake-up remain open.

## Priority 0 — Add selectable physical shell variants

1. [x] Replace the provisional flat shell drawing with a reusable CNA shell renderer
   that models the rim, translucent or opaque body, recessed LCD bezel, three
   physical buttons, reset pinhole, highlights, and material depth without a
   detached floor shadow.
2. [x] Add historically grounded P1 colour families, beginning with Translucent
   Blue/Yellow, Blue/Yellow, Pink/Yellow, Green/Yellow, and White/Blue.
3. [x] Provide an in-application host control for cycling the shell; it must not
   consume an original P1 A/B/C action or alter the 32 x 16 simulation.
4. [x] Persist the selected shell identifier in the versioned save format, retain a
   safe default for existing saves, and add save/load validation tests.
5. [x] Capture every variant on the same virtual display and compare silhouette,
   bezel, button offset, reset recess, highlights, and material treatment.

**Acceptance condition met (2026-08-24):** `V` cycles all five shell treatments,
format-5 persistence survives a process restart, pre-v5 saves receive the safe
default, same-display captures retain identical LCD geometry, and the
shell-control path does not mutate P1 state or framebuffer data.

## Priority 1 — Make the home LCD visually faithful

1. [x] Complete and regression-check the egg's two stable idle phases.
2. [x] Capture and transcribe Babytchi's complete 36-phase home cycle, including
   its true full/compressed geometry, horizontal path, cadence, wrap, and
   partial-write exclusion.
3. Transcribe Marutchi's two waste-separated stable silhouettes and cadence;
   capture a waste-free run before accepting its home origin/path as complete.
4. Create a visual-reference ledger for each remaining P1 home form: Marutchi,
   Tamatchi, Kuchitamatchi, Mametchi, Ginjirotchi, Maskutchi,
   Kuchipatchi, Nyorotchi, Tarakotchi, and Bill.
5. For each form, identify the stable 32 × 16 cell origin, its true idle-frame
   count, and the pixel changes between frames. Record uncertainty rather than
   inventing a source value.
6. Replace the provisional redraw of one form at a time with independently
   written one-bit frame data at its observed bounds. Keep the catalogue free
   from source-ROM data and make no use of frame translation as animation.
7. Extend `P1SpriteCatalogTests` for every verified sequence: each phase's rows
   must share its true observed width, all frames must remain inside the 32 × 16
   LCD, and the expected geometry and frame differences must be explicit.
8. Compare the rendered result against the P1 reference at normal LCD scale,
   not only a magnified bitmap. Verify that the character stays centred within
   its observed motion range and does not overwrite the physical face-icon bands.

**Acceptance condition:** the entire home roster has reference-compared idle
frames, with known uncertainty stated in `docs/p1-specification.md`; no form
uses the previous translated-sprite bobbing behaviour.

## Priority 2 — Add P1-specific action and transition visuals

1. [x] Replace the generic home-screen waste marks with the exact observed
   two-phase 8 × 8 glyph, stacking, and cadence. This does not complete the
   distinct Toilet/flush action.
2. [x] Implement the observed Toilet wipe core as a transient visual that
   transforms a framebuffer snapshot without persisting animation state or
   accepting A/B/C during the wipe. Keep its following character celebration
   open until a complete reference trace is captured.
3. [x] Replace the generic sickness plus with the exact common 7 × 7 skull and
   both observed Babytchi bottom poses. Do not invent sick poses for later forms.
4. [x] Replace the generic sleep symbol for Marutchi with both exact observed Z
   arrangements and preserve the independent body and waste animation clocks.
   Do not reuse this body behaviour for unobserved forms.
5. [x] Replace the generic Light text with both exact ON/OFF menu frames, the
   measured inactivity timeout, and the filled lights-out LCD with shifted,
   transparent Z phases.
6. Capture separate frame sequences for egg cracking/hatching, eating Bread,
   eating Candy, Character game play, sleeping for every remaining form,
   unhappy/refusal, illness,
   medicine, waste, attention, discipline, evolution, death, and the
   angel-and-stars ending.
7. Add a rendering state key to the programme/UI boundary for the remaining
   actions. The renderer must
   select a named P1 action sequence; it must not infer an action by mutating
   or replacing the persistent pet state.
8. Define action duration, frame cadence, interruption rules, and what A, B,
   and C do while each action is on screen. Keep those rules separate from the
   one-bit drawing data.
9. Add deterministic display/controller tests for each finite animation:
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
