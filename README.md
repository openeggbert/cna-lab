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
- A distinct left/right-facing crouch-and-reach pose while TAKE is performed.
- Declarative solids and hazardous regions.
- Hotspots for scenery, items, characters, mechanisms and hazards.
- Three explicit verbs: **USE / EXAMINE / TAKE**.
- A separate contextual action used by ENTER; if no contextual interaction is
  available, ENTER becomes jump.
- Inventory items with descriptions and a `usable` property.
- Rule-driven interactions with conditions, priorities and once-only rules.
- Persistent flags, counters, visited rooms and inventory mutations.
- Multi-message dialogue / inspection sequences in compact blue speech bubbles,
  anchored independently to the player or the target character for each line.
- Hidden or conditional hotspots, allowing examination to reveal later items.
- Fast travel to visited travel-anchor rooms.
- Death and win states.
- Versioned text save/load snapshots.
- A configurable title/menu screen shown before a game session starts.
- Optional looping scene animations and one-shot action animations, both made
  from ordinary palette visuals with QBasic timer-tick frame durations.
- QBasic-style procedural drawing through `PSET`, rectangles, lines, arcs,
  circles/ellipses, polylines, filled polygons, boundary `PAINT`, bitmap-font
  text and palette-indexed `GET`/`PUT` equivalents.
- Both declarative `Visual` values and a fluent C++ `Drawing` builder for
  reusable code-drawn objects and scenes.
- Monophonic square-wave sound effects described as QBasic-compatible
  frequency/duration steps and played through CNA's audio API.
- Palette-indexed game definitions: arbitrary RGB values cannot leak into room
  art, title art, or the shared interface.
- A CPU RGBA framebuffer renderer. The CNA host uploads one texture and presents
  it with point sampling, avoiding any mandatory content pipeline or font asset.
- Headless tests for world rules and persistence.

An English presentation and practical usage guide is available in the
[web directory](web/README.md). It is a dependency-free static site and includes
real Black Pine screenshots, integration examples and an API tour.

## Project layout

```text
include/explore2d/
  Types.hpp         basic types, conditions, mutations, visuals
  World.hpp         static game/world definitions
  Session.hpp       live gameplay state and interaction engine
  Canvas.hpp        CPU RGBA canvas + tiny bitmap font
  Drawing.hpp       fluent procedural scene-art builder
  QBasicSound.hpp   PC-speaker-like tone definitions and synthesis
  Renderer.hpp      generic world/HUD renderer
  Persistence.hpp   save/load snapshots
  CnaGame.hpp       optional CNA Game host
src/
test/
docs/
web/                 English presentation and usage guide
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
| F11 | toggle window / fullscreen |
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

Animations are optional room overlays. Static artwork remains static unless a
game explicitly adds an animation:

```cpp
foyer.animations.push_back({
    "lamp_blink", true, true, {explore2d::Condition::flag("power_on")}, {
        {8, {explore2d::CircleVisual{{120, 80}, 4,
            explore2d::PaletteColor::brightYellow, true}}},
        {8, {explore2d::CircleVisual{{120, 80}, 4,
            explore2d::PaletteColor::brown, true}}},
    }});
```

One-shot animations are started by
`Mutation::playAnimation("animation_id")`. Simple effects use `ToneStep`
values equivalent to QBasic `SOUND frequency, duration`; duration is measured
in the historical 18.2 Hz timer ticks and frequency `0` is a rest.

See **Black Pine** for a complete game using conditional discovery,
dialogue, inventory use, consumed items, fast-travel anchors, contextual room
transitions, hazards and a win state.

## Scope

Explore2D 0.1 is intentionally an adventure-game foundation, not a general
sprite/physics engine. It provides small scene/action animations and historical
tone effects, but not a general skeletal animation system, tracker/music player,
sampled-asset pipeline, editor, localization database, arbitrary scripted
minigame plug-ins, or serialized external world format. Those can be added
without changing the core room/rule/state model.

## License

Explore2D is available under the [MIT License](LICENSE).
