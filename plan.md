# People development plan

Last updated: 2026-08-24

This is the stable implementation ledger. IDs are never removed or renumbered.
Completed entries remain in place. New work receives a new ID in the relevant
range or a later unused range.

Statuses:

- `TODO`: accepted, ready or awaiting prerequisites.
- `DOING`: exactly one primary task for the current agent/session.
- `DONE`: acceptance criteria actually verified and recorded in Git.
- `BLOCKED`: cannot proceed; blocker is documented with evidence.
- `DEFERRED`: intentionally outside the current milestone.

## Milestone gates

| Gate | Required outcome |
|---|---|
| M0 Planning baseline | Legal boundary, dependency findings, architecture, asset standard, agent rules, and stable plan committed on `main` |
| M1 Isometric lot | CNA executable displays a generated 20 x 20 lot with pan, zoom, four rotations, deterministic tile picking, and tested transforms |
| M2 Small house | One logical room, walls, a door, and five original procedural furniture placeholders render and rotate correctly |
| M3 First interaction | One resident routes to a bed, reserves it, sleeps, gains energy, releases it, and completes an inspectable action |
| M4 Autonomous essentials | Bed, refrigerator, toilet, three motives, player actions, generalized utility scoring, and time controls support a simple day |
| M5 First vertical slice | Small house, substantial motive set, shower/kitchen/living objects, build/buy, money, and versioned save/load are playable |
| M6 Social/progression slice | Multiple residents, directional relationships, synchronized social actions, skills, work schedules, salary, and bills |
| M7 Neighborhood alpha | Multiple saved households/lots with coarse off-lot progression and household switching |

## Foundation (`PEO-001`–`PEO-009`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-001 | DONE | Repository planning baseline | Required documents exist, consistently name People/`people-cna`, state zero proprietary data and 2D runtime, record `cnanext`/`sharp-runtimenext` SHA and dirty state, pass documentation checks, and are committed on `main` |
| PEO-002 | DONE | CNA/sharp-runtime CMake integration | CMake uses C++23, finds `../cnanext`, forces `CNA_SHARP_RUNTIME_ROOT` to `../sharp-runtimenext`, disables unused CNA tests/examples/video, links aggregate `CNA`, and configures without changing dependencies |
| PEO-003 | DONE | Minimal People executable | CNA opens/runs, clears/draws with SpriteBatch, supports `--smoke-test`, exits cleanly, and has no backend API in game code |
| PEO-004 | TODO | Deterministic simulation clock | Fixed-step accumulator supports pause/1x/2x/3x, clamps runaway catch-up, is render-independent, and passes timing tests |
| PEO-005 | DONE | Core test infrastructure | Dependency-free `people_core_tests` is registered with CTest, reports individual failures, and runs without display/audio |
| PEO-006 | TODO | Runtime/dependency diagnostics | Startup reports People version, CNA renderer/capabilities, dependency identity supplied at configure, and simulation seed without branching on renderer name |
| PEO-007 | TODO | Warning-clean build policy | People targets enable practical warnings, Debug build is warning-free on current compiler, and warnings from untouched dependencies are not hidden globally |
| PEO-008 | TODO | Seeded random service | Named streams derive reproducibly from world seed plus stable IDs; no `rand()`; sequence and stream-isolation tests pass |
| PEO-009 | TODO | Blocker and decision ledger | Templates for `PEO-CNA-*`, `PEO-SR-*`, and architecture decisions record revision/platform/renderer/repro/impact/workaround/status |

