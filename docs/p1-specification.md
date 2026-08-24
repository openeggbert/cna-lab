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
| B shows the clock when no icon is selected and Attention is not lit | Verified in part | The Clock view remains until B returns; the P1 UI must not use B only as a generic menu key. |
| A+C on Clock enters clock setting; A+C on the clear home LCD toggles sound | Verified in part | A changes hours, B changes minutes, C confirms. The current app retains the sound setting until P1 audio traces are implemented. |
| A+C starts a new egg at the end screen without setting the clock again | Verified in part | The existing P1 device clock is retained; only hardware reset enters SET. |
| Rear reset produces a pulsating egg and requires clock setup | Verified | P1 manual. Desktop hold/confirmation is a host safety guard before this result. |

## Care functions

| Function | Status | Target behaviour |
| --- | --- | --- |
| Food | Verified | Select Meal or Snack. A full pet refuses food. P1 international presentation is provisionally bread for Meal and wrapped candy for Snack. |
| Light | Verified | Select On/Off. Turn light off when the character sleeps; it turns on automatically at wake-up. |
| Toilet | Verified | Flush droppings; leaving them causes illness. |
| Health Meter | Verified | Shows age/weight, discipline, hunger, and happiness pages. |
| Character game | Verified in part | Five turns. A chooses left, B chooses right; matching the direction raises happiness. The captured base success fraction is per form. |
| Medicine | Verified | A skull indicates sickness. Two or three doses can be needed. |
| Attention | Verified | It illuminates when the character needs care; it is not a selected action. |
| Discipline | Verified | Use after an unjustified call, meal refusal while hungry, or play refusal while unhappy. |

## Character catalogue

| Stage | International P1 character | Status |
| --- | --- | --- |
| Baby | Babytchi | Provisional |
| Child | Marutchi | Provisional |
| Teen | Tamatchi, Kuchitamatchi | Provisional |
| Adult | Mametchi, Ginjirotchi, Maskutchi, Kuchipatchi, Nyorotchi, Tarakotchi | Provisional |
| Hidden international character | Bill | Provisional |

The roster is corroborated by historical character references. The application
now renders three-phase home animation as hand-authored LCD frame data with an
explicit origin and height, rather than translating one static picture around
the screen. One Mametchi idle trace and one egg silhouette were visually
transcribed from the selected P1 reference. The remaining egg phases, character
redraws, all care-action frames, and every branch condition remain open until
they are compared frame by frame with the selected target programme. No ROM,
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
| Egg, asymmetric idle phase | Hand-read 16 × 10 drawing at `(8, 3)`; its crown, crack, and lower shell have focused regression checks. | Transcribed; regression checked. | Re-observe against a freshly started run after the reference display is relaunched. |
| Egg, expanded/alternate idle phase | A visual P1 phase was observed to use a different vertical extent, hence `P1SpriteFrame` supports up to 12 rows and its own origin. Its lifecycle position is not yet proven to be a hatch frame. | Observed only; not encoded as a new phase. | Restart → clock configuration → hatching trace and a stable three-frame capture. |
| Babytchi | No hand transcription is accepted yet. The existing drawing is a provisional placeholder. | Provisional. | Capture immediately after a confirmed hatch and record its idle cadence. |
| Marutchi | No selected-reference home sequence has been transcribed. | Provisional. | Observe after the Baby → Child transition. |
| Tamatchi / Kuchitamatchi | No selected-reference home sequence has been transcribed. | Provisional. | Observe one trace for each lineage. |
| Mametchi | Three independently drawn home phases are retained with a focused distinct-frame test. | Transcribed; regression checked. | Recheck the complete sequence at normal LCD scale. |
| Ginjirotchi / Maskutchi / Kuchipatchi / Nyorotchi / Tarakotchi / Bill | No selected-reference home sequence has been transcribed. | Provisional. | Capture the relevant qualified evolution path before replacing one form at a time. |

Reference-session note: TamaTool accepts the documented host controls when
focused under Xvfb (the shell-toggle control was visually confirmed), but an
Xvfb process is ephemeral between tooling sessions. Relaunch the emulator and
its virtual display before drawing conclusions from a missing later capture.
A clean bounded run was still visibly in egg idle animation after one speed
toggle over 24 seconds and after two toggles over eight seconds. This neither
invalidates the five-minute lifecycle rule nor identifies a Baby drawing: the
reference's exact accelerated timebase and reset/activation precondition must
be established before treating a later phase as a confirmed hatch.

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
