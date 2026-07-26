# CNA Tamagotchi

`cna-tamagotchi` is a C++ fan reimplementation built with
[CNA](https://github.com/openeggbert/cna). Its single historical target is the
**international 1997 Tamagotchi Generation 1 programme** (usually called
**P1**): one creature, three buttons, a 32 × 16 monochrome LCD, short
interactions, and consequences that unfold while the application is closed.

This is an independent, unofficial preservation project. It is not made by,
endorsed by, or affiliated with Bandai. P1 names, character designs, game
rules, and branding belong to their respective rightsholders; public release
of a faithful clone requires the appropriate permission.

## Player guide

The detailed English end-user tutorial is available as a standalone static site
in [web/index.html](web/index.html). It is being migrated alongside the game
from the earlier prototype to the selected 1997 international P1 reference.

## Current prototype

The current build already opens a CNA window and renders an egg-shaped device
with a 32 × 16 one-bit LCD, eight in-LCD icon-band pictograms, three primary
physical controls, an early deterministic pet simulator, and a tested JSON
save repository. The current
care slice renders four hunger, happiness, and discipline segments; schedules
sleep; handles waste; requests attention for empty needs, light, and false
calls; preserves those states in saves; and provides Food and Status LCD
screens rather than adding modern external controls. The Game icon still opens
legacy prototype mini-games. A win, not merely opening the icon, improves
happiness. Uncleaned waste and
excessive snacks can cause illness;
running health down to zero enters an original Farewell display state. Waste,
illness, and attention have their own compact LCD/shell indicators.
Successful care briefly draws a spark beside the creature; an unavailable care
action draws a small cross without writing an unchanged save slot.

Those systems are foundations, not a P1 implementation yet. In particular, the
current original creature catalogue, `0…100` need values, second game,
unfinished status/menu screens, temporary one-minute development clock, and
simplified illness/farewell rules will be replaced by the P1 model below.

## Reference target and display decision

The project will reproduce the **international P1 programme released in 1997**
as closely as practical. It will not combine P1 and P2 rules, characters, food,
games, or end screens, and it will not use later Connection, colour, online, or
2017-reissue rule changes. The reference hardware has a **32 × 16 pixel**
monochrome LCD, two icon bands, and three buttons. The contemporary P1 manual
documents the controls and care functions. [P1 instruction manual](https://www.bandai.com/amfile/file/download/file/3639/product/1309818/)

The app will retain only clearly documented desktop safeguards where physical
hardware cannot be reproduced safely. In particular, a held virtual reset
pinhole may require confirmation before it performs the original reset result:
a fresh pulsating egg followed by clock setup and hatching.

`cna-tamagotchi` therefore uses a permanent **32 × 16 logical LCD**. Every LCD
pixel is either off (muted yellow-green) or on (near-black), with integer
nearest-neighbour scaling and no grey pixels or anti-aliasing. The extra
physical space around it belongs to the shell, icon bands, and controls; it is
not additional display resolution.

The default is an olive LCD. Optional one-bit palettes are presentation-only
desktop accessibility settings; they cannot alter P1 rules or LCD resolution.

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

The final P1 mapping includes the original clock behaviour: B shows the clock
when Attention is not lit, and A+C enters clock setting. A and C together
create a new egg after the end screen. Desktop shortcuts may mirror those
button combinations but must not introduce a fourth care action.

The recessed pinhole at the lower-right of the shell simulates the original
rear reset switch without making it an accidental fourth care control. Hold it
with the mouse, or hold `R`, for about 1.5 seconds; the desktop guard then
requires B to confirm or C to cancel. A confirmed reset archives the active
slot as `.reset`, enters P1 clock setup, and produces a fresh pulsating egg.

## LCD icon bands and menus

Eight P1 pictograms sit *inside* the LCD in two permanent bands: four above
the creature and four below it. Their placement, selection sequence, status
pages, and care outcome will follow the international P1 programme.

| Top band | Function | Behaviour |
| --- | --- | --- |
| Food | Meal / snack | P1 bread restores hunger; candy restores happiness and adds more weight. |
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

## P1 life cycle

A new save begins with a pulsating egg. After clock setup it hatches after
about five minutes. The P1 path is Babytchi → Marutchi → Tamatchi or
Kuchitamatchi → a care-determined adult. The event scheduler will model the
historical stage timings, sleep schedules, weights, care-mistake windows, and
adult lifespans from the chosen international P1 reference.

Classic mode must continue while the program is closed. Saving stores UTC
time and processes the missed scheduled events when loading. Moving the system
clock backward must not create free care time. The present twelve-hour
simulation clamp is a development safeguard and will not limit P1 simulation;
the event engine will jump efficiently between meaningful events
instead of iterating through every minute.

There is no alternative gameplay mode in the P1 core. Accessibility features
must leave the simulated programme intact.

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

## Mini-game

P1 has exactly one mini-game: **Character**. In each five-round game, A guesses
that the creature will look left and B guesses that it will look right. Three or
more correct guesses fill one happiness heart; completing the game also reduces
weight when the creature is above its stage minimum. The P2 Number game will be
removed. [P1 instruction manual](https://www.bandai.com/amfile/file/download/file/3639/product/1309818/)

## Characters and evolution

The P1 roster is fixed. It has Babytchi, Marutchi, Tamatchi, Kuchitamatchi, the
six regular adults Mametchi, Ginjirotchi, Maskutchi, Kuchipatchi, Nyorotchi,
and Tarakotchi, plus the international hidden character Bill. The exact P1
care-mistake and discipline matrix—not a random seed or a player choice—will
select the branch. The original Generation 1/2 FAQ likewise describes a single
active character and an eleven-character total when baby, child, and hidden
characters are included. [Official FAQ](https://tamagotchi-official.com/us/series/original/faq/)

## End of life and a new egg

International P1 ends with its angel-and-stars screen. At the end screen,
pressing A+C produces a new egg. The current generic Farewell screen and
button-B restart will be removed. Any desktop save archive is a host-side
safety mechanism, not a fictional in-device journal.

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
care time. New slots use the standard per-user data directory (`LOCALAPPDATA`
on Windows, `~/Library/Application Support` on macOS, or `XDG_DATA_HOME` /
`~/.local/share` on Linux). An existing relative `saves/slot-1.json` or its
backup always takes precedence, so updates do not relocate an active pet. If
the active save cannot be read, the LCD offers `REST` when its backup is valid
or `NEW` to begin again; `NEW` first renames the damaged file to
`slot-1.json.corrupt` (with a numeric suffix when needed). Slot selection and
schema migrations remain to be added.

## Implementation order

1. Capture each P1 rule in the source-backed ledger before calling it faithful.
2. Replace legacy prototype state and creature lines with the P1 roster and
   four-heart simulation.
3. Implement the P1 menus, clock, reset outcome, Character game, evolution,
   timers, care mistakes, illness, and angel end sequence.
4. Archive legacy hybrid saves instead of converting them into fictional P1
   pets, then rewrite the detailed English player tutorial.

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
cmake --build --preset sdl-renderer --parallel 3
./cmake-build-sdl-renderer/CnaTamagotchi
```

Pass `--smoke-test` to exit after three rendered frames.

## Tests

```bash
cmake --build --preset sdl-renderer --parallel 3 --target CnaTamagotchiDomainTests CnaTamagotchiCreatureCatalogTests CnaTamagotchiDisplayTests CnaTamagotchiSaveLocationTests CnaTamagotchiPersistenceTests
ctest --test-dir cmake-build-sdl-renderer --output-on-failure
```

## Repository layout

```text
include/CnaTamagotchi/  application, display, domain, and persistence headers
src/                    executable entry point and implementations
tests/                  framework-free domain and persistence tests
analysis.md             technical analysis
plan.md                 staged implementation plan
docs/p1-specification.md source-backed P1 rule ledger
```

No licence file is included yet; it will be added separately.