## Isometric renderer (`PEO-010`–`PEO-029`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-010 | DONE | Coordinate value types | World tile, view tile, projected pixel, and camera state cannot be accidentally mixed; units/invariants are documented |
| PEO-011 | DONE | Four bounded rotations | All four world-to-view and inverse transforms work for square and rectangular lots with exhaustive bounds round trips |
| PEO-012 | DONE | Isometric projection | 96 x 48 projection and floor elevation produce expected exact tile centers; no render dependency |
| PEO-013 | DONE | Inverse projection/picking | Screen-to-world selects tile centers/insides, rejects outside points, and has deterministic shared-edge rules in all rotations |
| PEO-014 | DONE | Generated floor texture | CNA creates an original transparent diamond tile in memory with clean outline/alpha and no external asset |
| PEO-015 | DONE | 20 x 20 lot renderer | Visible grass/background and complete 20 x 20 diamond grid render through SpriteBatch with deterministic draw order |
| PEO-016 | DONE | Camera pan | WASD/arrows pan by elapsed real time, diagonal motion is normalized, and focus remains numerically stable |
| PEO-017 | DONE | Camera zoom | Wheel and +/- clamp zoom; cursor-centered zoom preserves the logical point under the cursor within tolerance |
| PEO-018 | DONE | Camera rotation controls | Q/E change exactly one of four orientations on key edges and preserve lot focus/selection |
| PEO-019 | DONE | Selection highlight | Hovered in-bounds tile is overlaid with an original generated diamond; selection tracks pan/zoom/rotation |
| PEO-020 | DONE | Deterministic render keys | Layer/floor/depth/anchor/stable-ID comparison is independent of insertion order and unit tested |
| PEO-021 | TODO | Render-item gather/sort | World renderables are gathered then globally sorted before SpriteBatch submission; debug builds can display keys |
| PEO-022 | TODO | Viewport culling | Conservative projected bounds omit invisible tiles without popping at zoom/rotation edges; tests cover bounds math |
| PEO-023 | TODO | Multi-tile sort analysis spike | At least three overlap cases establish segmentation/multiple-anchor policy; no unused runtime framework is added |
| PEO-024 | TODO | Object screen picking | Front-to-back authored bounds/hit regions select overlapping objects consistently under all rotations |
| PEO-025 | TODO | Floor-level projection | Floor offset and visible-floor filtering work in transform/tests without yet enabling upper-floor gameplay |
| PEO-026 | DONE | Renderer smoke screenshot | A displayed or backbuffer-read smoke captures the generated lot; result and limitations are recorded |
| PEO-027 | TODO | Cross-renderer 2D corpus | Two available CNA renderers draw/read a minimal People tile/object corpus with documented exact/tolerant comparisons |
| PEO-028 | TODO | Render performance counters | Gather, sort, draw-item, batch, frame, and simulation timing are inspectable without affecting release determinism |
| PEO-029 | DONE | M1 isometric-lot gate | Build/tests/runtime verify pan, zoom, rotation, picking, sorting, and zero runtime 3D; milestone state is documented |

## Lot and architecture (`PEO-030`–`PEO-049`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-030 | DONE | Lot grid model | Bounded width/height/floor storage validates coordinates and remains renderer-independent |
| PEO-031 | DONE | Floor tile state | Base terrain and optional floor-covering IDs persist independently from textures and support dirty flags |
| PEO-032 | DONE | Canonical wall edges | Shared edges normalize to one record; add/remove/query and boundary cases pass tests |
| PEO-033 | DONE | Procedural wall sprites | Original placeholder wall segments render in correct back/front layers for all views |
| PEO-034 | TODO | Wall placement/removal core | Logical mutations validate funds/topology hooks and mark affected room/render regions dirty |
| PEO-035 | DONE | Room detection | Flood fill assigns stable room regions from floors/wall boundaries; simple, split, merged, and outside cases pass |
| PEO-036 | TODO | Incremental room invalidation | Wall edits rebuild only a declared dirty area or intentionally fall back with measurable whole-floor rebuild |
| PEO-037 | DONE | Door edge model | Door attaches to valid wall edge, exposes route portal/open state, and removes cleanly with topology updates |
| PEO-038 | DONE | Procedural door rendering | Four view-correct closed/open placeholder door sprites align to wall edges |
| PEO-039 | TODO | Window edge model | Window attaches to valid wall without becoming a route portal and persists its state |
| PEO-040 | TODO | Wallpaper/paint state | Independent interior/exterior wall finishes are data references and render with placeholder colors |
| PEO-041 | TODO | Floor-covering placement | Covering add/remove is bounded, priced transaction-ready, and visually distinct without affecting walkability |
| PEO-042 | TODO | Wall visibility modes | Up/Cutaway/Down explicitly determine wall draw sets for each view and are keyboard/debug selectable |
| PEO-043 | TODO | Selected-room cutaway | Camera-facing walls obscuring selected resident/room are hidden consistently under all rotations |
| PEO-044 | TODO | Architecture command history | Build mutations are reversible commands with exact cost/refund data and undo/redo tests |
| PEO-045 | TODO | Architecture validation | Invalid isolated doors/windows, out-of-bounds walls, and unsupported floor edits fail with actionable reason codes |
| PEO-046 | DONE | One-room demo lot | A small original room with floor, walls, and one door loads from owned code/data and renders in four views |
| PEO-047 | DEFERRED | Multiple stories | Add upper-floor editing/visibility only after M5; floor transforms and persistence already reserve the dimension |
| PEO-048 | DEFERRED | Stairs | Explicit cross-floor footprint and navigation edges after multiple stories are playable |
| PEO-049 | DONE | Architecture M2 sub-gate | Room, walls, door, rotations, sorting, and picking pass tests and displayed smoke |

