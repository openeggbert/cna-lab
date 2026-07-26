# CNA Tamagotchi — Analysis and Technical Proposal

## Product intent

`cna-tamagotchi` will be an original virtual-pet game written in modern C++
and rendered with the sibling [CNA](../cna) framework. It should evoke the
small, tactile charm of a 1990s digital pet without reproducing Tamagotchi
characters, art, names, or product branding. The player raises an original
creature by attending to its needs over real elapsed time; care quality,
choices, and play determine its development.

The initial deliverable is intentionally only an application skeleton. It
must make later gameplay, rendering, saving, and testing independent rather
than placing all game logic in an XNA-style `Game` subclass.

## Reference-device findings

The original 1996/1997 Tamagotchi has a **32 × 16 pixel** monochrome LCD, two
rows of care icons, and three physical buttons. The game will use that same
32 × 16 logical resolution to preserve the compact visual constraint and
interaction rhythm of the reference device.

Each logical pixel
is either `off` (the LCD's pale green/yellow background) or `on` (near-black).
It will be scaled by an integer multiplier with nearest-neighbour sampling;
there is no anti-aliasing or intermediate grey. The display will be drawn
inside a larger, decorative device shell rather than being the full window.

Sources consulted:

- [Tamagotchi (1996 Pet) technical details](https://tamagotchi.fandom.com/wiki/Tamagotchi_(1996_Pet)) — 32 × 16 display and the icon-row layout.
- [Bandai Original Tamagotchi product description](https://www.bandai.com/original-tamagotchi-white-with-black) — feeding, cleaning, medicine, discipline, a character game, and care-driven adult outcomes.
- [Bandai UK Original Tamagotchi description](https://shop.bandai.co.uk/brand/bandai/collection/tamagotchi/tamagotchi-original-black-42963gtnp/) — hunger, happiness, discipline, illness, weight, sleep, and evolution as useful genre mechanics.

## Visual direction

The application window should feel soft and calm, not like a literal plastic
toy photographed against a background:

- A very light warm background transitions slowly (about 25–45 seconds) among
  ivory `#FFF9EE`, pale peach `#FFF0DF`, warm cream `#FFF6DF`, and misty
  apricot `#FDEBCE`. The phase is deterministic from time so it remains gentle
  rather than flashing randomly.
- The main device is a tall egg-shaped ellipse with a small rim, subtle
  highlight, and an optional short keychain tab. Initial shell palettes:
  `Sakura`, `Sky`, `Mint`, `Lavender`, `Sunshine`, and `Monochrome`.
  Shell colour is purely cosmetic and selectable when a save is created.
- The recessed LCD has a muted yellow-green/off colour (for example
  `#B8C58A`) and dark on pixels (`#20271B`). A little screen grain is optional
  and must never obscure pixels.
- The UI should be original: a generic egg device and original creatures,
  not a copy of a named commercial shell, frame motif, or character.

## Proposed controls and screen layout

The device has three clickable/keyboard-mappable controls:

| Control | Keyboard | Role |
| --- | --- | --- |
| A / select | `A`, Right arrow; Left arrow for previous | Move the icon selection |
| B / confirm | Enter, Space, `B` | Open or confirm the selected action |
| C / back | `C`, Backspace | Cancel and return selection to the meal icon |

In the shell, not inside the 32 × 16 LCD, use eight small pictograms split in
two bands. The active selection is dark; inactive icons use the shell's
shadow colour. The first proposed menu set is:

| Top band | Purpose | Bottom band | Purpose |
| --- | --- | --- | --- |
| Bowl | Meal / snack | Toilet | Clean messes |
| Ball | Mini-game | Health cross | Diagnose / medicine |
| Moon | Light / sleep | Heart graph | Need and age status |
| Bell | Attention log | Star | Care journal / memories |

Discipline is an in-screen action on the attention log, avoiding a ninth
permanent icon. Settings (sound, shell, accessibility, export/import) belong
in the start screen and pause menu, not the toy face.

## Game-system proposal

### Pet and time model

A save contains one active pet and a chronological journal. The core will
advance from a persisted UTC timestamp to `now`, clamping a single offline
advance to a safe maximum and processing events in deterministic time steps.
This prevents clock changes from producing exploits or enormous simulation
loops. The pet is an explicit state machine:

`Egg → Hatchling → Child → Teen → Adult → Elder / Farewell`.

Needs are numeric values in the domain layer, normally `0…100`: hunger,
happiness, energy, hygiene, health, affection, and discipline. Scheduled
events create requests (meal, sleep, mess, attention); ignored requests add
care-mistake records. A data-driven evolution table maps species, care band,
and a deterministic saved seed to original adult forms. Early implementation
will use a small set of original species such as Puffin, Mossling, Pebblet,
and Cometling rather than any protected Tamagotchi roster.

### Actions and consequences

The first vertical slice should cover feeding, cleaning, sleep/light,
medicine, one short timing mini-game, status, and attention/discipline. Meals
reduce hunger, snacks improve happiness but have a modest weight/health
trade-off, play consumes energy but improves happiness, and neglected messes
or illness deteriorate health. Audio and vibration-like feedback are optional
later; the game must remain fully playable visually.

## Architecture

The dependency direction is deliberately one way:

```text
Application / CNA adapter
          │ input, frame clock, drawing
          ▼
Game session / use cases ───── Persistence adapter
          │                         │
          ▼                         ▼
Domain: PetState, clock, actions, evolution, events
          │
          ▼
MonochromeDisplay (32 × 16 pixels, render-model only)
```

- **`domain/`**: C++ value types and deterministic rules with no CNA headers.
  This is where automated tests will concentrate.
- **`display/`**: a tiny packed or byte-per-pixel 1-bit framebuffer with safe
  pixel access, clear, and future sprite/text blitting. It is also independent
  of CNA.
- **`application/`**: the `Game` subclass, input mapping, timer bridge,
  window/shell renderer, and later audio. It translates framework events into
  domain actions and projects state to the framebuffer.
- **`persistence/`**: versioned save DTO and atomic file replacement. The
  domain must never directly read a file.

The current prototype creates these boundaries and renders the original
egg-shaped device, its 32 × 16 LCD, static care icons, and a demo pixel
creature over the warm background. It does not pretend that gameplay is
already implemented.

## Save-data design

Use a human-readable, versioned JSON document in the per-user application-data
directory, for example `cna-tamagotchi/saves/slot-1.json`. Store:

- `formatVersion`, save UUID, UTC `lastSavedAt`, game seed, and selected shell;
- pet species, name, life stage, birth time, needs, weight, care mistakes,
  evolution flags, and pending event schedule;
- user settings and a bounded activity journal.

Write to `slot-1.json.tmp`, flush/close it, then atomically replace the real
file; retain one `.bak` file for recovery. Parsing validates ranges and schema
version before mutating a session. A future export/import feature will use the
same schema but will be explicit about replacing an existing slot. JSON is
well-suited while saves are small and makes debugging/modding practical;
version migrations keep old pets playable.

## Delivery sequence and risks

1. Establish the buildable CNA project and pure C++ seam classes (this
   skeleton).
2. Add domain simulation and tests before the visible pet.
3. Render the device, menu icons, and 32 × 16 1-bit framebuffer using original assets
   and pixel art.
4. Add input/menu actions, the first care loop, and a simple mini-game.
5. Add versioned persistence, offline catch-up, sound, accessibility, and
   content/evolution expansion.

Important risks: the CNA checkout is a sibling dependency, visual design must
avoid copying a commercial device or character artwork, and real-time games
need clear behaviour for sleep, timezone, clock rollback, and long offline
periods. Automated deterministic simulation tests and a separate render model
address the latter two risks.
