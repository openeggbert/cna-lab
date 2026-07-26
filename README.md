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

P1 is the only active programme. Its rules and content are kept in a
data-driven programme definition, so a future P2 implementation can supply
separate P2 data to the same engine rather than turning this build into a
hybrid or duplicating the simulator.

This project is a clean behavioural reimplementation. It does not embed, load,
execute, compare against, or require an original Tamagotchi ROM, TamaLIB, or
any other emulation core. Fidelity is established through documented rules,
reconstructed one-bit assets, deterministic traces, and independent tests.

## Player guide

The detailed English end-user tutorial is available as a standalone static site
in [web/index.html](web/index.html). It is being migrated alongside the game
from the earlier prototype to the selected 1997 international P1 reference.

## Current prototype

The current build opens a CNA window and renders an egg-shaped device with a
full 32 × 16 one-bit game field, connected top/bottom icon bands, and three
physical controls. Its active state is now the shared P1 programme state, not
the retired Pipple/Budbit prototype. The implemented P1 trace covers the egg,
Babytchi, Marutchi, early illness and two-dose medicine, baby waste, P1
Bread/Candy, the five-round Character game, weight effects, Toilet, and the
captured Marutchi sleep schedule. P1 JSON saves have an explicit programme id;
an incompatible prototype save is preserved under a `.legacy` suffix rather
than invented into a P1 pet. A persisted, data-driven P1 growth resolver now
covers the documented classic visible teen/adult branches in deterministic
tests, including the hidden teen-version and four-bar discipline matrix. Need
meters now use the captured per-character rates, pause while the pet sleeps,
and schedule the bounded false Discipline calls; the later Bill branch is
still pending.

Those systems are foundations, not a complete P1 implementation yet. The new
character catalogue uses provisional one-bit placeholders pending exact sprite
and animation capture. The implemented fifteen-minute Attention windows cover
initial/zero-heart, lights-off, and scheduled false-discipline calls; genuine
need timers pause during sleep. Later illness, the Bill branch, life span, and
the timing that reaches the already-rendered angel end screen still need
source-backed implementation. Legacy
prototype source files remain temporarily for isolated historical tests, but
they no longer drive the active application.

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

`cna-tamagotchi` therefore uses a permanent **32 × 16 logical game LCD**.
Every game pixel is either off (muted yellow-green) or on (near-black), with
integer nearest-neighbour scaling and no grey pixels or anti-aliasing. Four
fixed icon cells above it and four below it are a physically connected LCD
surround, not rows within the 32 × 16 game bitmap; together they make the
visible LCD module appear close to square.

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

The active P1 mapping includes the original clock behaviour: with no icon
selected and no Attention call, B opens the clock and B returns to the home
LCD. Press A+C while viewing that clock to enter clock setting; A advances the
hour, B advances the minute, and C confirms. On a clear home LCD, A+C toggles
the P1 sound setting (the audio output itself is still pending). A+C together
create a new egg after the end screen **without** resetting the established
clock. Desktop shortcuts mirror those button combinations but do not introduce
a fourth care action.

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

Weight is meaningful after the baby stage: meals add a little, snacks add
more, and completing the game helps reduce it. Babytchi remains at its fixed
five-ounce starting weight regardless of feeding or play.

## Mini-game

P1 has exactly one mini-game: **Character**. In each five-round game, A guesses
that the creature will look left and B guesses that it will look right. Three or
more correct guesses fill one happiness heart; completing the game also reduces
weight after the baby stage. The P2 Number game will be removed. [P1 instruction manual](https://www.bandai.com/amfile/file/download/file/3639/product/1309818/)

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
pressing A+C produces a new egg. The renderer and chord already use that P1
presentation; the natural life-span/death transition that reaches it remains
an open, source-backed task. Any desktop save archive is a host-side safety
mechanism, not a fictional in-device journal.

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