## Object system (`PEO-050`–`PEO-069`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-050 | DONE | Definition/instance split | Immutable definitions and persistent instances use stable IDs; renderer state is absent from simulation instances |
| PEO-051 | DONE | Four-view sprite references | Direction/state selection maps view plus object orientation deterministically to authored sprite metadata |
| PEO-052 | DONE | Rotatable footprints | Arbitrary small cell masks rotate through four orientations and return to origin after four turns |
| PEO-053 | DONE | Basic placement validator | Bounds, floor support, occupied footprint, allowed rotation, and clearance return structured results with tests |
| PEO-054 | TODO | Object rotation | Rotation transforms footprint, slots, sprite choice, and selection preview without moving the anchor |
| PEO-055 | DONE | Catalog model | Categories/items are definition-driven and queryable without an object-type switch |
| PEO-056 | TODO | Object state values | Typed/validated persistent values support cleanliness/broken/occupied-style fields without living in render code |
| PEO-057 | TODO | Interaction slot model | Relative tile, facing, posture, capacity, and clearance rotate correctly and expose a world-space target |
| PEO-058 | TODO | Reservation service | Exclusive slot acquire/query/release supports contention and owner identity deterministically |
| PEO-059 | TODO | Reservation cleanup | Completion/cancel/interruption/route failure/object deletion/person deletion/load repair each release reservations in tests |
| PEO-060 | TODO | Placement preview | Valid/invalid footprint cells and reason are visible; commit reuses the identical validator |
| PEO-061 | TODO | Object deletion | Deletion cancels affected interactions, releases slots, clears occupancy, and emits a persistence-safe mutation |
| PEO-062 | TODO | Surface/container slots | Objects can require/place on authored surface slots with capacity and contained-object persistence |
| PEO-063 | TODO | Wall object constraints | Painting-like placeholder proves wall requirement/orientation validation without adding final decoration content |
| PEO-064 | TODO | State-derived sprites | Visual state selection reacts to typed object state without renderer mutation of state |
| PEO-065 | TODO | Object condition/degradation seed | Optional condition clock and break/dirty events are deterministic and disabled for objects without policy |
| PEO-066 | DONE | Procedural furniture generator | Bed, chair, table, refrigerator, and toilet each have original four-view placeholder sprites and metadata |
| PEO-067 | DONE | Five-object demo placement | Objects validate footprints, rotate, render/sort, and remain selectable inside the demo room |
| PEO-068 | TODO | Catalog/object inspector | Developer UI displays ID, footprint, orientation, state, slots, reservations, and sort anchor |
| PEO-069 | DONE | Object M2 gate | Five furniture types plus room/door satisfy displayed four-view placement and sorting acceptance |

## Residents, routing, and actions (`PEO-070`–`PEO-089`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-070 | DOING | Resident simulation entity | Stable ID, household, logical position, movement/action references, and deletion lifecycle exist without sprite state |
| PEO-071 | TODO | Four-view placeholder resident | Original procedural resident has four directions and idle frames with common foot anchor |
| PEO-072 | TODO | Walk animation | Movement direction selects walk frames by simulation progress; animation cannot alter route state |
| PEO-073 | TODO | Static occupancy grid | Floor/walls/object footprints produce deterministic walkability independent of renderer |
| PEO-074 | TODO | Tile-grid A* | Deterministic four-neighbor path finds shortest routes, handles no-path/start=goal, and passes fixed maps |
| PEO-075 | TODO | Door routing | Closed/open/passable policy integrates wall portals and produces expected paths |
| PEO-076 | TODO | Route to interaction slot | Planner targets authored approach tile/facing rather than object origin and reports exact failure reason |
| PEO-077 | TODO | Dynamic occupancy policy | Residents wait/replan around temporary people without permanent deadlock in two corridor scenarios |
| PEO-078 | TODO | Movement executor | Fixed-tick sub-tile progress follows path, updates facing, arrives exactly, and handles obstruction/replan |
| PEO-079 | TODO | Explicit action states | Queued/Routing/Executing/Completed/Failed/Canceled/Interrupted transitions and reason data are centralized/tested |
| PEO-080 | TODO | Per-resident action queue | Enqueue, priority, cancel current/queued, inspection, and bounded length policies pass tests |
| PEO-081 | TODO | Player-directed action command | Selecting resident/object can enqueue an eligible interaction without directly mutating motive/object state |
| PEO-082 | TODO | Native interaction contract | Eligibility, route target, reservation, timeline tick, completion, failure, and cleanup share a minimal interface |
| PEO-083 | TODO | Resident/path debug overlay | Selected resident shows route, destination slot, action state, and failure reason |
| PEO-084 | TODO | Resident deletion cleanup | Actions, social links, occupancy, and reservations reach safe terminal state when resident is removed |
| PEO-085 | TODO | Carry state | Resident can reference one carried entity/visual anchor with cleanup; no inventory system is implied yet |
| PEO-086 | TODO | Interaction interruption policy | Critical/player/system interruption points are explicit and never leave half-applied terminal effects |
| PEO-087 | TODO | Queue serialization boundary | Safe persistent queue fields or deterministic cancellation-on-load policy is specified and tested |
| PEO-088 | TODO | Routing performance counters | Expanded nodes, path length, replans, and failure classes are visible for selected route |
| PEO-089 | TODO | Resident movement gate | One resident can be directed across the furnished room in every view while simulation coordinates do not rotate |

