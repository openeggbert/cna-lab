# People architecture and product analysis

Date: 2026-08-24

Status: living design analysis. Implementation decisions proven by code and
tests supersede speculation here and must be reflected back into this file.

## 1. Product thesis

People is a household life simulator, not a colony simulator, city builder, or
realtime 3D avatar game. Its durable fantasy is:

```text
build a home
  + furnish it
  + place residents in it
  + guide their choices
  + watch object-driven autonomy create stories
```

The most valuable historical qualities are immediacy, legible needs, concrete
household constraints, objects with meaningful tradeoffs, interruption and
failure, social feedback, and the tension between time, money, work,
relationships, and self-care. Those are generic ideas. People will express them
through an original world, rules, constants, text, visuals, audio, user
interface, object catalog, characters, careers, brands, and fiction.

The first setting is provisionally **Juniper Vale**, an optimistic and slightly
exaggerated suburban place with warm turn-of-the-millennium computer aesthetics.
The setting and the public title are not cleared trademarks. A formal name and
trademark review is a release gate, not an assumption.

## 2. Non-negotiable legal and asset boundary

People must be clonable and playable with only:

```text
people-cna
cnanext
sharp-runtimenext
People-owned or permissively redistributable assets
```

It will never require an installation of another game. It will not load or
convert proprietary sprites, sounds, music, object data, neighborhoods, UI,
fonts, animations, archives, scripts, characters, or saves. Compatibility with
proprietary formats is outside the primary project and not on the roadmap.

The clean boundary is stronger than "do not commit assets": People also avoids
building an engine whose usefulness depends on users supplying those assets.
Placeholder art is generated procedurally inside People until owned art passes
provenance review.

Mechanics and observable behavior may be studied. Copyrighted expression must
not be recreated. Generative systems do not weaken this boundary: prompting for
a pixel-perfect or recognizable one-for-one recreation is prohibited.

New People code is MIT. CNA is Ms-PL, sharp-runtime is MIT, and external
reference projects retain their own terms. See `THIRD_PARTY.md`.

## 3. Reference analysis

### 3.1 Classic household simulation

Public manuals and observable play describe a loop formed from distinct live,
buy, and build modes; eight visible needs; household money; object quality;
skills and careers; relationships; and an open-ended rather than victory-based
structure. The important design lesson is coupling:

- better objects exchange scarce money and floor space for time or motive
  efficiency;
- poor needs reduce the time available for skill, work, and relationships;
- career advancement depends on preparation beyond merely attending work;
- household layout affects route time and contention;
- direct commands coexist with autonomous recovery behavior;
- failure, imperfect timing, and conflicting needs create stories.

People should preserve this pressure network but tune its units and curves from
its own playtests. Exact motive names, constants, prices, schedules, catalog
descriptions, icons, UI arrangement, and audiovisual feedback are not copied.

Useful public behavioral sources include:

- an archived original-era manual:
  <https://smetisteher.cz/manualy/sims_manual.pdf>;
- the FreeSO Volcanic technical paper:
  <https://freeso.org/stuff/Volcanic.pdf>;
- descriptive project material linked below.

These references inform questions, not source code.

### 3.2 FreeSO

FreeSO's public project-structure documentation separates its simulation VM,
entities, primitives, routing/model data, marshaled save state, architecture
editing, and lot rendering. The renderer distinguishes architecture, dynamic
entities, and static objects and can cache static presentation. This validates
several useful architectural ideas:

- simulation entities need explicit serializable state;
- behavior execution benefits from inspectable primitives and traces;
- architecture edits are commands over logical topology;
- static and dynamic rendering workloads should be distinguishable;
- deterministic tick inputs and command streams make reproduction/debugging
  practical;
- editor/debug tooling is a first-class requirement for complex objects.

Source: <https://github.com/riperiperi/FreeSO/wiki/Project-Structure>.

People will not reproduce SimAntics, its bytecode, its primitive numbering, or
its data formats. It will independently implement only the concepts proven
necessary by working objects. FreeSO is MPL-2.0 and depends on original
commercial resources, so neither its code nor its asset-loading model enters
People's MIT source.

### 3.3 Simitone

