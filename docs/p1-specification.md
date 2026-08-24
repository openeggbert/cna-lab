# International Tamagotchi P1 (1997) Specification Ledger

## Scope lock

This ledger applies only to the English-language international Generation 1
programme released in 1997. “P1” is fan shorthand; it must not silently mean a
Japanese 1996 unit, a Japanese P2 wave, an English P2, or a modern reissue.

P1 is represented as the first `ProgramDefinition` data package. The same
generic definition schema will later allow P2 data to reuse the engine without
mixing its content or rules into this P1 ledger.

The programme is implemented as clean C++ behaviour. This ledger must never
use a ROM, TamaLIB, or another emulator as an implementation dependency or
test oracle; evidence comes from public manuals, documented historical sources,
and reproducible observations.

Each entry has a confidence status:

- **Verified** — stated in a primary P1 manual or reproduced by a captured
  target-device trace.
- **Provisional** — supported by a secondary historical source and awaiting a
  target-device trace or primary period material.
- **Open** — do not make a fidelity claim until captured.

## Primary reference

- [Bandai Generation 1 instruction manual](https://www.bandai.com/amfile/file/download/file/3639/product/1309818/)
  — current official manual for the original P1 programme. This is the baseline
  for buttons, care functions, reset, hatching, Character game, medicine, and
  discipline.

## Display and control contract

| Rule | Status | Evidence / implementation consequence |
| --- | --- | --- |
| 32 × 16 one-bit game LCD, two four-icon bands | Provisional | Historical P1 documentation and device observation; retain the entire 32 × 16 game bitmap and render the fixed icon cells in its connected top/bottom surround. |
| A selects, B confirms, C cancels | Verified | Manual activation and care instructions. Do not add direct-care buttons. |
| Home icon selection clears after ten seconds without A | Verified | A 30 fps 1× trace kept Food dark for frames 16–310 and cleared it at frame 311, 9.83 seconds after its first stable frame. Represent the nominal ten seconds in programme display data; each A restarts it, menus pause it, and transient actions own their own input rules. |
| B shows the clock when no icon is selected and Attention is not lit | Verified in part | The Clock view remains until B returns; the P1 UI must not use B only as a generic menu key. |
| A+C on Clock enters clock setting; A+C on the clear home LCD toggles sound | Verified in part | A changes hours, B changes minutes, and C confirms back to Clock view; B then returns home. Initial/reset SET instead starts the egg directly. The current app retains the sound setting until P1 audio traces are implemented. |
| Leaving Clock SET open pauses the device | Verified in implementation | Match the classic P1 pause workaround: B opens Clock, A+C enters SET, and C resumes. While SET is visible, simulation, device-clock, action, timeout, and visible animation timers do not advance. The desktop save additionally restores this paused screen after a process restart without offline catch-up. |
| A+C starts a new egg at the end screen without setting the clock again | Verified in part | The existing P1 device clock is retained; only hardware reset enters SET. |
| Rear reset produces a pulsating egg and requires clock setup | Verified | P1 manual. Desktop hold/confirmation is a host safety guard before this result. |

## Care functions

| Function | Status | Target behaviour |
| --- | --- | --- |
| Food | Verified | Select Meal or Snack. A full pet refuses food. P1 international presentation is provisionally bread for Meal and wrapped candy for Snack. |
| Light | Verified | Exact ON/OFF menu. During sleep, OFF fills the 32 × 16 field and shows the sleep Z phases as clear cells; the light turns on automatically at wake-up. |
| Toilet | Verified | Flush droppings; leaving them causes illness. |
| Health Meter | Verified | Shows age/weight, discipline, hunger, and happiness pages. |
| Character game | Verified in part | Five turns. A chooses left, B chooses right; matching the direction raises happiness. The captured base success fraction is per form. |
| Medicine | Verified | A skull indicates sickness. Two or three doses can be needed. |
| Attention | Verified | It illuminates when the character needs care; it is not a selected action. |
| Discipline | Verified | Use after an unjustified call, meal refusal while hungry, or play refusal while unhappy. |

## Character catalogue

| Stage | International P1 character | Status |
| --- | --- | --- |
| Baby | Babytchi | Complete 36-phase home-idle and two-phase sick cycles transcribed; other visuals provisional |
| Child | Marutchi | Two home silhouettes and the observed sleep overlay transcribed; clean-state path provisional |
| Teen | Tamatchi, Kuchitamatchi | Provisional |
| Adult | Mametchi, Ginjirotchi, Maskutchi, Kuchipatchi, Nyorotchi, Tarakotchi | Provisional |
| Hidden international character | Bill | Provisional |

The roster is corroborated by historical character references. The application
now renders home animation with a per-sequence frame count, hand-authored LCD
data, and explicit origin and height, rather than translating one static
picture around the screen. One Mametchi idle trace, both stable egg silhouettes,
and a complete 36-phase Babytchi home cycle were visually transcribed from the
selected P1 reference; two Marutchi silhouettes and the two-phase stacked-waste
home overlay are also transcribed. Remaining
redraws, remaining care-action frames, and every branch condition remain open
until they are compared frame by frame with the selected target programme. No ROM,
emulator core, or reference-programme data is included in this project.

## Home-LCD visual reference ledger

This ledger is the gate for changes to `P1SpriteCatalog.cpp`. “Observed” means
viewed in the external reference on a virtual display; it does not mean that a
screenshot, ROM byte, or emulator asset is kept in this repository. A row may
become “transcribed” only when the one-bit cells were read visually and written
by hand into the catalogue. It is deliberately acceptable for an entry to stay
open rather than substitute a plausible modern redraw.

| Form / sequence | Observation and geometry | Catalogue status | Next evidence needed |
| --- | --- | --- | --- |
| Egg idle sequence | Fresh unaccelerated 30 fps audit over the complete LCD: hand-read wide 12 × 10 phase at `(10, 3)` and tall 10 × 11 phase at `(11, 2)`. Stable changes begin every 21–22 host frames, represented as 0.70 seconds per phase; partial LCD-write frames are excluded. | Two corrected phases transcribed; every row, geometry, count, cadence, wrapping, and corrected normal-scale placement checked. | Capture cracking/hatching separately. |
| Babytchi | Fresh confirmed post-hatch 30 fps traces at 1× over the complete LCD: two 6 × 6 full poses at `y=9`, followed by two 8 × 3 compressed poses at `y=13`, repeat while moving horizontally. The complete stable-origin cycle is `7, 11, 8, 11, 15, 18, 14, 12, 10, 7, 10, 8, 12, 15, 17, 14, 13, 10, 6, 10, 9, 12, 14, 17, 15, 13, 9, 6, 11, 9, 11, 14, 18, 15, 12, 9`. It repeated in a separate trace. Stable phases average about 0.46 seconds; incremental one-host-frame LCD writes are excluded. | Complete 36-phase cycle transcribed; every pose row, origin, bound, count, cadence, wrap, and corrected normal-scale placement checked. | Capture Babytchi care-action sequences separately. |
| Babytchi sick state | Ten-second complete-LCD 30 fps 1× trace: a fixed 7 × 7 skull at `(25, 1)` accompanies two 8 × 3 bottom poses at `(12, 13)`: `.######. / #..##..# / ########` and `..####.. / .#.##.#. / ########`. Stable pose changes begin every 27–29 host frames, represented as 0.93 seconds. Waste continues on its independent two-phase cadence. | Both sick poses, skull cells, geometry, cadence, wrap, fallback for unobserved forms, and a five-second normal-scale sick-plus-waste run checked. | Capture illness onset, Medicine action, recovery, and sick poses for every later form separately. |
| Marutchi | Confirmed Baby → Child run, complete-LCD 30 fps trace at 1×: 10 × 9 long pose and 10 × 8 short pose, both at `(11, 3)`, alternate every 27–28 host frames (0.92 seconds). Two waste piles were spatially separate at `x≥24`; their cells are excluded from every transcribed row. | Both stable silhouettes, cadence, two-phase count, wrapping, and normal-scale application placement checked. A clean-state horizontal path is not inferred from this waste-present trace. | Capture the same reference cycle without waste and compare its origins. |
| Marutchi sleep state | Complete-LCD 30 fps 1× trace: the normal long/short Marutchi body keeps alternating independently while two Z arrangements alternate every 24–25 host frames (0.82 seconds). The small arrangement occupies 7 × 6 cells at `(24, 0)` and the large Z occupies 4 × 6 cells at `(25, 2)`. Existing waste continues on its own cadence. | Both exact Z phases, geometry, cadence, wrapping, independent Marutchi-body timing, independent waste timing, and a six-second normal-scale application run checked. | Capture sleep poses for every other form separately; do not infer that their body animation matches Marutchi. |
| Light menu / lights out | Complete-LCD 30 fps 1× flow on sleeping Marutchi. The menu has exact large ON/OFF lettering and a 6 × 7 marker at `(1, 1)` or `(1, 9)`. A changes the selection, B confirms, and inactivity returns home on the same nominal ten-second clock as icon selection. Confirming OFF fills the entire field and cuts the same two sleep-Z glyphs out at `(16, 0)` and `(17, 2)`, eight cells left of their normal-light origins; character and waste are absent. | Both complete 32 × 16 menu selections, all marker/text cells, lights-out fill, 11/12-cell transparent Z phases, horizontal shift, programme-data timeout, and normal-scale application timeout checked. | Capture the wake-up transition and verify whether every later form uses the same lights-out Z placement. |
| Waste home overlay | Complete-LCD 1× traces with one and two piles: each pile is an 8 × 8 glyph at `x=24`; the first occupies `(24, 8)` and the second stacks at `(24, 0)`. Both use the same two stable one-bit phases and change together about once per second. | Both exact phases, bounds, stacking positions, cadence, direct wrapping, and normal-scale two-pile placement checked. | Capture Toilet/flush as a separate finite action sequence. |
| Toilet wipe core | Complete-LCD 30 fps trace after selecting Toilet with one pile: the entire source framebuffer moves left two cells per stable phase while a six-column band enters from the right. Its four repeating rows are `..##.#`, `.##.#.`, `##.#..`, `.##.#.`. Sixteen positions take the band from the clipped right edge to `x=0`; stable starts average about 0.10 seconds apart, the complete left-edge band holds about 0.30 seconds, then the LCD is briefly blank. Incremental host-frame writes are excluded. | Deterministic framebuffer transform, exact band cells, clipping, cadence, final hold, blank phase, input blocking, and normal-scale two-pile application run checked. | Capture and transcribe the following character-specific post-flush celebration before calling the whole Toilet action complete. |
| Tamatchi / Kuchitamatchi | No selected-reference home sequence has been transcribed. | Provisional. | Observe one trace for each lineage. |
| Mametchi | Three independently drawn home phases are retained with a focused distinct-frame test. | Transcribed; regression checked. | Recheck the complete sequence at normal LCD scale. |
| Ginjirotchi / Maskutchi / Kuchipatchi / Nyorotchi / Tarakotchi / Bill | No selected-reference home sequence has been transcribed. | Provisional. | Capture the relevant qualified evolution path before replacing one form at a time. |

Reference-session note: a fresh unaccelerated TamaTool process was observed on
a dedicated Xvfb display. A lossless 30 fps working capture showed two stable
egg silhouettes; each transition was written over one or two host frames,
which are not additional animation phases. Stable transition starts were
21–22 frames apart. A second run confirmed activation by setting the timer and
explicitly returning to the home egg, used 10× only to pass the approximately
five-minute hatch wait, then captured Babytchi at 1×. Its stable poses lasted
mostly 12–15 frames at 30 fps, with one-host-frame partial LCD writes between
them. Working screenshots, video, and emulator saves remain outside this
repository. An Xvfb process is ephemeral between tooling sessions, so relaunch
the reference and its virtual display before drawing conclusions from a
missing later capture.


Capture-geometry correction (2026-08-24): TamaTool v0.1 lays out each logical
cell on a 10-pixel stride with a 9 × 9 active square. An earlier 288 × 144
working crop was therefore not a complete 32 × 16 matrix and its Babytchi
coordinates and lower rows were rejected. The replacement trace covers the
319 × 159 active matrix extent and samples each logical cell at its known
centre. The 36-phase period and approximately 0.46-second cadence survived the
correction; all egg and Babytchi pose rows, bounds, and origins were independently replaced.
## Evolution-rule representation

The shared engine represents a growth chart as ordered `EvolutionRule` data:
source character, target character, care-mistake range, visible-discipline-bar
range, optional discipline-mistake range for programmes that need it, an
optional A/B teen lineage, and an optional target value for the visible
discipline meter. The target meter value matters because P1 forms do not all
carry the displayed bars unchanged through an evolution.

The P1 package supplies the following classic visible branches. `A` and `B`
are hidden versions of an otherwise identically named teenager; `D` is the
four-segment 0–4 discipline meter at the adult transition.

| Teen | Version | Care mistakes | D | Adult |
| --- | --- | --- | --- | --- |
| Tamatchi | A | 0–2 | 4 | Mametchi |
| Tamatchi | A | 0–2 | 3 | Ginjirotchi |
| Tamatchi | A | 0–2 | 0–2 | Maskutchi |
| Tamatchi | A | 3+ | 4 / 3 / 0–2 | Kuchipatchi / Nyorotchi / Tarakotchi |
| Tamatchi | B | 0–2 | 4 / 0–3 | Ginjirotchi / Maskutchi |
| Tamatchi | B | 3+ | 4 / 0–3 | Nyorotchi / Tarakotchi |
| Kuchitamatchi | A | any | 4 / 3 / 0–2 | Kuchipatchi / Nyorotchi / Tarakotchi |
| Kuchitamatchi | B | any | 4 / 0–3 | Nyorotchi / Tarakotchi |

At the Marutchi transition, 0–1 care mistakes selects Tamatchi and 2+ selects
Kuchitamatchi. Three or four discipline bars records hidden version A; zero to
two records B. The simulator has deterministic traces for each data-defined
visible adult target. The visible meter then starts at 50% for a version-A
teen and 0% for a version-B teen. Adults start at 100% (Mametchi and
Kuchipatchi), 50% (Ginjirotchi and Nyorotchi), or 0% (Maskutchi and
Tarakotchi).

This is **provisional** source-backed behaviour, not yet a final fidelity
claim. The shared simulator now produces and resolves the fifteen-minute
Hunger, Happiness, and lights-off Attention windows, records genuine missed
calls, and excludes missed classic-P1 false discipline calls. It also applies
the captured per-character Hunger/Happy loss rates only while the form is
awake, and raises bounded false Discipline calls from the documented combined
heart-decrement cadence. Their timer phase is persisted. The later
international Maskutchi → Bill branch is represented as a later data rule:
only the type-B, zero-discipline teen history is eligible, it is checked at
the age-ten wake-up, and the target Bill meter starts at 100%. This remains
provisional pending a selected-target trace.

## Timing and evolution capture table

| Rule | Status | Required trace before implementation is final |
| --- | --- | --- |
| New egg hatches after about five minutes | Verified | Reset/activation → clock setup → hatch. |
| Baby → child timing | Verified | Babytchi evolves to Marutchi after 65 minutes. |
| Child → teen timing and branch | Verified in part | At age 3, 0–1 care mistakes yields Tamatchi; 2+ yields Kuchitamatchi; 75–100% discipline records version A, 0–50% version B. |
| Teen → adult timing and matrix | Verified in part | At age 6, the form is selected by teen type, care mistakes, and 25% discipline bands. Capture each branch in tests. |
| Maskutchi → Bill | Provisional implemented | A type-B Maskutchi whose teen began with zero discipline changes at the age-ten wake-up; qualified and rejected paths are deterministic tests. |
| Hunger/happiness decay | Provisional implemented | The P1 data package carries the captured loss rates for every form; the simulator decrements only while awake and persists each timer phase. |
| Attention window and care-mistake criteria | Provisional implemented | The simulator implements 15-minute hungry/happy/lights-off and false-call windows, a genuine-call care mistake, no classic-P1 mistake for a missed false call, and the documented six/seven-decrement false-call cadence. |
| Sleep/wake time per character | Verified in part | The captured per-character schedules drive sleep, light calls, wake-up age increments, and the pause in need loss. Capture target traces for remaining life-cycle timing. |
| Waste, sickness, medicine doses | Verified in part | Babytchi waste at 15/45 minutes and sickness at minute 33 requiring two doses are traced; capture later-stage triggers and recovery. |
| Weight changes, minimum weight, game effect | Open | Record Meal, Snack, game win, game loss, and refusal boundaries. |
| Adult life span and end sequence | Open | Record well-cared and neglected adult runs. |

## Captured P1 data values

- Babytchi begins at 5 oz, loses hearts every 2–3 minutes, poops at minutes 15
  and 45, becomes ill at minute 33 (two medicine doses), naps from minute 40
  to 45, and becomes Marutchi at minute 65. None of its care affects later
  evolution.
- Marutchi begins at 10 oz and sleeps 20:00–09:00; Tamatchi and Kuchitamatchi
  begin at 20 oz and sleep 21:00–09:00.
- Need loss / false-call cadence is data per form: Babytchi loses Hungry/Happy
  at 3/4 awake minutes; Marutchi at 50/60 and then calls after six real heart
  decrements; both teenagers at 75/85 and six; Mametchi/Bill at 81/91 with no
  false call; Ginjirotchi at 81/91 and seven; Maskutchi at 55/65 and seven;
  Kuchipatchi at 60/70 with none; Nyorotchi at 60/70 and seven; Tarakotchi at
  45/50 and seven. Calls are capped at the number that could fill the form's
  initial visible Discipline meter.
- Character-game success chance is also per-form programme data: 50% for all
  P1 forms except Maskutchi (10/32, 31.25%) and Kuchipatchi (22/32, 68.75%).
  The desktop game's persisted deterministic sequence resolves the shown
  direction after the A/B guess, so a restart does not replace a sequence with
  a new host-random result.
- Adult minimum weight / sleep pairs are: Mametchi and Ginjirotchi 30 oz,
  22:00–09:00; Maskutchi 30 oz, 23:00–11:00; Kuchipatchi 20 oz,
  22:00–09:00; Nyorotchi 10 oz, 22:00–09:00; Tarakotchi 20 oz,
  22:00–10:00; Bill 30 oz, 22:00–09:00.
- Babytchi's weight is fixed at 5 oz. Every later P1 form has a recorded
  minimum weight and a 99 oz upper bound; Bread/Candy and a completed
  Character game clamp at those per-form limits.
- The Health Meter has four pages: age/weight (ounces in international P1),
  Discipline, Hungry, and Happy. Both heart meters range from 0 to 4;
  discipline moves in 25% increments.
- The P1 visible discipline meter is initialised on each form change: type-A
  teens begin at 50%, type-B teens at 0%; Mametchi/Kuchipatchi begin at 100%,
  Ginjirotchi/Nyorotchi at 50%, and Maskutchi/Tarakotchi at 0%. This is
  provisional reverse-engineering evidence, represented as data on the
  relevant evolution rule rather than a hard-coded engine exception.

The visible controls, baby trace, sleep schedule, and classic chart are
captured from [Thaao's original-P1 care guide](https://thaao.net/tama/p1/) and
its [P1 character guide](https://thaao.net/tama/p1/?p=chara). The per-form
loss rates, false-call cadences, and initial meter values are provisional
reverse-engineering evidence from the [P1/P2 evolution and base-stat
record](https://www.tamatalk.com/threads/tamagotchi-p1-p2-evolution-guide.200023/).
The official manual remains the primary source for visible controls and care
functions. All secondary-source values must still be checked against target
traces before release.

## Explicit exclusions

- P2 characters, hamburger/cake food, Number game, wavy background, and UFO
  ending.
- Japanese P1 text/food/end-screen variants.
- Modern reissue snack penalties and quality-of-life additions.
- Legacy CNA Tamagotchi Pipple/Budbit forms, PEEK/NUM lines, random seed
  selection, Elder, and generic Farewell presentation.