## First household loop (`PEO-090`–`PEO-109`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-090 | TODO | Energy motive | Documented units/decay/thresholds, fixed-tick changes, clamp behavior, and tests |
| PEO-091 | TODO | Bed sleep interaction | Route, reserve, enter sleep, increase energy over time, stop at policy threshold, release, complete |
| PEO-092 | TODO | Bed failure/cancel tests | Contention, blocked route, bed deletion, resident cancel, and interruption leave correct state/resources |
| PEO-093 | TODO | Hunger motive | Independent decay/threshold/utility response and deterministic tests |
| PEO-094 | TODO | Refrigerator eat interaction | Route/reserve, cost or inventory rule, timed eating, hunger gain, cleanup, and failure reasons work end-to-end |
| PEO-095 | TODO | Bladder motive | Independent decay/threshold/utility response and deterministic tests |
| PEO-096 | TODO | Toilet interaction | Route/reserve/use/relief/flush timeline, object occupancy, cancel cleanup, and completion work |
| PEO-097 | TODO | Interaction advertisements | Bed/fridge/toilet publish eligibility, expected motive effects, duration/cost, and slot without motive-specific scheduler code |
| PEO-098 | TODO | Utility score decomposition | Deficit, benefit, route/time/cost/contention, context, personality placeholder, and seeded variation are individually inspectable |
| PEO-099 | TODO | Autonomous scheduler | Idle resident periodically/event-selects eligible advertised action with stable seed and reason trace |
| PEO-100 | TODO | Autonomy hysteresis | Cooldown/commitment prevents rapid action thrashing while critical priorities can interrupt at allowed points |
| PEO-101 | TODO | Mood aggregate | Nonlinear derived wellbeing reflects critical motive and is tested without duplicating an external formula |
| PEO-102 | TODO | Comfort/ease motive | Seating/bed quality can change it; units and effects are original and tested |
| PEO-103 | TODO | Hygiene motive | Decay and object effects support warning/critical state with deterministic tests |
| PEO-104 | TODO | Shower interaction | Route/reserve/use/cleanliness/object water state/cancel cleanup increase hygiene |
| PEO-105 | TODO | Fun/enjoyment motive | Entertainment advertisements and decay work with original curve |
| PEO-106 | TODO | Entertainment interaction | Original radio/tabletop placeholder raises fun, reserves capacity correctly, and can be interrupted |
| PEO-107 | TODO | Social/connection and environment motives | Remaining baseline motive storage/decay/derived environment hooks are defined and tested |
| PEO-108 | TODO | Time controls UI | Pause/1x/2x/3x visibly reflect fixed clock; build/buy pause policy is explicit |
| PEO-109 | TODO | M3/M4 household gate | Displayed build proves directed sleep then seeded autonomous bed/fridge/toilet survival with three motives and traces |

## Build, buy, UI, and economy (`PEO-110`–`PEO-129`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-110 | TODO | Mode state machine | Live/Build/Buy transitions, input ownership, pause policy, and cancel behavior are explicit/tested |
| PEO-111 | TODO | Original HUD shell | Selected resident, clock/speed, funds, mode controls, motives, and action queue use an original developer-readable layout |
| PEO-112 | TODO | Build wall tool | Drag/click produces validated reversible wall commands and clear preview/cost |
| PEO-113 | TODO | Build floor tool | Area placement/removal uses reversible transactions and room dirtiness |
| PEO-114 | TODO | Bulldoze tool | Resolves exact target/cost/refund and honors deletion cleanup without broad destructive selection |
| PEO-115 | TODO | Household funds | Integer smallest-unit balance, no float money, insufficient-funds result, and deterministic tests |
| PEO-116 | TODO | Transaction ledger | Purchase/refund/build/income/bill entries record reason/source/tick and atomically update balance |
| PEO-117 | TODO | Buy placement transaction | Catalog selection, preview, rotate, commit, debit, cancel, and initial refund policy work end-to-end |
| PEO-118 | TODO | Catalog UI | Data-driven categories/items, price/stat text, selection, scrolling, and disabled affordability state |
| PEO-119 | TODO | Object sale/refund | Owned instance removal and refund are one transaction; occupied/in-use policy is explicit |
| PEO-120 | TODO | Counter and surface placement | Counter validates footprint/surface slots and supports refrigerator meal workflow prerequisite |
| PEO-121 | TODO | Basic meal preparation | Refrigerator/counter/chair/table timeline creates/carries/eats/disposes an original meal placeholder |
| PEO-122 | TODO | Trash and cleaning | Meal waste becomes a cleanable/disposable object affecting environment |
| PEO-123 | TODO | Environment score | Room decoration/cleanliness/broken/crowding inputs produce inspectable original score and motive modifier |
| PEO-124 | TODO | Personality axes | Small original axis set modifies cleaning/activity/social/ambition utility with controlled tests |
| PEO-125 | TODO | Small-house authored content | One bedroom, bathroom, kitchen, and living space use only owned procedural/data content |
| PEO-126 | TODO | Settings model | Window/display/audio/input preferences use explicit defaults and version-ready serialization |
| PEO-127 | TODO | Input mapping | Actions rather than hard-coded gameplay checks map keyboard/mouse controls and can later persist bindings |
| PEO-128 | TODO | Accessibility baseline | UI scaling, pause-anytime, color-independent validity cues, and readable debug text are documented/tested where possible |
| PEO-129 | TODO | M5 build/buy sub-gate | Player can buy/rotate/place/sell essential objects and build floor/walls with correct money in a playable house |