Simitone demonstrates the integration burden of connecting neighborhood,
household, UI, build/buy, save, object, and avatar systems. Its top-level
repository embeds FreeSO and requires a legitimate original installation. It
also explores realtime/hybrid 3D, which is specifically not People's runtime
direction.

The useful lesson is scope discipline: one active lot and a complete small
household loop are more valuable than broad neighborhood shells. Simitone is
reference-only. The inspected repository root did not expose a clear license
file, so no reuse is safe without a future exact file-level review.

Source: <https://github.com/riperiperi/Simitone>.

## 4. Dependency inspection

People targets the newer sibling checkouts requested by the project owner:

| Dependency | Branch | HEAD at inspection | Worktree |
|---|---|---|---|
| `../cnanext` | `next` | `33ff296f5ffe42cfa9c3a2060da55a953f2a9f4e` | dirty, 31 pre-existing paths |
| `../sharp-runtimenext` | `next` | `54578590b328aa9612fe38bfddca9fd8ca795144` | clean |

The dirty CNA tree is a reproducibility risk: its actual API/build behavior may
include changes not named by HEAD. People must not edit or clean it. A release
must switch to a reviewed clean commit or record a patch identity separately.

### 4.1 Actual CNA consumption model

Inspection of the current checkout establishes:

- CNA is consumed with `add_subdirectory`; there is no assumed installed
  `find_package(CNA)` flow.
- `CNA_SHARP_RUNTIME_ROOT` is CNA's real cache path override. People must set it
  to the absolute `../sharp-runtimenext` path before adding CNA.
- CNA's public compatible aggregate is the `CNA` interface target.
- `CNA_BUILD_TESTS` and `CNA_BUILD_EXAMPLES` can be forced off for a consumer.
- `CNA_ENABLE_VIDEO=OFF` avoids optional FFmpeg work not needed by the first
  slice.
- `CNA_GRAPHICS_RENDERER`, `CNA_PLATFORM`, and `CNA_AUDIO_PLATFORM` are the real
  renderer/window/audio selection axes.
- `cna_copy_renderer_runtime(target)` is the supported runtime-payload hook.
- `SDL3::SDL3main`, when present, is a platform entrypoint support target used
  by CNA's own examples. Naming it in CMake is not a backend bypass in game
  code.

The verified public runtime surface includes `Game`, `GraphicsDeviceManager`,
`GraphicsDevice`, `SpriteBatch`, `Texture2D`, `Color`, `Rectangle`, `Vector2`,
`Keyboard`, and `Mouse`. `Texture2D(GraphicsDevice&, width, height)` plus
`SetData(Color*, count)` supports generated placeholder textures. SpriteBatch
supports position, destination rectangle, source rectangle, tint, scaling,
rotation, and depth overloads. Mouse exposes absolute position and cumulative
wheel state through CNA's XNA-style property naming.

People game code will not branch on renderer names. It will use the common 2D
surface and create `PEO-CNA-*` blockers for behavior that violates it.

### 4.2 CNA risks and validation strategy

- A dirty dependency may change mid-session. Record SHA and dirty count at each
  milestone; reconfigure if headers or CMake files change.
- Different SpriteBatch backends may disagree about alpha, sampling, tint, and
  edge pixels. Use point sampling for pixel-art-like sprites and add a small
  cross-renderer corpus later.
- Mouse/window behavior needs a displayed smoke test; coordinate math remains
  independently unit tested.
- Content loading is still evolving in the dirty checkout. Initial textures are
  generated in memory so renderer work is not blocked on asset pipelines.
- Optional audio and video should remain disabled until their milestone.

No framework blocker is recorded yet because People has not observed a failing
behavior. Speculation is not a blocker.

## 5. Runtime architecture

The initial source graph is intentionally small:

```text
people_core (no CNA dependency)
  |- world coordinates and rotations
  |- isometric projection and inverse picking
  |- deterministic sort keys
  `- later simulation, routing, interactions, persistence DTOs

People executable
  |- CNA Game lifecycle and input
  |- camera presentation state
  |- generated Texture2D placeholders
  `- SpriteBatch renderer
```

The simulation owns tile positions, time, people, objects, motives, queues,
reservations, and random streams. Presentation owns screen origin, zoom, view
rotation, textures, animation frame, and selection. Changing the camera cannot
mutate simulation positions.

## 6. Isometric coordinates and rotations

