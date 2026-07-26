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
| B shows the clock when Attention is not lit | Verified | Official Original FAQ; the P1 UI must not use B only as a generic menu key. |
| A+C enters clock setting | Verified | Official Original FAQ. A changes hours, B changes minutes, C confirms. |
| A+C starts a new egg at the end screen | Verified | Official Original FAQ. |
| Rear reset produces a pulsating egg and requires clock setup | Verified | P1 manual. Desktop hold/confirmation is a host safety guard before this result. |

## Care functions

| Function | Status | Target behaviour |
| --- | --- | --- |
| Food | Verified | Select Meal or Snack. A full pet refuses food. P1 international presentation is provisionally bread for Meal and wrapped candy for Snack. |
| Light | Verified | Select On/Off. Turn light off when the character sleeps; it turns on automatically at wake-up. |
| Toilet | Verified | Flush droppings; leaving them causes illness. |
| Health Meter | Verified | Shows age/weight, discipline, hunger, and happiness pages. |
| Character game | Verified | Five turns. A chooses left, B chooses right; matching the direction raises happiness. |
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

The roster is corroborated by historical character references; exact pixel
sprites, animation frames, and every branch condition remain open until they
are captured from the selected target programme.

## Evolution-rule representation

The shared engine represents a growth chart as ordered `EvolutionRule` data:
source character, target character, care-mistake range, discipline-mistake
range, and optional A/B teen lineage. P1 supplies rules for Marutchi →
Tamatchi/Kuchitamatchi and the visible adult outcomes. The simulator has
deterministic traces for the documented Mametchi, Ginjirotchi, Maskutchi,
Kuchipatchi, Nyorotchi, and Tarakotchi branches.

This is **provisional** source-backed behaviour, not yet a final fidelity
claim. The current live Attention/care-mistake producer is incomplete, so the
persisted counters and resolver are tested independently before every P1 call
condition is connected. The later international Maskutchi → Bill special
branch remains open until its timing and zero-mistake condition are captured.

## Timing and evolution capture table

| Rule | Status | Required trace before implementation is final |
| --- | --- | --- |
| New egg hatches after about five minutes | Verified | Reset/activation → clock setup → hatch. |
| Baby → child timing | Verified | Babytchi evolves to Marutchi after 65 minutes. |
| Child → teen timing and branch | Verified in part | At age 3, 0–1 care mistakes yields Tamatchi; 2+ yields Kuchitamatchi. Capture exact version/discipline carry state. |
| Teen → adult timing and matrix | Verified in part | At age 6, the form is selected by teen type, care mistakes, and 25% discipline bands. Capture each branch in tests. |
| Hunger/happiness decay | Open | Record every heart change by stage. |
| Attention window and care-mistake criteria | Open | Trigger hunger, happiness, light, and false calls separately. |
| Sleep/wake time per character | Open | Record exact transitions by current form. |
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
- Adult minimum weight / sleep pairs are: Mametchi and Ginjirotchi 30 oz,
  22:00–09:00; Maskutchi 30 oz, 23:00–11:00; Kuchipatchi 20 oz,
  22:00–09:00; Nyorotchi 10 oz, 22:00–09:00; Tarakotchi 20 oz,
  22:00–10:00; Bill 30 oz, 22:00–09:00.
- The Health Meter has four pages: age/weight (ounces in international P1),
  Discipline, Hungry, and Happy. Both heart meters range from 0 to 4;
  discipline moves in 25% increments.

These values were captured from [Thaao's original-P1 care guide](https://thaao.net/tama/p1/)
and its [P1 character guide](https://thaao.net/tama/p1/?p=chara); the official
manual remains the primary source for visible controls and care functions.
The provisional A/B lineage and adult threshold table is additionally recorded
by a [community P1 chart](https://tamagotchi.fandom.com/wiki/User_blog%3AThePeejdom/A_100%25_accurate_P1/P2_chart);
it must be checked against target traces before release.

## Explicit exclusions

- P2 characters, hamburger/cake food, Number game, wavy background, and UFO
  ending.
- Japanese P1 text/food/end-screen variants.
- Modern reissue snack penalties and quality-of-life additions.
- Legacy CNA Tamagotchi Pipple/Budbit forms, PEEK/NUM lines, random seed
  selection, Elder, and generic Farewell presentation.
