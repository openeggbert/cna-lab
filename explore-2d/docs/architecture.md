# Explore2D architecture

## Design boundary

Explore2D treats an exploration adventure as three separable problems:

1. **World definition** — immutable descriptions of rooms, objects and rules.
2. **Session** — mutable player/world state and deterministic rule execution.
3. **Presentation/host** — enforce the Explore2D visual language, draw the
   session, and obtain input from CNA.

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
Mutations cover flags, inventory, counters, travel unlocks, room moves, optional
one-shot animation cues, death and victory. Rules can also name a tone effect.

### `HintDefinition`

Hints reuse the same condition vocabulary as interaction rules. The session
selects the highest-priority matching hint, with declaration order breaking
ties. Hint lookup is read-only: opening F1 help never applies a mutation,
advances dialogue, or changes a snapshot. Games can therefore author guidance
against their real puzzle flags instead of maintaining a parallel objective
tracker.

## `AdventureSession`

The session owns all mutable state: player pose, inventory, flags, counters,
visited rooms, unlocked fast-travel anchors, the selected verb, UI choice state,
message queues, active one-shot animations, scene time and pending audio cue IDs.
The selected language is a presentation preference held by the session but is
not part of `SessionSnapshot`, so switching languages never forks game state.

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

`AdventureRenderer` always draws a 640×350 logical image. Its layout is part of
the engine contract: the 492×262 room view sits at `(8,8)`, a right-hand panel
contains the game mark and text inventory, and bordered strips below contain
the location and the three verbs. Messages use compact blue bordered speech
bubbles near the player or target speaker; inventory choices remain in the side
panel; the travel map replaces the room view.

`Canvas` is CPU-owned and palette indexed. It retains RGBA bytes only for the
final CNA upload. Public drawing methods accept one of the 16 exact EGA colours,
not arbitrary RGB values. It offers pixel plotting, filled or outlined
rectangles, lines, arcs, filled or outlined circles and ellipses, polylines,
filled polygons, flood filling, palette-indexed image capture/blitting with
QBasic-style raster operations, and a built-in 5×7 bitmap font. `Drawing`
provides an origin-aware fluent C++ builder over the same `Visual` values. These
are direct modern equivalents of the small QBasic drawing vocabulary used by
the design reference.

Room animation is deliberately an overlay rather than a demand that every
object become a sprite. A `SceneAnimationDefinition` may autoplay or wait for a
`playAnimation` mutation, may loop or run once, and may have ordinary world-state
conditions. Frame lengths use QBasic's 18.2 Hz timer tick. Each frame is simply
a list of the same procedural `Visual` values used by static art.

`WorldDefinition::presentation` lets a game configure title text, menu wording,
title colour cycling, byline and code-drawn title artwork. The title layout and
four-entry NEW GAME / LOAD / SETTINGS / QUIT flow remain engine-owned, so games can have
their own identity without losing the Explore2D identity.

## Localization

Every user-facing world value is `LocalizedText`: an English-compatible
fallback plus translations indexed by stable language ID. This covers title,
interface, rooms, travel anchors, hotspots, inventory, procedural `TextVisual`
labels, messages, exit warnings, hazards, and terminal outcomes. Games declare
the languages available in `WorldDefinition::localization`; unsupported or
missing translations resolve to the fallback rather than an internal key.

`AdventureSession::setLanguage()` changes resolution at runtime. Choice lists
are rebuilt from localized labels when opened, while active messages and stable
world data resolve during rendering. The CNA shell exposes Settings from both
the title screen and the Escape pause menu and persists the language ID to the
host's separate settings file.

`Canvas` decodes UTF-8 by code point for drawing, measuring, wrapping, and
truncation. Its fixed 5×7 font includes Czech uppercase/lowercase diacritics;
glyphs are still code-drawn into the same indexed EGA framebuffer.

The CNA adapter owns one `Texture2D`, uploads the logical framebuffer through
`SetDataRGBA`, and draws it using `SpriteBatch` with `PointClamp` and opaque
blending. The logical image is integer-scaled and centred in the current back
buffer.

Audio follows the same boundary. The headless core validates and queues
`ToneEffectDefinition` IDs; it does not initialize an audio device. The CNA host
synthesizes signed 16-bit mono square-wave PCM and gives it to CNA
`SoundEffect`. Effects are monophonic and newly started sounds interrupt the old
one, intentionally matching PC-speaker/QBasic-era limitations. F11 delegates
fullscreen switching to CNA's `GraphicsDeviceManager`. F1 temporarily pauses
the host shell and renders the session's current `HintDefinition`; F1, Enter,
or Escape restores the previous play/pause shell without altering session mode.

The fixed resolution, palette and interface are intentionally not abstracted
away. A game built on Explore2D accepts these constraints in the same way that
a fantasy console game accepts its console's display model.

## Persistence

The save format begins with `EXPLORE2D_SAVE 1` and stores the session snapshot
in a human-readable text form. `loadSnapshot()` validates syntax; the session's
`restore()` separately checks that referenced rooms/items exist in the current
world.

For a shipped game, add a game/content version to the save metadata before the
world begins making breaking ID changes.