### 6.1 Coordinate spaces

Use explicit types or naming for:

- world tile coordinate `(x, y, floor)`;
- view tile coordinate after discrete rotation;
- projected lot-local pixels;
- final screen pixels after camera origin and zoom;
- mouse screen pixels.

For a 20 x 20 lot, rotation is around the lot's indexed bounds, not around an
unbounded origin. With width `W` and height `H`:

```text
r0: (x, y)
r1: (H - 1 - y, x)
r2: (W - 1 - x, H - 1 - y)
r3: (y, W - 1 - x)
```

Rectangular lots swap view width/height for odd rotations. This avoids negative
view coordinates and makes bounds/picking testable. Inverse rotation restores
the exact simulation coordinate.

### 6.2 Projection

At zoom 1.0, People v1 uses a 96 x 48 tile:

```text
halfW = 48
halfH = 24
screenX = (viewX - viewY) * halfW
screenY = (viewX + viewY) * halfH - floor * floorHeight
```

The dimensions provide cleaner silhouettes than 64 x 32 while retaining a
cheap integer base transform and allowing 0.5x presentation on smaller screens.
They are not selected for compatibility with another engine.

Picking reverses camera translation and zoom, applies the analytical inverse,
then resolves boundary ambiguity by testing the candidate diamond and nearby
tiles. A point exactly on a shared edge uses a documented stable tie break so
all four rotations behave consistently.

Unit tests cover world/view round trips, rectangular bounds, projection
round-trips at tile centers, diamond edges, outside-lot rejection, floor
offsets, camera translations, zoom, and all four rotations.

## 7. Camera model

The camera contains only presentation state:

```text
screen origin/pan in pixels
zoom clamped to an intentional range
view rotation 0..3
```

Keyboard pan is time-based. Mouse-wheel zoom is centered on the pointer: the
world point beneath the cursor should remain stable when the zoom changes.
Rotation is discrete and edge-triggered, retaining a stable lot focus. No
arbitrary yaw or perspective camera exists.

The initial controls are developer-oriented: WASD/arrows pan, wheel or +/-
zoom, Q/E rotate, Escape exits. Controls later move into original UI and
settings data.

## 8. Depth sorting

Insertion order is not a contract. Every draw item receives a deterministic key
whose fields are compared lexicographically:

```text
floor
view-space max(x + y) of footprint
draw layer
view-space sort anchor y/x
local offset
stable entity ID
```

Initial layers include terrain, floor covering, floor detail/rug, wall-back,
object/person, wall-front, effect, and UI. Stable entity ID breaks genuine
ties; it must not be the primary depth rule.

Large footprints use their farthest view-space extent for coarse ordering and a
declared contact/sort anchor for ties. Later, objects that interleave with
people or walls must be segmented into independently sortable sprite parts or
declare multiple sort regions. Painter's-order cycles should be detected in
debug builds rather than hidden by insertion order.

## 9. Floors, walls, rooms, and cutaway

The logical lot is a bounded grid of floors. Floor coverings occupy tile cells.
Walls occupy directed canonical edges between tile vertices; normalization
ensures adjacent tiles refer to one wall, not duplicate state. Doors and
windows attach to wall edges and add state/portals rather than replacing the
topology with decorative sprites.

The first implementation has one floor. Multi-floor data is designed only far
enough to keep `floor` in coordinates and persistence; stairs and upper-story
rendering wait until the household loop works.

Rooms are connected floor regions separated by closed wall edges. Doors can
affect routing connectivity without necessarily merging environment/privacy
semantics. Build edits mark a local topology region dirty. A simple whole-floor
flood fill is acceptable initially, but the API should expose dirty rebuilds so
large houses do not force per-frame global work.

Wall presentation modes are `Up`, `Cutaway`, and `Down`. Initial walls may stay
up. Automatic cutaway later identifies camera-facing walls for the selected
person/room in view space, so the same logical walls work under all rotations.

## 10. Object model

Definitions are immutable content; instances are persistent simulation state.
The split prevents visual/catalog metadata from becoming mutable world truth.

An object definition eventually includes:

- stable content ID and localized display keys;
- catalog category, price, description, and statistics;
- footprint and allowed orientation mask;
- directional/state sprite references and anchors;
- placement predicates;
- routing and interaction slots;
- advertised interactions and estimated motive/economic effects;
- state schema and degradation policy.