## Social simulation (`PEO-130`–`PEO-149`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-130 | TODO | Multiple residents | Household can own at least two independently routed/queued residents without ID/occupancy collisions |
| PEO-131 | TODO | Directional relationships | A-to-B and B-to-A values differ, clamp/decay policy is explicit, and save-ready lookup is tested |
| PEO-132 | TODO | Social eligibility | Target availability, relationship/mood/personality/context requirements return structured reasons |
| PEO-133 | TODO | Paired reservation/synchronization | Both residents route/reserve/enter/leave one social timeline atomically and recover from either-side failure |
| PEO-134 | TODO | Conversation interaction | Original talk interaction changes connection and directional relationship with inspectable outcome |
| PEO-135 | TODO | Positive social interaction | Joke/encourage-style action has acceptance and both-participant feedback without copied text/animation |
| PEO-136 | TODO | Negative social interaction | Disagree/argue-style action has contextual outcome and directional relationship effects |
| PEO-137 | TODO | Social autonomy | Advertised resident interactions enter generalized utility candidate set with repetition/context penalties |
| PEO-138 | TODO | Social action queue UI | Both participants show linked action and cancellation/interrupt ownership clearly |
| PEO-139 | TODO | Visitors | Non-household resident can enter/leave via lot portal and participates under explicit permissions |
| PEO-140 | TODO | Privacy context | Room/relationship/occupancy provides a generic privacy predicate for selected interactions |
| PEO-141 | TODO | Relationship milestones | Friendship-like flags derive from original thresholds/events and never replace directional raw state |
| PEO-142 | TODO | Recent social memory | Bounded tagged events influence short-term repetition/acceptance and serialize deterministically |
| PEO-143 | DEFERRED | Romance | Add only after adult social basics, original consent/context rules, and content review |
| PEO-144 | DEFERRED | Family graph | Parent/sibling/partner semantics wait for household/life-stage roadmap |
| PEO-145 | TODO | Social debug inspector | Displays directional values, recent events, eligibility, synchronized state, and utility components |
| PEO-149 | TODO | Social milestone gate | Two residents autonomously and directly converse with tested synchronization and directional outcomes |

## Skills, careers, schedules, and bills (`PEO-150`–`PEO-169`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-150 | TODO | Skill definition/progress | Stable data definitions, XP/level curve, clamping, and deterministic gain tests |
| PEO-151 | TODO | Cooking skill effect | Meal speed/quality/success visibly changes with skill using original constants |
| PEO-152 | TODO | Mechanical skill effect | Repair success/speed and accident probability integrate object state and seeded random |
| PEO-153 | TODO | Creativity/logic/fitness/charisma definitions | Data exists only with at least one working interaction or career use per skill |
| PEO-154 | TODO | Career data model | Levels, schedules, salary, requirements, performance, and promotion keys validate externally |
| PEO-155 | TODO | Simulation calendar | Day/time/week progression, schedule queries, speed/pause, and save round trips are deterministic |
| PEO-156 | TODO | Work departure | Resident receives preparation cue, routes to lot portal, enters off-lot state, and vacates occupancy |
| PEO-157 | TODO | Abstract work duration | Off-lot resident advances fixed schedule/performance without active-lot rendering/actions |
| PEO-158 | TODO | Work return and salary | Resident returns via portal; one ledger income posts exactly once with trace |
| PEO-159 | TODO | Career performance | Mood, attendance, skills, and bounded event variation produce inspectable original performance |
| PEO-160 | TODO | Promotion/demotion | Requirements and transitions apply once, update pay/schedule, and produce original notification text |
| PEO-161 | TODO | Household bills | Periodic amount is transparent, posts to ledger, has due/late state, and never double-charges |
| PEO-162 | TODO | Bill payment interaction | Resident/player can pay through an original household object/UI with funds and failure handling |
| PEO-163 | TODO | Object maintenance/repair | Broken state disables relevant interactions and advertises skill-influenced repair |
| PEO-164 | TODO | Cleaning autonomy | Personality, room cleanliness, motive/context, and advertised cleaning utility choose sensible tasks |
| PEO-165 | TODO | Career/skill UI | Selected resident view presents job, schedule, performance, requirements, skills, and progress originally |
| PEO-169 | TODO | M6 progression gate | Two-resident household attends work, earns salary, pays bills, gains a skill, and can promote deterministically |

