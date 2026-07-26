# International Tamagotchi P1 (1997) Specification Ledger

## Scope lock

This ledger applies only to the English-language international Generation 1
programme released in 1997. “P1” is fan shorthand; it must not silently mean a
Japanese 1996 unit, a Japanese P2 wave, an English P2, or a modern reissue.

P1 is represented as the first `ProgramDefinition` data package. The same
generic definition schema will later allow P2 data to reuse the engine without
mixing its content or rules into this P1 ledger.

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
| 32 × 16 one-bit LCD, two four-icon bands | Provisional | Historical P1 documentation and device observation; retain the existing logical resolution and move all icons inside it. |
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

## Timing and evolution capture table

| Rule | Status | Required trace before implementation is final |
| --- | --- | --- |
| New egg hatches after about five minutes | Verified | Reset/activation → clock setup → hatch. |
| Baby → child timing | Open | Fresh hatch with no interactions; record timestamp and LCD frames. |
| Child → teen timing and branch | Open | Controlled good- and poor-care runs with every care call recorded. |
| Teen → adult timing and matrix | Open | Controlled care-mistake/discipline runs for every adult and Bill. |
| Hunger/happiness decay | Open | Record every heart change by stage. |
| Attention window and care-mistake criteria | Open | Trigger hunger, happiness, light, and false calls separately. |
| Sleep/wake time per character | Open | Record exact transitions by current form. |
| Waste, sickness, medicine doses | Open | Record trigger, display, repeated dose, and recovery traces. |
| Weight changes, minimum weight, game effect | Open | Record Meal, Snack, game win, game loss, and refusal boundaries. |
| Adult life span and end sequence | Open | Record well-cared and neglected adult runs. |

## Explicit exclusions

- P2 characters, hamburger/cake food, Number game, wavy background, and UFO
  ending.
- Japanese P1 text/food/end-screen variants.
- Modern reissue snack penalties and quality-of-life additions.
- Legacy CNA Tamagotchi Pipple/Budbit forms, PEEK/NUM lines, random seed
  selection, Elder, and generic Farewell presentation.