An instance includes stable entity ID, definition ID, tile/floor/orientation,
owner household, state values, reservation state, contained entities, and
depreciation/condition where enabled.

Definitions become external data after several native objects prove the schema.
A huge object inheritance hierarchy and an object-type switch are both avoided.

## 11. Placement, routing slots, and reservations

Placement validates bounds, floor support, rotated footprint occupancy, wall or
surface requirements, routing clearance, and special predicates. Preview uses
the same validator as commit. Transactions apply money and world mutation
atomically so failed placement cannot leak either.

Interactions target slots, not merely object tiles. A slot declares relative
tile offset, facing, posture, capacity/exclusivity, and approach clearance.
Rotating an object transforms the slot through the same integer rotation
utility used by footprints.

Reservations are explicit owner/target/slot records. Release occurs on all
terminal paths: success, cancellation, interruption, route failure, deletion of
either endpoint, and load repair. Tests must exercise each path, including two
people contending for one bed or toilet.

## 12. People and action queues

A resident simulation entity owns identity, household reference, tile/sub-tile
movement state, motives, mood, personality, skills, directional relationships,
career, inventory, action queue, current interaction, and deterministic random
stream identifier. Rendering owns sprite and animation frame separately.

Action state is centralized:

```text
Queued -> Routing -> Executing -> Completed
                     |              |
                     +-> Failed <---+
Queued/Running -> Canceled or Interrupted
```

Each transition records a reason and releases resources. Player-issued actions
can carry priority without silently erasing autonomy. The queue is inspectable
from the first interaction milestone even before polished UI.

## 13. Motives, mood, and personality

Baseline candidate motives are nourishment, rest, hygiene, relief, connection,
enjoyment, ease, and surroundings. Working code may initially use familiar
technical labels such as Hunger/Energy/Bladder, but shipping terminology and UI
will be independently reviewed.

Each motive defines normalized units, min/max, periodic decay, state modifiers,
warning/critical thresholds, and utility response curve. The simulation clock
applies motive changes at a lower frequency than movement without coupling to
render frames.

Mood is a derived summary, not a ninth independently drifting resource. Use a
weighted nonlinear aggregate so one critical need cannot be hidden by several
full bars. Environment and recent events can modify it later.

Personality begins with a small original set of axes affecting utility weights,
social acceptance, cleaning tolerance, activity, patience, and ambition. Named
traits wait until axes produce visible behavior.

## 14. Interaction architecture and PeopleBehavior

Objects advertise interactions containing eligibility, target slot, reservation
mode, expected utility effects, duration/cost, object and person preconditions,
animation/timeline key, and terminal effects.

The progression is deliberate:

1. implement bed/chair/refrigerator/toilet interactions as explicit native C++
   state machines;
2. identify common operations with tests;
3. express linear deterministic action sequences in data;
4. add an original small interpreter only when branching/reuse justifies it;
5. migrate objects incrementally while retaining behavior traces.

Working interpreter name: **PeopleBehavior**. It is not SimAntics compatible.
Potential primitives include route, reserve, release, wait, animate, modify
motive, modify money/relationship, read/write typed object state, branch,
weighted choice, skill/motive tests, spawn/delete, sound, call, and return.

Every primitive has explicit inputs/outputs, deterministic timing, a bounded
step budget, a serializable program counter where needed, and a trace record.
There is no arbitrary native scripting or proprietary bytecode loader.

## 15. Autonomy

Final autonomy queries eligible advertised interactions and scores them from:

```text
motive deficit response
* advertised effect confidence
* personality/preference weight
+ context and player-policy modifiers
- route/time/cost/contention penalties
+ small seeded variation
```

The scheduler need not evaluate every object every frame. It runs periodically,
when a critical state changes, or after queue completion/failure. Candidate
discovery first uses a lot query; later it uses spatial indices and cached
definition advertisements.

The chosen action and top rejected candidates are inspectable with individual
score components. This observability is essential for tuning. A hardcoded
"hungry means fridge" prototype may exist briefly, but the first autonomous
three-motive milestone must use generalized advertisements.

## 16. Routing