## Data-driven content and PeopleBehavior (`PEO-170`–`PEO-189`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-170 | TODO | Current CNA data-format decision | Inspect actual `cnanext` CNJ/content support, compare minimal JSON alternative, document choice with working read test |
| PEO-171 | TODO | External object schema v1 | Chair/bed definitions load with IDs/catalog/footprint/rotations/sprites/slots/stats; invalid fields report paths |
| PEO-172 | TODO | External catalog schema | Categories and localized display keys load without recompiling core; duplicate IDs fail deterministically |
| PEO-173 | TODO | Animation metadata | Direction/clip/frame/duration/anchor/event markers validate and drive placeholder walk/interaction |
| PEO-174 | TODO | Sound metadata | Event IDs, variants, volume/range/category, and provenance references validate without requiring audio files yet |
| PEO-175 | TODO | Asset manifest schema | Files, hashes, origin/tool/date/prompt/process/license/reviewer state validate; unapproved shipping references fail |
| PEO-176 | TODO | Native primitive inventory | Working interactions identify shared operations and explicit semantics before VM design freezes |
| PEO-177 | TODO | PeopleBehavior sequence format | Deterministic validated linear sequence expresses at least one existing interaction with trace parity |
| PEO-178 | TODO | Route/reserve/release/wait primitives | Each has explicit result, timeout, cleanup, step trace, and unit/integration tests |
| PEO-179 | TODO | Animation/motive/state/money primitives | Typed operations synchronize timeline events and reject invalid targets/data |
| PEO-180 | TODO | Branch/random/test primitives | Seeded weighted choice and typed conditions are deterministic with bounded control flow |
| PEO-181 | TODO | Call/return and budgets | Validated call graph rejects recursion/cycles policy violations; per-tick instruction budget prevents hangs |
| PEO-182 | TODO | Behavior serialization | Program identity/version, PC, locals, waits, and reservations restore only at declared safe points |
| PEO-183 | TODO | Behavior trace viewer | Resident/interaction shows numbered primitive, inputs, result, elapsed ticks, and terminal cleanup |
| PEO-184 | TODO | Native-to-data parity migration | Bed plus one multi-stage interaction run through data with deterministic result parity tests |
| PEO-185 | TODO | Content namespace/override policy | Core/user IDs, dependency/load order, conflicts, and safe override behavior are explicit and tested |
| PEO-186 | TODO | Hot reload for developer data | Valid definition changes reload at safe boundary or clearly require lot restart; invalid reload preserves old state |
| PEO-187 | TODO | First coherent furniture set | Essential small-house objects share art scale, prices, stats, interactions, descriptions, and provenance |
| PEO-188 | TODO | Content author guide | One new simple chair can be added without core compilation and passes validation/test checklist |
| PEO-189 | TODO | Data-content gate | Clean build loads validated external essential content and traces at least two PeopleBehavior interactions |

## Persistence (`PEO-190`–`PEO-209`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-190 | TODO | Save schema/version header | Magic/type/version/build/content IDs are explicit; invalid/future versions fail safely |
| PEO-191 | TODO | Lot save/load | Dimensions, terrain, floor coverings, walls, doors/windows round-trip exactly |
| PEO-192 | TODO | Object persistence | Definition ID, stable instance ID, transform, typed state, ownership/container links round-trip |
| PEO-193 | TODO | Resident persistence | Identity, household, position, motives, personality, skills, career, and safe action state round-trip |
| PEO-194 | TODO | Relationship persistence | Directional pairs and recent bounded events round-trip without duplicate/asymmetry loss |
| PEO-195 | TODO | Household/economy persistence | Funds, ledger checkpoint, residents, lot, bills, and progression round-trip without double posting |
| PEO-196 | TODO | Clock/random persistence | Calendar/tick/speed policy and required random stream state resume deterministically |
| PEO-197 | TODO | Reservation/load repair | Invalid owners/targets and unsafe active interactions cancel predictably without leaked occupancy |
| PEO-198 | TODO | Transactional save write | Temporary write/validate/flush/replace preserves prior save on injected failure where platform permits |
| PEO-199 | TODO | Schema validation diagnostics | Corrupt/missing/wrong-type/unknown-required data identifies logical path and does not partially load |
| PEO-200 | TODO | Migration framework | Ordered pure migrations and fixture tests upgrade oldest supported version to current |
| PEO-201 | TODO | Settings persistence | Separate versioned user settings tolerate missing/new fields and do not contaminate world determinism |
| PEO-202 | TODO | Save slots UI | Original slot list/new/overwrite/delete-confirm/load/error flow avoids destructive ambiguity |
| PEO-203 | TODO | Autosave policy | Safe cadence/rotation/retention avoids saving mid-transaction and reports failure without blocking play |
| PEO-204 | TODO | Deterministic replay seed fixture | Save/reload followed by scripted ticks produces matching resident/object/money digest |
| PEO-209 | TODO | M5 persistence gate | Full small household saves, reloads, and continues autonomous loop with equivalent state and no duplicate effects |

