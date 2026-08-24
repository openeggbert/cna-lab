# Explore2D architecture

## Design boundary

Explore2D treats an exploration adventure as three separable problems:

1. **World definition** — immutable descriptions of rooms, objects and rules.
2. **Session** — mutable player/world state and deterministic rule execution.
3. **Presentation/host** — draw the session and obtain input from CNA.

This makes the game rules portable and testable. CNA is not hidden behind a
fake generic platform abstraction; it is simply kept at the outer edge where it
belongs.

## Core data model

### `WorldDefinition`

Owns item definitions, rooms and interaction rules. IDs are strings on purpose:
for a story-heavy game, stable readable IDs are more useful than exposing
integer addresses inherited from a particular original implementation.

`validate()` checks cross-references such as the start room, exit destinations,
hotspot uniqueness, item references and interaction targets before a session is
allowed to start.

### `RoomDefinition`

A room is one fixed screen. It supplies:

- background colour;
- default player spawn;
- solids;
- decorative primitives;
- conditionally visible hotspots;
- hazards;
- directional exits;
- optional fast-travel anchor metadata.

The model does not require rooms to form a rectangular grid. Two adjacent
screens can connect to any destinations and can use explicit destination spawn
points.

### `InteractionRule`

Interactions use a compact data-oriented rule system:

```text
verb + target + optional inventory item
    + conditions
    -> messages
    -> mutations
```

Rules can have priorities and once-only flags. This permits progressive dialogue
and state-sensitive descriptions without hard-coding individual room classes.

Conditions currently cover flags, item possession, counters and room visitation.
Mutations cover flags, inventory, counters, travel unlocks, room moves, death and
victory.

## `AdventureSession`

The session owns all mutable state: player pose, inventory, flags, counters,
visited rooms, unlocked fast-travel anchors, the selected verb, UI choice state,
and message queues.

A `SessionSnapshot` is deliberately presentation-independent. Saving the game is
therefore a serialization problem, not a GPU/game-loop problem.

### Action target resolution

Dense adventure scenes often contain overlapping logical zones: an item may sit
on a desk, a key may lie inside a drawer region, or a lever may overlap a larger
machine hotspot. Explore2D first prefers a nearby hotspot that has an
applicable rule for the requested verb and only then falls back to the nearest
visible hotspot. For contextual ENTER, it uses only a real contextual target;
otherwise ENTER remains a jump.

This prevents decorative geometry from accidentally swallowing TAKE/USE/context
actions.

## Renderer

`AdventureRenderer` draws into `Canvas`, a CPU-owned RGBA buffer. The 0.1
renderer intentionally uses only simple procedural primitives and a small built-
in 5x7 bitmap font. A game can therefore boot with no asset pipeline.

The CNA adapter owns one `Texture2D`, uploads the logical framebuffer through
`SetDataRGBA`, and draws it using `SpriteBatch` with `PointClamp` and opaque
blending. The logical image is integer-scaled and centred in the current back
buffer.

This is intentionally simple. A future richer renderer can coexist with the
same world/session model.

## Persistence

The save format begins with `EXPLORE2D_SAVE 1` and stores the session snapshot
in a human-readable text form. `loadSnapshot()` validates syntax; the session's
`restore()` separately checks that referenced rooms/items exist in the current
world.

For a shipped game, add a game/content version to the save metadata before the
world begins making breaking ID changes.