Initial navigation is deterministic four-neighbor tile-grid A*. The route graph
considers bounds, floor, static footprints, wall edges, door portals, target
slot access, and a stable neighbor order. The heuristic is Manhattan distance
with deterministic tie breaks.

Dynamic people should be a soft occupancy cost initially rather than permanent
walls, with local waiting/replan to avoid deadlock. Exclusive destinations are
protected by reservations before route execution. Stairs later add explicit
cross-floor edges.

Navigation does not know sprite dimensions. Movement converts path segments to
sub-tile progress and a cardinal/isometric-facing animation state.

## 17. Time and determinism

Use a fixed simulation step with an accumulator separated from Draw. Proposed
starting frequency is 20 simulation ticks per real second at 1x. Pause/1x/2x/3x
changes how many fixed steps are consumed, not the delta passed to systems.

Multi-rate schedules are integer tick divisors:

- movement and interaction timelines: every tick;
- autonomy: on events plus a modest periodic interval;
- motives: periodic fixed interval;
- rooms/environment/economy/statistics: dirty or slower intervals.

Random services use named streams derived from an explicit world seed and
stable entity/system IDs. No `rand()`, time-seeded global generator, hash-order
randomness, or render-frame randomness is allowed. Save files preserve the
world seed and state/counters needed to resume deterministically.

## 18. Relationships, skills, careers, and economy

Relationships are directional per resident pair and can later separate recent
sentiment, durable bond, familiarity, social flags, and significant events.
Social actions synchronize two queues/reservations and resolve outcomes from
both participants' state.

Skills are data definitions with levels/experience and modifiers to speed,
quality, success, career requirements, and accident probability. Careers are
data-driven levels, schedules, salary, skill/relationship/performance
requirements, and promotion rules. Work is initially off-lot abstract state.

A household owns resident IDs, lot ID, funds, object ownership, bills, and
progression. All money changes are ledger-style transactions with reason and
source IDs. Build/buy validates funds and commits cost plus mutation atomically.

## 19. Content system

The content layer eventually owns definitions for objects, interactions,
behaviors, careers, skills, animations, sounds, catalog categories, and asset
manifests. JSON is a likely human-authored format, but adoption waits for an
inspection of the actual CNJ/JSON support available through the selected CNA and
sharp-runtime revisions. The first schema must be validated with precise error
paths and reject unknown required fields.

Stable string IDs survive save files and mods. Runtime numeric handles may
accelerate lookup but are never serialized as the only identity. Core content
and user content occupy explicit namespaces to avoid accidental collisions.

## 20. Persistence

Save data is explicit, versioned, and transactional. It persists world/lot
dimensions, floor/wall topology, rooms as derived or validated cache, object
instances and states, residents, motives, relationships, skills, careers,
household money, clock, and random state. Active interactions are either fully
serialized at declared safe points or repaired/canceled deterministically on
load.

Write a temporary file, validate/flush, then atomically replace the destination
where supported. Never serialize vtables, pointers, padding, or raw C++ memory.
Migration fixtures begin before public save versions multiply.

## 21. Rendering and asset batching

Each frame gathers render items from visible floors, architecture, objects,
people, and effects; derives view-specific sprites and sort keys; sorts once;
and submits to SpriteBatch. The first milestone can draw a separate generated
tile texture per cell. Static lot caching, atlases, viewport culling, and dirty
chunks follow measurements.

Selection highlighting initially uses a generated diamond overlay. Picking is
logical, not alpha-hit-testing the texture. Object picking later checks sorted
screen bounds or authored hit masks from front to back.

Zoom uses SpriteBatch scale/destination rectangles and point sampling. The world
does not become 3D merely because CNA internally submits textured triangles.

## 22. Developer observability

The debug UI grows with systems and may remain text/basic shapes initially. It
should expose selected entity IDs, tile and screen coordinates, room/occupancy,
sort keys, path, slots/reservations, motives, mood, action queue, interaction
state, autonomy candidates with score components, behavior trace, seed/tick,
FPS, and simulation work counters.

Debug tools consume public simulation inspection APIs and do not become the
only route to mutate state. Deterministic reproduction commands should include
seed, tick, lot/save, and input/action log where available.

## 23. Test strategy