## Neighborhood and households (`PEO-210`–`PEO-229`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-210 | TODO | Neighborhood model | Original Juniper Vale data owns stable lot/household IDs and summary state without loading all lots |
| PEO-211 | TODO | Multiple households | Create/load/save at least two households independently with no active-state leakage |
| PEO-212 | TODO | Lot selection UI | Original neighborhood presentation selects owned/empty lots and reports household summary |
| PEO-213 | TODO | Active-lot transition | Current lot saves/unloads, target loads, clock/household ownership remains valid, and failures preserve prior state |
| PEO-214 | TODO | Coarse off-lot clock | Inactive households advance only declared schedule/economy events, not full object interactions |
| PEO-215 | TODO | Off-lot career simulation | Attendance/salary/performance events match active-lot abstractions without double-processing on activation |
| PEO-216 | TODO | Household switching | Player switches households through explicit save boundary and selected resident/UI resets correctly |
| PEO-217 | TODO | Empty lot purchase/move | Funds/ownership/residents transfer transactionally with rollback on failure |
| PEO-218 | TODO | Visitor source/return | Off-lot residents can visit active lot and return with relationship/event updates safely summarized |
| PEO-219 | TODO | Neighborhood save schema | Index plus per-lot/household data supports partial load, validation, and migration |
| PEO-220 | TODO | Background progression budget | Coarse updates have bounded work proportional to due events, with counters and deterministic order |
| PEO-229 | TODO | M7 neighborhood gate | Two households/lots switch, work/progress off-lot, save/reload, and remain deterministic |

## Audio, assets, polish, and performance (`PEO-230`–`PEO-269`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-230 | TODO | CNA audio capability spike | Actual `cnanext` public audio API is inspected with original/CC0 test tone and blocker recorded if needed |
| PEO-231 | TODO | Audio event service | Simulation emits stable sound events; presentation resolves variants without altering deterministic simulation |
| PEO-232 | TODO | Original UI sound set | Owned/permissive sounds have provenance, consistent loudness, and no identifiable imitation |
| PEO-233 | TODO | Household ambience | Original layered ambient event policy reacts to room/object context with performance limits |
| PEO-234 | TODO | Original music system | Newly created tracks/loops with provenance support mode/context transitions and user volume controls |
| PEO-235 | TODO | Particle/effect sprites | 2D-only original effects are pooled, layered/sorted explicitly, and never become simulation truth |
| PEO-236 | TODO | Atlas packing | Deterministic tooling packs reviewed sprites, preserves anchors/metadata, and detects overlap/hash drift |
| PEO-237 | TODO | Static lot chunk cache | Measured static architecture/object layers cache by dirty chunk without stale visuals after edits/rotation |
| PEO-238 | TODO | Spatial interaction index | Autonomy candidate query scales by nearby definitions/regions and matches exhaustive oracle results |
| PEO-239 | TODO | Pathfinding optimization | Profile identifies bottleneck; cache/hierarchy improves measured workload without changing oracle paths |
| PEO-240 | TODO | Large-lot benchmark | Repro seed with eight residents/hundreds objects reports tick/render/query/path/sort budgets and hardware context |
| PEO-241 | TODO | Multi-renderer validation | At least two materially different CNA renderers build/run core 2D slice; differences/blockers documented |
| PEO-242 | TODO | UI polish pass | Original visual system, spacing, typography, icons, focus/hover/disabled states, and scaling are coherent |
| PEO-243 | TODO | Localization-ready strings | User-facing text uses stable keys, plural/format arguments, fallback, and no copied catalog prose |
| PEO-244 | TODO | Error/recovery UX | Missing content/save/version/device errors give actionable safe choices and never suggest proprietary data |
| PEO-250 | TODO | Asset directory/manifest bootstrap | Prompt/source/generated/processed/Content layout and one procedural asset record pass validator |
| PEO-251 | TODO | Object render rig script | Optional headless Blender script emits four People-v1 views, anchors, and metadata reproducibly |
| PEO-252 | TODO | Sprite processing tool | Trim/normalize/downsample/alpha checks preserve contact anchors and produce deterministic hashes |
| PEO-253 | TODO | Asset provenance validator | Missing rights/tool/date/prompt/process/results/reviewer fields fail with paths; shipping scan is clean |
| PEO-254 | TODO | Character sheet validator | Four directions, required clips/frames, duration, canvas, and anchor consistency are checked |
| PEO-255 | TODO | Original character art phase B | At least one reviewed owned full-body idle/walk set replaces procedural resident without code changes |
| PEO-256 | TODO | Layered-avatar feasibility test | Measured alignment/draw/atlas/art burden leads to documented compose-or-flatten choice |
| PEO-260 | TODO | First 50-object content plan | Categories/interaction reuse/art batches/tests/provenance define coherent quality gate, not raw count |
| PEO-261 | DEFERRED | 100-object milestone | Enabled only after first 50 pass consistency/performance/playtest gate |
| PEO-262 | DEFERRED | 250-object milestone | Enabled only after schema/tooling proves contributor-scale authoring |
| PEO-263 | DEFERRED | 500-object milestone | Long-term content target; consistency and interaction value remain acceptance gates |

