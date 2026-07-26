# CNA Tamagotchi

`cna-tamagotchi` is an original C++ virtual-pet game built with
[CNA](https://github.com/openeggbert/cna). It aims for the deliberately small,
real-time experience of the first 1996/1997 virtual-pet devices: one creature,
three buttons, a tiny monochrome display, short interactions, and consequences
that unfold while the application is closed.

The project takes inspiration from genre mechanics and hardware constraints.
It does **not** copy Tamagotchi characters, sprite art, logos, shell patterns,
sound effects, or names. Every creature, icon drawing, shell, and asset in this
project will be original.

## Current prototype

The current build already opens a CNA window and renders an original pastel
egg-shaped device with a 32 × 16 one-bit LCD, eight shell icons, three physical
controls, and two complete original creature lines. It also has an early
deterministic pet simulator and a tested JSON save repository. The current
care slice renders four hunger, happiness, and discipline segments; schedules
sleep; handles waste; requests attention for empty needs, light, and false
calls; preserves those states in saves; and provides Food and Status LCD
screens rather than adding modern external controls. The Game icon opens the
line's original two-choice mini-game: Peek for Pipple and Number for Budbit.
A win, not merely opening the icon, improves happiness. Uncleaned waste and
excessive snacks can cause illness;
running health down to zero enters an original Farewell display state. Waste,
illness, and attention have their own compact LCD/shell indicators.
Successful care briefly draws a spark beside the creature; an unavailable care
action draws a small cross without writing an unchanged save slot.

Those systems are foundations, not the final classic ruleset. In particular,
the internal `0…100` need values, unfinished status/menu screens, temporary
one-minute development clock, and simplified long-term illness/farewell rules
will be replaced by the faithful model below.

## Reference target and display decision

The reference target is the first-generation 1996/1997 device (commonly called
P1/P2 by fans), not a later connected, colour, or online Tamagotchi model.
The original has a **32 × 16 pixel** monochrome LCD, two icon bands, and three
buttons. [Technical reference](https://tamagotchi.fandom.com/wiki/Tamagotchi_(1996_Pet))

`cna-tamagotchi` therefore uses a permanent **32 × 16 logical LCD**. Every LCD
pixel is either off (muted yellow-green) or on (near-black), with integer
nearest-neighbour scaling and no grey pixels or anti-aliasing. The extra
physical space around it belongs to the shell, icon bands, and controls; it is
not additional display resolution.

## Controls

The finished device will have exactly three primary controls, like the
historical device:

| Control | Physical position | Target behaviour | Desktop equivalent |
| --- | --- | --- | --- |
| A | left | Move the selection forward through icons or menu choices | `A`, Right arrow |
| B | centre | Open, confirm, or perform the selected action | `B`, Enter, Space |
| C | right | Cancel, close a submenu, or return to the previous screen | `C`, Backspace, Escape |

Mouse and touch input now press these same three virtual buttons; their hit
areas remain correct when the game window is resized. Icons do not become
independent modern UI buttons: clicking an icon may be offered as an
accessibility shortcut later, but it must invoke the equivalent A/B/C sequence.

The current build uses C to close Food, Status, and Game screens; pressing B
on Food opens a Meal/Snack menu, and B on Status advances its three compact
data pages. Left is a temporary desktop convenience for moving backwards
through choices.

## Shell icons and menus

Eight original-drawn pictograms will sit outside the LCD in two bands. Their
function follows the early-device pattern while their artwork remains original.

| Top band | Function | Behaviour |
| --- | --- | --- |
| Food | Meal / snack | A meal restores hunger; a snack restores happiness but adds more weight and can contribute to illness when overused. |
| Light | On / off | The player turns the light off only after the creature sleeps; waking turns it back on. |
| Game | One short game | Raises happiness and helps control weight. |
| Medicine | Treat illness | Works only for an ill creature. |

| Bottom band | Function | Behaviour |
| --- | --- | --- |
| Clean | Remove waste | Removes droppings; leaving them too long can cause illness. |
| Status | Inspect data | Cycles age/weight, discipline, hunger hearts, and happiness hearts. |
| Discipline | Respond to false calls | Used only when the creature calls despite having no genuine need. |
| Attention | Indicator only | Lights and beeps automatically; it is not a selectable action. |

This is the same functional family described for the original device: food,
light, game, medicine, cleaning, health/status, discipline, and attention.
[Original function overview](https://tamaconnection.neocities.org/vintage/p1p2)

## Classic life cycle

A new save begins with an egg. The egg hatches after about five minutes. The
creature then progresses through baby, child, teen, and adult forms in real
time. The target cadence is the classic one: baby to child after roughly
65 minutes, teen around age three days, and adult around age six days; each
real day counts approximately as one year of the creature's age.
[Classic growth summary](https://tamagotchi.fandom.com/wiki/Tamagotchi_(1996_Pet))

Classic mode must continue while the program is closed. Saving stores UTC
time and processes the missed scheduled events when loading. Moving the system
clock backward must not create free care time. The present twelve-hour
simulation clamp is a development safeguard and will not limit Authentic
Classic mode; the event engine will jump efficiently between meaningful events
instead of iterating through every minute.

An optional Relaxed mode will be added later for accessibility. It may pause
time and disable permanent loss, but it is not the default and does not change
the Classic rules.

## Needs, calls, mistakes, and illness

The final visible state is intentionally small:

- **Four hunger hearts** and **four happiness hearts**, rather than a large RPG stat sheet.
- **Four discipline segments**.
- **Age** and **weight** on the status screen.
- **Sleep state**, illness, waste, and an active attention call.

A call for attention begins when hunger or happiness becomes empty, when a
sleeping creature needs the light off, or when it makes a deliberate false call
for discipline. The player normally has fifteen minutes to act. Ignoring a
genuine call creates a care mistake; discipline calls have their own rule path.
The historical attention timer and its care-mistake consequence are central to
the original experience. [Care reference](https://tamagotchi.fandom.com/wiki/Care)

Waste does not itself trigger attention. It remains visible on the LCD; leaving
it too long can make the creature ill. Illness is also possible after excessive
snacks or severe neglect. Medicine only solves illness, not hunger, sadness,
or a false discipline call.

Weight is meaningful: meals add a little, snacks add more, and a successful
game helps reduce it. This makes the player choose between a quick snack and
playing the small game.

## Mini-games

The initial authentic programme will have two selectable game profiles:

- **Peek game** — the player predicts whether the creature will look left or right.
- **Number game** — the player predicts whether the next number is higher or lower.

Only one profile is active for a given egg line, preserving the compact feel of
the original P1/P2 devices: Pipple uses Peek and Budbit uses Number. The rules
and artwork are original, while the two-choice, seconds-long interaction remains
intentionally simple.

## Creatures and evolution

The game will contain multiple original creature forms, but it will not let the
player choose an already-grown adult from a menu. Care and a stored generation
seed determine the outcome, which is the important original-game idea.

Two complete egg lines are implemented. Each line has:

- one egg, one baby form, and one child form;
- two teen forms;
- six adult forms with distinct animal-like silhouettes (for example bird-like,
  amphibian-like, hedgehog-like, lizard-like, aquatic, and nocturnal);
- one rare late or special form.

This gives 22 living forms, two care-driven rare forms, and a shared egg LCD
state without turning the device into a creature-collection game. The first
generation begins on the Pipple line; each later new egg deterministically
selects the Pipple or Budbit line from the saved generation seed. Further egg
lines can be added only after each has a complete growth tree, animation set,
rules table, and tests.

The current resolver uses care mistakes, discipline, weight, illness, and age
to select the teen and adult form. Snack history, game performance, stage-
specific care mistakes, and a saved generation seed will be added as the
classic care-event system replaces the temporary `0…100` simulator. Better care
makes the healthier adult outcomes more likely, but no adult form is simply a
reward for maximising one number.

## Death, farewell, and a new egg

Death or farewell belongs in Classic mode. It will be presented quietly and
respectfully. The current Farewell display reads `NEW`; B starts a new,
freshly-seeded egg. A chronological journal and a successful-adult memory/egg
event remain future work. Relaxed mode will make permanent loss optional, but
Classic mode will preserve stakes.

## Technical architecture

```text
CNA application / input / renderer
             |
             v
session controller and event scheduler <---- JSON save repository
             |
             v
pure domain: hearts, care events, evolution, life stages, rules
             |
             v
32 × 16 monochrome display model
```

The domain remains free of CNA headers. The renderer only projects domain state
onto the LCD. This lets the rule engine be unit-tested without a graphics
window and keeps persistence from directly mutating gameplay after invalid
data is read.

Current persistence writes a versioned JSON document through a `.tmp` file and
keeps a `.bak` copy before replacing an existing save. The application uses
`saves/slot-1.json` relative to its working directory, loads it at startup,
performs bounded UTC catch-up, and writes only after a changed care state (or
once for a new slot), rather than rewriting unchanged data on every launch.
Its UTC simulation anchor retains sub-minute time and never moves backwards,
so repeated quick restarts or rolling the system clock back cannot grant extra
care time. Platform-specific user-data paths, slot selection, schema migrations,
and recovery UI remain to be added.

## Implementation order

1. Complete the conversion from the prototype's generic 0–100 values to
   authentic four-heart needs and discipline segments, including illness rules.
2. Add status and food submenus, then make A/B/C navigation match the physical
   device in every screen.
3. Implement the egg, hatch, stage schedule, complete sleep schedules, care mistakes,
   illness, death/farewell, and event-jump offline processing.
4. Add feedback animations to the implemented creature lines.
5. Add selectable user-data save slots plus migration/recovery UI and tests.
6. Add sounds, visual accessibility options, further complete egg lines,
   and a Relaxed mode without weakening the default Classic mode.

Each completed milestone is built, tested, and committed locally. No remote
push is performed until a remote exists and the user explicitly requests it.

## Build and run

### Prerequisites

- CMake 3.21 or newer
- A C++23 compiler
- A sibling CNA checkout at `../cna`, or an explicit `CNA_ROOT_DIR`
- CNA's own sibling dependencies, as documented by the CNA project

```bash
cmake --preset sdl-renderer
cmake --build --preset sdl-renderer
./cmake-build-sdl-renderer/CnaTamagotchi
```

Pass `--smoke-test` to exit after three rendered frames.

## Tests

```bash
cmake --build --preset sdl-renderer --target CnaTamagotchiDomainTests CnaTamagotchiCreatureCatalogTests CnaTamagotchiPersistenceTests
ctest --test-dir cmake-build-sdl-renderer --output-on-failure
```

## Repository layout

```text
include/CnaTamagotchi/  application, display, domain, and persistence headers
src/                    executable entry point and implementations
tests/                  framework-free domain and persistence tests
analysis.md             technical analysis
plan.md                 staged implementation plan
```

No licence file is included yet; it will be added separately.