Start with a dependency-free `people_core_tests` executable registered in
CTest. This avoids making GoogleTest or networking a prerequisite. Tests use a
small assertion harness with failure messages and nonzero exit.

Required families:

- four rotation transforms and inverses, including rectangular lots;
- projection, inverse projection, zoom/pan, tile-center and edge picking;
- deterministic sort keys and footprint rotation;
- placement and wall-edge normalization;
- room flood fill and dirty rebuild;
- A* reachability, stable ties, doors, footprints, and failure;
- reservation acquire/contention/release on every terminal path;
- fixed clock, pause/speed, motive decay, and seeded random reproduction;
- queue transitions and interaction completion/failure/cancel;
- autonomy score components and stable choices;
- directional relationships and synchronized social actions;
- household transactions and insufficient-funds atomicity;
- versioned save round trips, invalid data, and migrations.

Displayed smoke tests prove CNA lifecycle, input, SpriteBatch, Texture2D upload,
and presentation. They supplement rather than replace logic tests. A future
backbuffer readback/golden corpus should tolerate only documented renderer
differences.

## 24. Scalability

Target active-lot scale is eight residents, hundreds of objects, a large
multi-floor house, and comfortable ordinary hardware. Architectural
consequences:

- use stable dense handles or iteration-friendly storage while retaining
  persistent IDs;
- partition spatial queries by tile/chunk and definition advertisement;
- do not scan every object for every resident per rendered frame;
- rebuild rooms and cached render chunks only when relevant topology/state is
  dirty;
- batch sprite submissions by atlas/state after global depth order permits it;
- cap behavior work per tick and surface overruns;
- measure route/autonomy/sort costs before complex optimization.

Neighborhood scale stores dozens of lots/households but simulates only the
active lot at full frequency. Off-lot work and progression use coarse events.
People remains a household simulator.

## 25. Milestone reasoning

The mandatory order is:

```text
CNA lifecycle and tested projection
  -> visible lot and picking
  -> walls/room and placeholder objects
  -> one resident and A*
  -> bed route/reserve/sleep end-to-end
  -> three motives plus bed/refrigerator/toilet autonomy
  -> build/buy and household money
  -> save/load and time controls
  -> social/careers/content expansion
```

The behavior interpreter, avatar customization, multi-floor rendering,
neighborhood simulation, and hundreds of objects must not precede the first
resident sleeping successfully.

## 26. Major risks

1. **Dirty CNA dependency.** Behavior is not captured by SHA. Mitigation: never
   modify it, record state, validate current APIs, and move to a clean pin.
2. **Cross-renderer SpriteBatch differences.** Mitigation: common API subset,
   generated corpus, explicit alpha/sampler policy, blocker records.
3. **Picking/sorting under rotation.** Mitigation: bounded integer rotation,
   analytical transforms, stable tie rules, exhaustive tests.
4. **Interaction state explosion.** Mitigation: explicit queues and terminal
   cleanup; extract behavior primitives only from working objects.
5. **Autonomy performance and opacity.** Mitigation: periodic/event-driven
   evaluation, cached candidates, decomposed score traces.
6. **Reservation leaks/deadlocks.** Mitigation: centralized ownership and tests
   for every terminal/deletion route.
7. **Content-schema churn.** Mitigation: delay externalization until native
   objects prove requirements; version schemas and validate strictly.
8. **Art inconsistency/provenance.** Mitigation: fixed v1 standard, quarantine,
   machine-readable manifest, placeholders, no unreviewed assets.
9. **Scope diffusion.** Mitigation: stable plan, playable acceptance gates, one
   active lot and adult household before expansion systems.
10. **Legal confusion from references.** Mitigation: no proprietary-data path,
    no code copying, explicit third-party record, release clearance.

## 27. Decisions requiring later evidence

- Whether CNA's CNJ or another current API should carry authored definitions.
- Whether four-view character layers remain aligned and performant enough for
  runtime composition or should be flattened offline.
- Whether complex furniture needs sprite segmentation or a partial-order graph.
- Fixed tick frequency after movement and interaction feel tests.
- Exact motive terminology, curves, autonomy weights, and economy balance.
- Static render-target caching after actual profiling.
- Multi-floor wall cutaway and stair representation after the first household
  loop is stable.

These are deliberate open decisions, not invitations to build unused abstract
frameworks now.
