# Design extraction from Tajná mise

This document records which reusable ideas were intentionally carried into
Explore2D after studying the supplied QBasic game/ongoing C++ port and the
supplied game website. It is a design extraction, not a source port.

## Principles retained

### Fixed connected screens

The world is experienced as discrete screens rather than a scrolling camera.
Crossing an edge changes room and places the player at an explicit spawn point.
This makes every room a hand-authored puzzle/exploration composition.

### Tiny movement/action vocabulary

The engine supports walking, a jump/context key, a selected verb and direct
verb shortcuts. `turnBeforeWalk` can reproduce the useful deliberate behaviour
where pressing the opposite direction first turns the character before motion.

### USE / EXAMINE / TAKE

These are first-class engine concepts rather than game-specific callbacks.
EXAMINE can select either the nearby world object or a carried item. TAKE can
reveal/add stateful inventory. USE combines a carried item with the current
world target.

### Inventory as persistent world state

Items are not just icons. Acquiring, consuming or merely carrying an item can
participate in interaction conditions. Optional collectibles are supported as
naturally as mandatory puzzle objects.

### Exploration reveals state

A hotspot can become visible only when conditions are satisfied, so examining
one thing can reveal another takeable object. This directly supports the style
of careful room investigation where examining the environment matters.

### Contextual ENTER

ENTER/Space first tries a contextual rule on an overlapping target. If there is
none, the same input jumps. This supports characters, ladders, levers and other
special actions without adding a large control vocabulary.

### Rule/state driven progression

Flags, counters, items and visited rooms are the central progression mechanism.
This generalizes the original game's heavy dependence on persistent item/state
values while replacing address/value-specific implementation details with named
state.

### Dialogue and scripted beats

An interaction can enqueue multiple speech/inspection/warning messages and apply
mutations before the sequence. This is enough for conversations, discoveries,
chapter gates and many scripted beats while keeping them declarative.
Each line can identify the player or target as speaker. The renderer places a
small blue bubble by that speaker, preserving the scene instead of covering it
with a full-width modal panel.

### Selective animation and action poses

The reference animates particular scenery and actions without treating every
room detail as an animated entity. Explore2D follows that economy: rooms may
declare conditioned looping overlays, while rules may trigger short one-shot
sequences. TAKE also selects a temporary facing-aware crouch/reach player pose.
Static visuals remain the normal case.

### Discovered fast travel

Rooms marked as travel anchors enter the map only after they have been visited
(or explicitly unlocked). The map is therefore a progress convenience, not an
omniscient geometric world map.

### Hazards and death

Rooms can contain conditional death regions, and falling outside an unconnected
screen can also kill the player. Restarting returns to the initial state; normal
save/load is separate.

### Procedural drawing remains viable

Because the original game demonstrates that visually distinctive rooms can be
assembled from drawing primitives, Explore2D does not force bitmap assets.
The supplied game website and QBasic source confirm that ordinary scenes use
`SCREEN 9`: 640×350 with 16 EGA colours. Explore2D now makes that exact display
model part of its public contract. Its `Canvas` provides palette-indexed
equivalents of `PSET`, `LINE`, rectangle fill/outline, `CIRCLE`, ellipse,
`PAINT`, arcs, connected lines/polygons, text, and palette-indexed `GET`/`PUT`
with copy, preset, AND, OR and XOR operations over a CPU framebuffer.

### Restrained tone sound

Effects use sequences equivalent to QBasic `SOUND frequency, duration`, with
historical 18.2 Hz timer ticks, square waves, rests and monophonic playback.
CNA owns the actual audio device; the game does not gain a modern sampled-audio
or music layer merely because CNA could provide one.

### Shared interface and title sequence

The original game's scenes are framed by a stable interface: a large room view,
right-hand textual inventory, and lower action strips. Choices temporarily use
the side panel and longer messages overlay the scene. Explore2D retains this
spatial grammar and provides an engine-owned title/menu phase before gameplay.
Games configure the title, subtitle, colour sequence and procedural artwork,
but do not replace the overall interface with an unrelated one.

## Intentionally generalized or changed

- Explore2D does not preserve QBasic memory addresses or numeric item slots;
  stable string IDs are used instead.
- Jumping uses a small generic velocity/gravity model rather than timing-exact
  emulation of the original jump trajectory.
- The engine does not copy any original room, story, riddle, dialogue or artwork.
- Specialized minigames are not hard-wired into the core. A future custom-mode
  interface can add them without contaminating ordinary room logic.

## Why this boundary

The reusable value of Tajná mise is its interaction grammar: compact controls,
screen-by-screen exploration, stateful inventory, environmental examination and
world changes. Those principles are more useful to new games when expressed as
data and deterministic rules than when the engine attempts to imitate every
historical implementation detail.
