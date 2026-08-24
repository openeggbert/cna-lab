# Explore2D

Explore2D is a small, deliberately opinionated C++23 2D exploration/adventure
engine built to sit above
[CNA](https://github.com/openeggbert/cna). It is intended for games made from
fixed, connected screens: walk into a scene, inspect it, collect objects, use
inventory items on mechanisms, talk to characters, unlock routes, survive
hazards, and gradually change the persistent state of the world.

The project is deliberately split into a **CNA-independent gameplay core** and
an optional **CNA host**. Game rules can therefore be unit-tested without a
window or GPU while CNA remains responsible for input, the game loop, and final
presentation.

The API and visual constraints are inspired by design lessons extracted from *Tajná mise*,
but Explore2D is not a port or compatibility layer and contains no original
Tajná mise story, room, text, or art assets.

Explore2D games intentionally share a recognisable presentation: a fixed
640×350 logical screen, the exact 16-colour EGA palette of QBasic `SCREEN 9`, a
492×262 non-scrolling scene, text inventory on the right, and action/status
strips below. This is a defining feature rather than a temporary limitation.

## What the engine already provides

- Fixed, non-scrolling rooms connected by left/right/up/down exits.
- Per-room spawn points and conditional exits with blocked-route messages.
- Player walking, optional turn-before-walk behaviour, jumping and gravity.
- Declarative solids and hazardous regions.
- Hotspots for scenery, items, characters, mechanisms and hazards.
- Three explicit verbs: **USE / EXAMINE / TAKE**.
- A separate contextual action used by ENTER; if no contextual interaction is
  available, ENTER becomes jump.
- Inventory items with descriptions and a `usable` property.
- Rule-driven interactions with conditions, priorities and once-only rules.
- Persistent flags, counters, visited rooms and inventory mutations.
- Multi-message dialogue / inspection sequences.
- Hidden or conditional hotspots, allowing examination to reveal later items.
- Fast travel to visited travel-anchor rooms.
- Death and win states.
- Versioned text save/load snapshots.
- A configurable title/menu screen shown before a game session starts.
- QBasic-style procedural drawing through `PSET`, rectangles, lines,
  circles/ellipses, boundary `PAINT`, and bitmap-font text equivalents.
- Palette-indexed game definitions: arbitrary RGB values cannot leak into room
  art, title art, or the shared interface.
- A CPU RGBA framebuffer renderer. The CNA host uploads one texture and presents
  it with point sampling, avoiding any mandatory content pipeline or font asset.
- Headless tests for world rules and persistence.

## Project layout

```text
include/explore2d/
  Types.hpp         basic types, conditions, mutations, visuals
  World.hpp         static game/world definitions
  Session.hpp       live gameplay state and interaction engine
  Canvas.hpp        CPU RGBA canvas + tiny bitmap font
  Renderer.hpp      generic world/HUD renderer
  Persistence.hpp   save/load snapshots
  CnaGame.hpp       optional CNA Game host
src/
test/
docs/
```

`Explore2D::Core` never includes CNA. `Explore2D::Cna` is created only when
a CNA target is available.

## Build the headless core

```bash
cmake -S . -B build -DEXPLORE2D_BUILD_CNA=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Build with CNA

Use a complete CNA checkout, including the third-party/submodule content required
by that CNA revision:

```bash
cmake -S . -B build \
  -DEXPLORE2D_BUILD_CNA=ON \
  -DCNA_SOURCE_DIR=/path/to/cna
cmake --build build -j
```

Explore2D asks embedded CNA for the SDL_Renderer backend and disables CNA's own
examples/tests/network option in this sub-build. If your CNA revision uses a
different renderer-selection contract, adjust the small CNA block in the top
level `CMakeLists.txt` rather than coupling the gameplay core to it.

## Host controls

The default CNA host intentionally preserves the compact keyboard vocabulary of
its design inspiration:

| Key | Action |
|---|---|
| Left / Right | turn or walk |
| Up | cycle USE → EXAMINE → TAKE |
| Down | perform selected verb |
| 1 / 2 / 3 | direct USE / EXAMINE / TAKE |
| Enter or Space | contextual action, otherwise jump |
| M | discovered-destination travel map |
| S / L | quick save / quick load |
| Q | quit |
| Up / Down + Enter | navigate the title menu, choices, or map |
| Escape | cancel a choice/message |

## Defining a game

A game normally owns one function that constructs a `WorldDefinition`. Rooms,
hotspots and interactions are ordinary C++ data:

```cpp
explore2d::WorldDefinition world;
world.title = "My Adventure";
world.startRoom = "foyer";
world.presentation.title.subtitle = "A SHORT EXPLORATION STORY";
world.presentation.title.artwork = {
    explore2d::RectVisual{{20, 80, 600, 140}, explore2d::PaletteColor::blue, true},
    explore2d::CircleVisual{{520, 110}, 20, explore2d::PaletteColor::brightYellow, true},
};

world.addItem({"key", "KEY", "A small brass key.", true});

explore2d::RoomDefinition foyer;
foyer.id = "foyer";
foyer.label = "FOYER";
foyer.defaultSpawn = {32, 232};
foyer.background = explore2d::PaletteColor::brightBlue;
foyer.solids.push_back({0, 260, 492, 28});
foyer.decorations.push_back(explore2d::LineVisual{
    {0, 190}, {240, 70}, explore2d::PaletteColor::white});
foyer.hotspots.push_back({
    "key_pickup", "KEY", {200, 200, 60, 60},
    explore2d::HotspotKind::item, {}, {}});
world.addRoom(std::move(foyer));

world.addInteraction({
    explore2d::Verb::take,
    "key_pickup",
    std::nullopt,
    {},
    {{"You take the key.", explore2d::MessageStyle::inspect}},
    {explore2d::Mutation::addItem("key")},
    0,
    "key_taken_once"});
```

See **Black Pine** for a complete game using conditional discovery,
dialogue, inventory use, consumed items, fast-travel anchors, contextual room
transitions, hazards and a win state.

## Scope

Explore2D 0.1 is intentionally an adventure-game foundation, not a general
sprite/physics engine. It does not yet provide an editor, animated sprite system,
audio layer, localization database, arbitrary scripted minigame plug-ins, or a
serialized external world format. Those can be added without changing the core
room/rule/state model.

## License

Explore2D is available under the [MIT License](LICENSE).