## Release, portability, and deferred expansion (`PEO-270`–`PEO-299`)

| ID | Status | Task | Acceptance criteria |
|---|---|---|---|
| PEO-270 | TODO | Clean dependency pins | Release candidate records clean reviewed CNA/sharp-runtime commits and reproducible checkout instructions |
| PEO-271 | TODO | License/notice packaging | People MIT, CNA Ms-PL, sharp-runtime MIT, transitive notices, and asset attributions ship correctly |
| PEO-272 | TODO | Name/trademark clearance | Qualified review clears or replaces People/setting/brands before serious public/commercial release |
| PEO-273 | TODO | Proprietary-data absence audit | Source/history/build/package/content scan finds no original commercial data, loaders, names, or undocumented assets |
| PEO-274 | TODO | Linux release gate | Clean configure/build/tests/displayed runtime/package run on documented toolchain |
| PEO-275 | TODO | Windows release gate | Native or approved cross-build plus real Windows runtime verifies CNA renderer/input/audio/save paths |
| PEO-276 | TODO | macOS release gate | Apple build and real runtime verify supported CNA path without claiming unrun coverage |
| PEO-277 | TODO | Web feasibility gate | Only after desktop slice: assess Emscripten renderer/content/save/input limitations with actual build |
| PEO-278 | TODO | Mod content safety model | Namespaced data packages, validation, dependency/version rules, and no arbitrary native plugins |
| PEO-279 | TODO | Public alpha checklist | M5 or later gate, known issues, save compatibility, notices, asset rights, crash/log path, and feedback channel complete |
| PEO-280 | DEFERRED | Aging/life stages | Adult household simulation and persistence must be strong first |
| PEO-281 | DEFERRED | Children/family formation | Requires reviewed life-stage, social, safety, and content designs |
| PEO-282 | DEFERRED | Death/inheritance | Requires mature persistence, household transition, tone, and content design |
| PEO-283 | DEFERRED | Fire/emergency services | Requires hazards, routing/reservations, failures, services, effects, and audio |
| PEO-284 | DEFERRED | Pets | Requires separate agent needs/routing/animations/content budget |
| PEO-285 | DEFERRED | Weather | Requires exterior/room model, sprite effects, schedules, assets, and performance validation |
| PEO-286 | DEFERRED | Community lots | Requires neighborhood transitions, visitors, schedules, and lot-type content |
| PEO-287 | DEFERRED | Businesses/services | Requires economy/visitors/off-lot progression and robust content behavior |
| PEO-288 | DEFERRED | Pools/gardening/outdoor systems | Requires mature build mode, routing, object states, seasons decision, and bespoke art |
| PEO-289 | DEFERRED | Parties/procedural events | Requires stable multi-resident social autonomy and event persistence |

## Blocker ledger

No confirmed framework blocker exists yet. Add entries only after a minimal
reproduction demonstrates actual behavior.

| ID | Status | Summary | Reproduction/evidence | Workaround |
|---|---|---|---|---|
| PEO-CNA-001 | — | Reserved for first confirmed CNA issue | — | — |
| PEO-SR-001 | — | Reserved for first confirmed sharp-runtime issue | — | — |

## Immediate execution queue

1. Commit the verified bootstrap milestone as
   `Add initial isometric People skeleton` on `develop`.
2. Complete `PEO-020` deterministic render keys without changing the working
   presentation.
3. Implement `PEO-030`–`PEO-035`, then visible walls and objects toward M2.
4. Preserve a runnable build after every coherent commit.
