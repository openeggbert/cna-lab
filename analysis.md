# CNA Tamagotchi — International P1 Analysis and Technical Proposal

## Product decision

`cna-tamagotchi` is an independent C++ fan reimplementation of the
**international 1997 Tamagotchi Generation 1 programme** (P1). It is no longer
an original virtual-pet game and must not be a P1/P2 hybrid. P2 characters,
food, Number game, wavy LCD background, and UFO ending are outside scope.

The target is the English-language 1997 international device, not the original
Japanese 1996 device and not a modern `Original Tamagotchi` reissue. That
distinction is essential: regional and reissue versions change visible food,
hidden characters, end screens, and some rules.

The project is unofficial and unaffiliated with Bandai. A faithful public
distribution that includes protected names, sprites, logos, or sound effects
needs permission from the relevant rightsholders.

## Reference specification

The implementation reference is the 1997 international P1 instruction manual,
supported by period P1 documentation and direct observation of the target
programme. Every rule implemented as “faithful” must be recorded in a
source-backed specification ledger and verified by a deterministic test trace.

| Area | International P1 target |
| --- | --- |
| Screen | 32 × 16 one-bit LCD, checkerboard background |
| Controls | A selects; B confirms; C cancels; B shows clock when Attention is off; A+C sets the clock and starts a new egg from the end screen |
| Icons | Food, Light, Game, Medicine; Toilet, Health Meter, Discipline, Attention |
| Food | Bread meal; wrapped-candy snack |
| Game | Five-round Character game: predict left with A or right with B |
| Growth roster | Babytchi, Marutchi, Tamatchi, Kuchitamatchi, six standard adults, hidden Bill |
| End screen | Angel among stars |
| Reset | Rear reset switch; fresh pulsating egg, clock setup, then hatch after about five minutes |

The official P1 manual documents the controls, eight care functions, game,
medicine, and discipline behaviour. [P1 instruction manual](https://www.bandai.com/amfile/file/download/file/3639/product/1309818/)
Bandai identifies Generation 1 and Generation 2 as different characters and
slightly different play patterns even though their core care loop is shared.
[Official comparison](https://tamagotchi-official.com/us/news/01_166/)

## Required fidelity

The final programme must reproduce these player-visible properties:

- the international P1 roster and its one fixed evolution tree: Babytchi →
  Marutchi → Tamatchi or Kuchitamatchi → Mametchi, Ginjirotchi, Maskutchi,
  Kuchipatchi, Nyorotchi, Tarakotchi, or hidden Bill;
- actual one-bit P1 character sprites and their state animations, not
  substitute animal shapes;
- P1 food symbols and menus, P1 checkerboard screen treatment, P1 icon order,
  P1 Character game, and P1 angel end screen;
- real-time progression while the app is closed, including stage changes,
  sleep/wake times, hunger and happiness loss, waste, illness, calls,
  discipline, care mistakes, age, weight, and life span;
- the P1’s discrete four-heart hunger, happiness, and discipline display,
  rather than hidden `0…100` RPG-like values;
- exact action outcomes, including refusal, attention calls, repeated medicine
  where required, game-result effects, and the combination-button flows.

The desktop app may add only non-fictional host safeguards: nearest-neighbour
scaling, optional one-bit palettes, platform save paths, a reset hold plus
confirmation to prevent accidental loss, corrupt-save recovery, and backup
archives. They must not change simulated P1 state or appear as in-device P1
features.

## Rule-capture method

Available web guides are useful pointers but not a sufficient authority for
every timer or evolution condition. Before each rules subsystem is called
complete, capture it in `docs/p1-specification.md` with:

1. target variant and source or observation;
2. inputs and elapsed real time;
3. expected LCD state, sounds, care-mistake count, and persistent state;
4. an automated domain test that replays the same trace.

This prevents accidental adoption of Japanese P1, P2, or modern-reissue
behaviour. It also makes disputed details explicit rather than hiding guesses
inside simulation constants.

## Domain model

The current prototype’s `PetSpecies`, two original creature lines, random
generation seed, Elder stage, generic Farewell state, and `0…100` needs do not
belong in the P1 model. Replace them with a P1-specific domain:

```text
P1PetState
  form                 exact P1 current character
  stage                egg / baby / child / teen / adult / end
  hunger[0..4]         four visible hearts
  happiness[0..4]      four visible hearts
  discipline[0..4]     four visible bars
  age, weight          displayed P1 values and units
  clock                local device time and clock-setting state
  care events          timestamped P1 mistakes and discipline calls
  waste, illness, sleep, light, attention, game-round state
  UTC anchor           host persistence only
```

Evolution must use explicit, stage-bound P1 care-mistake and discipline
conditions. It must not derive an adult from a general “care quality” score,
random seed, custom affection, energy, hygiene, or a selectable species.

The event engine should jump between meaningful scheduled events for offline
catch-up. It must process events in chronological order, use a monotonic UTC
anchor to defend against clock rollback, and avoid writing saves unless state
changes. This preserves the requested SSD-friendly behaviour while retaining
real-time care.

## Rendering and input

The existing independent 32 × 16 display model is the correct seam to keep.
Replace its legacy symbols and original sprite catalogue with a P1 LCD asset
catalogue. The device shell must become a close desktop presentation of the
1997 international P1: compact egg proportions, three round buttons, a recessed
screen, checkerboard LCD field, permanent in-display icon bands, and a small
rear-reset analogue. Decorative background and palette selection remain outside
the logical LCD.

The logical button model remains A/B/C. Mouse, touch, keyboard, and controller
input all translate to those buttons; no direct menu buttons or extra care
actions may bypass the P1 state machine.

## Persistence migration

Existing saves describe a different game and cannot be truthfully converted
into P1 pets. A save-format migration must therefore detect a legacy slot,
preserve it as an archive, and offer a new P1 egg. New P1 saves need a target
identifier such as `international-p1-1997` so that a future incompatible mode
cannot silently load them with the wrong rules.

## Implementation order

1. Record this target in README, plan, and the end-user tutorial; add the
   source-backed P1 rule ledger.
2. Replace the legacy domain values and creature catalogue with the P1 roster,
   a dedicated P1 evolution resolver, and sprite tests.
3. Implement P1 menus, bread/candy feeding, Health Meter pages, attention and
   discipline semantics, clock flows, and the Character game.
4. Replace provisional timers with captured P1 schedules, care-mistake rules,
   illness, sleep, weight, life span, and angel ending; build golden trace
   tests for each.
5. Implement legacy-save archival/migration, reset-to-clock-setup, detailed
   player documentation, and final visual/audio comparison passes.

No P2 content will be retained after the migration except historical notes in
the development record.
