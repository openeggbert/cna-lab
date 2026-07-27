# Iron Shadows Technical and Product Analysis

## Document status

- Project name: **Iron Shadows** (provisional and intentionally easy to rename)
- Fictional prototype city: **Iron City**
- Repository purpose: establish an original C++ open-city action-adventure foundation on CNA and sharp-runtime
- Primary content-authoring path: Mesh Craft / MC3, with glTF/GLB as interchange and CNA CNJ as a runtime model format
- Current implementation stage: playable technical scaffold, not a production game
- License of original repository code: MIT
- Engine dependency stack: sharp-runtime → CNA → cna-extended → Iron Shadows (see section 5)
- Locked scope philosophy: full Mafia 1 (2002) **content** scope (multiple districts plus countryside, roughly 15-20 missions), built and simulated at Mafia 1-era **system fidelity** — not a from-scratch second engine, not GTA/modern-open-world-scale simulation. See section 1 and the conclusion.
- Performance target: see `docs/performance-targets.md` (roughly 2-4 GB RAM / 512 MB-1 GB VRAM, dual- to quad-core)

## 1. Executive summary

Iron Shadows is technically feasible as an original historical open-city action-adventure using the supplied CNA, sharp-runtime, and Mesh Craft repositories. The available code already covers a meaningful portion of the low-level platform:

- CNA provides a real game loop, input, 2D and 3D graphics abstractions, vertex/index buffers, textures, render targets, effects, models, skeletal animation support, audio, media playback, multiple graphics backends, and conversion tooling.
- sharp-runtime provides broad `System.*`-style C++ facilities that are useful for file access, serialization, collections, threading, tasks, networking, XML, JSON, time, diagnostics, and other non-rendering infrastructure.
- Mesh Craft and MC3 provide a construction-oriented scene language with primitives, extrusions, materials, object hierarchies, definitions and instances, CSG, collision metadata, animation data, areas, actions, triggers, Lua integration, and glTF/GLB export.

The missing part is not “a graphics API,” and a second inspection changed the picture further: CNA itself already ships more than an earlier pass assumed — working `PbrEffect`/`SkinnedPbrEffect` materials, shadow-mapping examples, GPU instancing across four backends, and a post-processing example — and the sibling repository `cna-extended` (a C++ port of MonoGame.Extended plus an additive `World3DEXT` layer) already provides an ECS, a `Transform3` scene hierarchy, 3D collision/octree broadphase, and single-clip skinned-model playback. Neither replaces the other; cna-extended detects and links the `CNA` target a consuming project already defines. The real remaining investment is a much smaller **integration and game layer**, not a second engine:

- wiring CNA's existing materials/shadows/instancing/post-processing into the game instead of redesigning them;
- wiring cna-extended's ECS/scene-hierarchy/collision instead of rewriting that boilerplate;
- production vehicle and player/character controllers;
- animation blending/transitions and inverse kinematics (cna-extended only gives single-clip playback, not blend trees or state machines — this piece is still real, scoped-down work);
- pedestrian, traffic, and police AI — at **Mafia 1 (2002) fidelity**, not modern open-world fidelity (simple lane-following and signal compliance, not intersection-reservation deadlock avoidance; a witnessed offense escalates to one pursuit level, not multi-tier wanted stars with search-area decay; sidewalk waypoint navigation for a modest number of nearby pedestrians, not statistical unloaded-population simulation);
- missions, dialogue, cutscenes, checkpoints, and save migration;
- navigation data and road/lane graphs;
- scalable asset packaging and licensing provenance;
- a small amount of tooling beyond Mesh Craft itself (mainly a runtime debug/mission-state view — not a second, bespoke editor suite), automated validation, and a manual (not procedural) content-production workflow in Mesh Craft/MC3. Mesh Craft is not just an XML/CLI tool: it is already a real 3D scene editor (Dear ImGui viewport, orbit camera, gizmos, CSG, extrude-along-path, scene hierarchy/properties panels, a first-person Walk Mode with collision, and a bounded live preview of area/trigger/timer event bindings), so spatial content is authored visually in it, not hand-typed.

The correct strategy is to keep CNA general-purpose and XNA-compatible, use CNA's own existing modern-rendering facilities and cna-extended's scene/ECS facilities directly, and only add a new "CNA EXT" type when a genuine gap remains after that — not design a new engine layer speculatively. The repository created with this analysis follows that separation.

The world is built as **discrete districts/chapters connected by loading screens** (the actual Mafia 1 structure), not a single seamless streamed map — this removes the need for sector-streaming machinery in the first slices. The first production proof should not be an entire city. It should be one complete vertical slice containing one district's worth of city blocks, one drivable vehicle, one accessible interior, several characters, a dialogue, an in-engine cutscene, a short mission, a save checkpoint, and the complete MC3-to-runtime asset path. The intended endpoint after repeating that slice is the full campaign scope described in section 23, not a permanently tiny demo.

## 2. Original identity and legal separation

Iron Shadows must be an original work. A broad genre or high-level premise is not enough to create infringement by itself, but copying protected expression can. The project should not reproduce another game's distinctive story, characters, missions, map, dialogue, cinematography, logos, music, models, textures, vehicle branding, user interface, or other recognizable content.

The project should therefore preserve the following rules from its first commit:

1. Use an original title, fictional city, organizations, characters, plot, and chronology.
2. Do not use “Mafia,” another game's subtitle, or confusingly similar branding in the shipped title.
3. Do not reproduce mission order, scene staging, dialogue, or map geometry from a reference game.
4. Treat other games as genre and usability references, not source material.
5. Record the origin and license of every non-original asset.
6. Do not assume that “free download” means commercial use, modification, source redistribution, or AI transformation is permitted.
7. Keep evidence of license terms and acquisition dates in the asset registry.
8. Prefer original, commissioned, public-domain, CC0, or clearly permissive assets.
9. Use fictional vehicle manufacturers and avoid trademarked badges unless permission is clear.
10. Perform a dedicated legal/name search before the final public announcement and commercial release.

The provisional title **Iron Shadows** was selected because it suggests an industrial historical atmosphere without copying another product's identity. The setting name **Iron City** is similarly provisional. Both are centralized in documentation and data so they can be changed later.

An earlier internal candidate, **Iron Lantern**, was deliberately abandoned during preliminary name checking because it already had prominent unrelated uses. This illustrates why every title in this repository is a replaceable working name and why a professional trademark, company-name, store, domain, and product search remains a release gate.

## 3. Inspected source repositories

This analysis was grounded in the supplied source archives rather than only in a conceptual engine design.

### 3.1 CNA observations

The inspected CNA tree uses CMake and C++23. Its current build contract expects sharp-runtime as a sibling repository. The EasyGL backend also expects an EasyGL sibling repository. CNA exposes backend selection through `CNA_GRAPHICS_BACKEND`, including EasyGL, Vulkan, software, headless, SDL renderer, bgfx, WebGPU, Direct3D variants, Canvas, ASCII, and other backends present in the tree.

The inspected public API includes, among other relevant facilities:

- `Game`, `GameTime`, `GameWindow`, `GraphicsDeviceManager`, and `GraphicsDevice`;
- keyboard, mouse, gamepad, touch, joystick, haptics, clipboard, text-input, sensor, and power APIs;
- `VertexBuffer`, `IndexBuffer`, dynamic buffers, vertex declarations, and common vertex layouts;
- textures, cube textures, 3D textures, render targets, sampler, blend, rasterizer, and depth/stencil state;
- `BasicEffect`, `AlphaTestEffect`, `DualTextureEffect`, `EnvironmentMapEffect`, `SkinnedEffect`, `PbrEffect`, and `SkinnedPbrEffect`;
- generic shader/effect objects and effect techniques, passes, parameters, and annotations;
- `Model`, model meshes, mesh parts, bones, model effect collections, and bounding primitives;
- `AnimationPlayer`, `SkinnedModelEXT`, skeletal data, and skinned vertex layouts;
- occlusion queries and instanced draw paths;
- sound effects, dynamic sound, audio listeners and emitters, XACT-style abstractions, media songs, video, and video playback;
- content readers and model/content tooling;
- a glTF/GLB-to-CNJ conversion utility in the build tree.

CNA's own examples demonstrate direct 3D rendering with vertex and index buffers, `BasicEffect`, matrices, depth testing, keyboard input, and indexed primitives. The Iron Shadows scaffold deliberately follows these real interfaces.

CNA's example/shader-test tree goes further than section 6.3 of an earlier revision of this document assumed: it contains working shadow-mapping examples (`easygl_shadowmapping_createshadowmap_shader_test.cpp`, `..._drawwithshadowmap_shader_test.cpp`), GPU instancing tests across EasyGL, D3D9, Vulkan, and WebGPU, and a post-processing effect example. Treat these as the starting point for Iron Shadows rendering, not as a reason to design a parallel "CNA EXT" material/shadow/instancing system from scratch.

### 3.2 sharp-runtime observations

The inspected sharp-runtime source provides a broad C++ implementation of familiar .NET-style APIs. Relevant namespaces include:

- `System.Collections`, generic collections, concurrent collections, immutable collections, and specialized collections;
- `System.IO`, streams, files, directories, paths, archives, compression, hashing, and watchers;
- `System.Text`, encodings, regular expressions, and JSON;
- `System.Xml`, LINQ to XML, readers, writers, and XPath;
- `System.Threading`, channels, cancellation, synchronization, and tasks;
- `System.Net`, HTTP, sockets, WebSockets, DNS, endpoints, and network information;
- time, date, diagnostics, cryptography, globalization, reflection-like/runtime support, and numerics.

Iron Shadows currently uses `System::IO::File` and `System::IO::Directory` in its save path, proving that sharp-runtime is integrated as a real dependency rather than merely named in CMake.

sharp-runtime should remain infrastructure, not become the game engine itself. It is appropriate for data, I/O, tasks, and utility layers. Rendering, physics, entity simulation, mission semantics, and navigation should remain explicit Iron Shadows/CNA systems.

### 3.3 Mesh Craft / MC3 observations

The supplied Mesh Craft tree includes the MC3 schema and conversion path. MC3 version 0.3 supports construction-oriented scene data suitable for AI and procedural generation. Relevant capabilities include:

- analytic primitives such as boxes, spheres, cylinders, cones, and planes;
- external mesh references;
- extrusions using multiple cross-section and path forms;
- groups, definitions, instances, pivots, transforms, tags, visibility, and metadata;
- PBR-oriented materials and texture references;
- cameras, environment, ambient/directional/point/spot lights;
- CSG union, difference, and intersection for supported geometry;
- collision roles and collision metadata;
- invisible logical areas and interaction zones;
- animation clips and channels for supported properties;
- states, actions, event bindings, timers, triggers, and Lua scripting facilities;
- glTF/GLB export through `mc3togltf`;
- a compact MCB representation in the broader project.

The included Iron Shadows MC3 prototype was validated against the supplied `mc3.xsd`. It represents a road crossing, several buildings, a warehouse, a mission marker, lights, PBR material data, static collision metadata, and trigger metadata.

Mesh Craft itself (the application, distinct from the MC3 data format) is a real C++23 3D scene editor, not just a schema/CLI toolchain: a Dear ImGui application with a live 3D viewport, orbit camera, transform gizmos, a scene hierarchy panel and properties panel, CSG boolean operations, extrude-along-path authoring, an animation timeline, autosave/recovery, glTF/GLB import as bounded editable MC3 content, and a first-person **Walk Mode** that exercises walk-collision proxies directly in the editor. It also has a bounded **Preview/Play** mode that dispatches timer, Walk Mode area-enter/exit, and picked-object click event bindings through an isolated, resource-limited Lua VM for authoring feedback — this is editor-side authoring convenience, not the shipped game's runtime scripting (Iron Shadows' own mission scripting, per group 24, is deliberately simpler: engine-evaluated condition/action expressions, not a general embedded VM). Practically, this means Iron Shadows' spatial content — buildings, roads, interiors, props, collision, and mission trigger/marker areas — is authored by placing and manipulating objects directly in Mesh Craft's viewport, not by hand-editing MC3 XML text.

Mesh Craft should be treated as an authoring system for hand-built content. Iron Shadows content (buildings, roads, interiors) is manually authored in MC3/Blender rather than produced by procedural generators; Mesh Craft's preview/action runner is useful for authoring feedback but should not silently become the complete production game runtime, and building a generator toolchain on top of it is explicitly out of scope for now (see section 7.3). Production mission, physics, streaming, and AI semantics should be owned by the game layer.

### 3.4 cna-extended observations

`cna-extended` is a sibling repository (`../cna-extended`), wired into Iron Shadows' `CMakeLists.txt` and linked into `iron_shadows_core`. It is a C++23 port of MonoGame.Extended (2D collision/quadtree, tweening, screens, sprite/tilemap rendering, 2D particles, an Artemis-style ECS) plus an additive, less battle-tested `World3DEXT` layer: a `Transform3` scene-graph hierarchy, 3D collision shapes with octree broadphase, `ModelComponentEXT`/`SkinnedModelComponentEXT` rendering components, and a single-clip skinned-animation driver (`AnimationSystem3DEXT`) that advances playback and recomputes bone transforms with no blending or state machine.

It is **not** a material/PBR/shadow/instancing/post-processing system — none of that exists in cna-extended, and it does not need to, because CNA already provides it (section 3.1). Its own `CMakeLists.txt` detects an existing `CNA` target from a parent project and links against it rather than building its own copy, which is exactly how Iron Shadows consumes it. Use it for the ECS, scene hierarchy, 3D collision/octree, and skinned-model playback boilerplate a small team should not rewrite from scratch; still plan to add real animation blending/transitions/IK as scoped Iron Shadows work, since neither CNA nor cna-extended provides it today.

## 4. Current repository implementation

The created repository is intentionally more than a directory skeleton. It currently implements a small deterministic prototype:

- a CNA `Game` subclass and update/draw loop;
- a procedural colored-box 3D renderer using CNA vertex/index buffers and `BasicEffect`;
- a small Iron City city-block layout;
- simple XZ-axis AABB/circle collision against buildings;
- a third-person on-foot controller with walk, sprint, strafe, and turn;
- a kinematic sedan controller with acceleration, reverse, steering, drag, handbrake, and collision response;
- enter/exit interaction;
- a three-line prologue dialogue;
- a mission state machine from briefing through warehouse delivery;
- save/load implemented through sharp-runtime file and directory APIs;
- smoke-run command-line support;
- core tests for world collision, vehicle motion, dialogue, mission progression, and save round trips;
- an MC3 source scene and MC3 → GLB → CNJ build script;
- `cna-extended` linked as a sibling dependency (auto-detecting and linking the `CNA` target this project defines), verified end to end with a full `compile-software` build and passing core tests;
- documentation, licensing policy, formatting configuration, CMake presets, helper scripts, and a large plan.

The procedural renderer is temporary. Its job is to provide an immediate place to test player motion, camera behavior, mission logic, and vehicle controls before the production asset loader is complete.

## 5. Recommended architecture

The dependency direction should remain simple and enforceable:

```text
sharp-runtime
      ↓
     CNA (materials, shadows, instancing, post-processing already available)
      ↓
cna-extended (ECS, Transform3 hierarchy, 3D collision/octree, skinned-model playback)
      ↓
Iron Shadows reusable game systems
      ↓
Iron Shadows content and campaign
```

A future workspace may split the reusable layer into its own repository, but it is premature to create too many repositories before subsystem boundaries stabilize. Within the current repository, target-level and directory-level separation should be used first. Only add a new "CNA EXT" layer for a capability that is genuinely missing from both CNA and cna-extended after checking both.

### 5.1 Layer responsibilities

#### sharp-runtime

- file and directory access;
- JSON/XML support;
- collections and utility types where they provide clear value;
- tasks, cancellation, and worker coordination;
- networking only where required;
- compression, hashing, diagnostics, and platform-neutral helpers.

#### CNA core

- XNA-compatible application and graphics abstractions;
- graphics resources and draw submission;
- input, audio, media, windowing, content, and platform services;
- compatibility-focused behavior and cross-backend contracts.

#### cna-extended

- ECS (`World`/`Entity`/`ComponentManager`);
- `Transform3` scene hierarchy;
- 3D collision shapes and octree broadphase;
- skinned-model rendering components and single-clip animation playback.

#### CNA EXT (only if a real gap remains)

- PBR materials/effects, shadows, instancing, and post-processing are already available directly from CNA (section 3.1) — use them first;
- ECS, scene hierarchy, and 3D collision/octree are already available from cna-extended (section 3.4) — use them first;
- a genuinely missing, genuinely reusable capability (e.g. animation blend trees/state machines, which neither CNA nor cna-extended provides) is the only thing that should turn into a new CNA EXT type, and only after confirming at least two realistic consumers or a clear framework-level contract.

#### Iron Shadows engine layer

- entities/components or another clearly defined scene-object model;
- world partitioning and streaming;
- physics integration and collision ownership;
- vehicles, characters, cameras, AI, missions, dialogue, cutscenes, and save games;
- asset registry, runtime package loading, content versioning, and gameplay tools.

#### Iron Shadows content layer

- Iron City map and districts;
- characters, organizations, vehicles, missions, dialogue, cinematics, audio, textures, and models;
- localization and legal metadata.

### 5.2 Suggested module structure

```text
Application
Assets
Audio
Characters
Cinematics
Core
Debug
Dialogue
Entities
Gameplay
Graphics
Input
Localization
Missions
Navigation
Persistence
Physics
Platform
Scripting
Traffic
Vehicles
World
```

Each module should expose a narrow public interface and keep implementation detail private. Game state should not be globally reachable from every system. Long-lived systems should be assembled in an explicit composition root.

## 6. Graphics strategy

### 6.1 Prototype backends

For the first playable slice, development should focus on a small backend set:

1. EasyGL as the primary development backend when its sibling repository and dependencies are available.
2. Vulkan as a modern independent validation backend.
3. Software or headless as deterministic compile/test paths where supported.
4. Direct3D 11 later for Windows production confidence.

The game should not block its first mission on perfect behavior across all planned CNA backends. The broader CNA backend matrix is valuable, but game-level production support must be explicitly tiered.

### 6.2 Rendering milestones

The rendering target is deliberately restrained, matching `docs/performance-targets.md` (roughly 2-4 GB RAM, 512 MB-1 GB VRAM): baked lightmaps for buildings, one dynamic sun, a handful of important dynamic lights, and limited/simple shadows — not SSAO, SSR, volumetric fog, or cascaded shadows everywhere. A practical progression is:

1. procedural debug geometry;
2. static CNJ models and materials;
3. texture loading and PBR material assignment via CNA's existing `PbrEffect`;
4. baked lightmaps plus one dynamic sun and CNA's existing shadow-mapping path, kept limited in range/resolution;
5. instancing for repeated city props, using CNA's existing instancing path;
6. LOD groups and frustum culling;
7. skinned characters using cna-extended's skinned-model components;
8. restrained post-processing (fog, color grading) using CNA's existing post-processing example as a starting point;
9. profiling and backend parity.

A historical city does not require every modern effect to look convincing. Strong art direction, stable shadows, fog, restrained PBR materials, ambient audio, color grading, and coherent lighting are higher priorities than feature-count maximalism. Decals, particles, weather, SSAO, SSR, and volumetric effects are explicitly deferred past v1 (see section 40-equivalent post-slice research in the plan), not designed in now.

### 6.3 CNA EXT recommendations

Do not design a new "CNA EXT" material/scene/animation/shadow/instancing/post-processing layer from scratch. Section 3.1 and 3.4 establish that the pieces that layer would have provided already exist:

- materials, shadows, instancing, post-processing → integrate directly from CNA (`PbrEffect`, `SkinnedPbrEffect`, its shadow-mapping and instancing examples, its post-processing example);
- ECS, scene hierarchy, 3D collision/octree, skinned-model playback → integrate directly from cna-extended.

The only remaining candidate for a genuinely new CNA EXT type is animation blending/transitions/state-machine support, since neither CNA nor cna-extended provides it. Even that should only move into CNA EXT once it is genuinely general-purpose and has at least two realistic consumers or a clear framework-level contract — Iron City mission logic, police AI, and vehicle gameplay must never enter CNA EXT.

## 7. Asset pipeline

### 7.1 Source and runtime formats

No single format should be forced to serve every phase.

#### MC3

Use as an editable, AI-friendly source format for:

- buildings and interiors;
- roads, sidewalks, signs, street furniture, and procedural scene composition;
- constructional prefabs;
- collision and logical metadata;
- areas, tags, and authoring-time actions.

#### glTF/GLB

Use as an interchange and inspection format for:

- Blender and third-party tools;
- AI model output;
- skeletal characters and animations;
- debugging conversion output;
- external validation.

#### CNJ

Use as a CNA-oriented model representation for:

- meshes and mesh parts;
- materials and textures;
- skeletons and animation clips where supported;
- CNA-ready buffers and sidecar binaries.

#### MCB or a future `.cnapack`

Use for complete runtime scene/package data that must preserve more than renderable triangles:

- object identities and hierarchy;
- instance tables;
- collision proxies;
- areas and triggers;
- navigation and road-graph references;
- stream-sector metadata;
- authored actions that remain meaningful at runtime;
- version and dependency metadata.

### 7.2 Recommended conversion path

```text
MC3 source + textures + metadata
        ├── mc3togltf ──→ GLB inspection/interchange
        ├── CNA glTF tool ──→ CNJ render model
        └── scene compiler ──→ runtime metadata/package
```

A simple MC3 → GLB → CNJ path can lose MC3-specific semantics that glTF does not represent. Therefore render geometry and gameplay metadata should either travel in parallel or be compiled by a dedicated scene compiler.

### 7.3 Content authoring: manual first, AI as a co-author

Manual hand-authoring in Mesh Craft/MC3 and Blender is the primary content path for v1, including at the full multi-district-plus-countryside campaign scope in section 23. Building a procedural building/road/interior generator toolchain (auto-generating city content from rules or grammars) is explicitly out of scope for now — it is a real idea, parked as post-slice research, not a v1 investment, because it is exactly the kind of tooling investment a small team authoring a bounded, hand-designed campaign does not need.

AI's role here is as a co-author inside that manual process (drafting a building variant, suggesting dialogue lines, proposing a floor plan to hand-adjust in MC3), not as an autonomous generator system. Every asset — AI-assisted or not — must still pass deterministic validation gates:

- legal source and license check;
- schema/format validation;
- scale, axis, pivot, and naming validation;
- triangle, vertex, material, texture, and bone budgets;
- non-manifold and degenerate geometry checks;
- UV and texture-reference checks;
- collision-proxy checks;
- skeleton and animation convention checks;
- runtime load test;
- provenance record and content hash.

If procedural generation is revisited later, the scalable hierarchy to grow into would be:

```text
small components → prefabs → buildings/road modules → blocks → sectors → districts → world assembly
```

## 8. World structure and district loading

Iron City is **not** a single seamless streamed map. Following Mafia 1's actual structure, the world is a set of discrete districts/chapters connected by loading screens — the player finishes a mission or reaches a transition point, a loading screen runs, and the next district (or the countryside, or an interior-scale district) becomes the active scene. This removes the need for seamless-sector streaming, fast-travel bulk-streaming, and similar always-on-world machinery from the design entirely.

A practical hierarchy within one loaded district is still useful for organizing content and keeping a single district's load affordable:

```text
District (fully loaded while active)
 └── Block
      ├── static geometry
      ├── instances
      ├── collision
      ├── navigation
      ├── road/lane graph
      ├── ambient zones
      └── interactive entities
```

Within the active district, the game can still separate logical simulation range, physics range, render range, and audio range around the player (e.g. full detail near the player, lower LOD/AI frequency further away) — but this is a same-district level-of-detail policy, not cross-district streaming. Loading the next district can afford a visible loading screen, so resource loading only needs to be reasonably fast and give clear progress feedback; it does not need background streaming, cancellation, or priority-changing machinery designed for a player who never stops moving through the world.

## 9. Roads, traffic, and navigation

Road rendering and road logic should be separate outputs from a shared road graph.

### 9.1 Road graph

The graph should model:

- nodes and directed segments;
- lane centerlines and widths;
- permitted directions and turns;
- speed limits;
- intersections and stop lines;
- signals, signs, and priority rules;
- crossings, parking slots, spawn points, and service access;
- surface type and condition;
- links to visual road meshes.

### 9.2 Visual output

From the graph, tools can generate:

- road surfaces;
- curbs and sidewalks;
- lane markings;
- crossings and traffic islands;
- signs and signals;
- tram or rail elements where needed;
- collision and navigation boundaries.

### 9.3 Pedestrian navigation

Pedestrians should use sidewalk/navigation data rather than vehicle lanes. The first version can use a waypoint graph; a navmesh and local avoidance can follow. Interior navigation must connect through explicit portals to exterior navigation.

### 9.4 Traffic AI

Traffic AI is scoped to **Mafia 1 (2002) fidelity**, deliberately, not to modern open-world fidelity:

- spline/lane following;
- distance keeping;
- signal compliance;
- obstacle braking;
- route selection from a lane graph;
- despawn/respawn outside the player's attention.

Explicitly out of scope for now: intersection-reservation deadlock avoidance, overtaking/passing AI, parking maneuvers, emergency yielding, accident recovery, and public transit. These are real modern-open-world features, not gaps in Mafia 1's own traffic — they should stay parked as post-slice research rather than v1 work.

## 10. Physics

CNA and MC3 expose rendering and collision-related data, but the inspected stack is not a complete production 3D rigid-body simulation for an open-city game. A mature external physics library should be integrated rather than reimplemented.

### 10.1 Recommended candidate

Jolt Physics is a strong candidate because it offers modern C++, rigid bodies, collision queries, sensors, character support, constraints, vehicles, and multithreaded design. Bullet or another maintained library remains possible after a focused prototype comparison.

The integration should hide the selected library behind Iron Shadows interfaces:

```text
PhysicsWorld
RigidBodyHandle
CharacterBody
VehiclePhysics
CollisionShape
PhysicsMaterial
QueryFilter
RaycastResult
TriggerEvent
```

This prevents physics-library types from leaking into mission, character, or asset code.

### 10.2 MC3 collision mapping

MC3 collision metadata can compile to runtime bodies:

- `static` → static rigid body or static compound;
- `dynamic` → dynamic rigid body;
- `kinematic` → engine-driven body;
- `trigger` → sensor/trigger volume;
- `none` → no physics body.

Production data must also support shape selection, layer, mask, material, mass, center of mass, convex decomposition, and simplified collision proxies.

## 11. Vehicles

The current vehicle is a useful kinematic gameplay test but not production vehicle physics. A staged vehicle roadmap is recommended.

### 11.1 Stage one: controllable prototype

- acceleration and reverse;
- speed-dependent steering;
- drag and handbrake;
- simple collision;
- camera and enter/exit interaction.

This stage is implemented in the scaffold.

### 11.2 Stage two: raycast vehicle

- chassis rigid body;
- four wheel contacts;
- suspension travel, spring, and damping;
- tire longitudinal/lateral grip;
- engine torque curve;
- gearbox, final drive, brakes, and steering geometry;
- stable low-speed behavior;
- tunable assists.

### 11.3 Stage three: historical vehicle character

- manual and automatic control modes;
- clutch and gear-shift timing;
- different drivetrains;
- surface-dependent grip;
- damage states and wheel failure;
- doors, occupants, lights, horn, and instruments;
- engine, exhaust, tire, suspension, impact, and cabin audio;
- AI control using the same vehicle interface.

The target should be believable weight and readable handling rather than simulation complexity for its own sake.

## 12. Characters and animation

CNA already contains the foundations for skinned models and animation clips, and cna-extended's `ModelComponentEXT`/`SkinnedModelComponentEXT` plus `AnimationSystem3DEXT` already provide single-clip skinned-model rendering and playback (advancing playback and recomputing bone transforms). What is still missing — in both CNA and cna-extended — and remains real, scoped Iron Shadows work is blending and transition logic on top of that playback:

- clip blending and transition rules;
- locomotion blend spaces;
- layered animation and bone masks;
- additive poses;
- root motion;
- synchronized events and notifies;
- inverse kinematics for feet, hands, steering wheels, and look-at;
- entry/exit alignment with vehicles;
- facial or jaw animation;
- animation LOD and update throttling;
- deterministic state restoration for checkpoints.

All human characters should use a controlled skeleton convention wherever possible. Shared skeletons enable animation reuse and reduce content costs. Imported AI-generated characters should be retargeted and validated rather than allowed to define arbitrary incompatible rigs.

Mesh Craft is most valuable for constructed environments and props. Organic character creation should generally use an AI/Blender/retarget/glTF pipeline, with MC3 placing or referencing the resulting character assets in scenes.

## 13. Missions, dialogue, and cutscenes

### 13.1 Mission system

A mission should be a data-driven state graph with explicit conditions, actions, checkpoint boundaries, failure reasons, and versioned persistence. Critical engine logic remains in C++; mission composition can use declarative data and constrained scripting.

A mission runtime needs:

- state and objective graphs;
- world/area triggers;
- entity references that survive streaming;
- timers and counters;
- spawn/despawn actions;
- dialogue and cinematic actions;
- failure/retry/checkpoint handling;
- save-compatible variables;
- debug visualization and forced transitions;
- validation for unreachable or ambiguous states.

### 13.2 Dialogue system

The current dialogue reader proves flow but should evolve to include:

- stable IDs;
- speakers and entity binding;
- localized text keys (ship one language first, but every line uses a stable key from day one so a second language can be added later without touching the dialogue system itself);
- voice assets and durations;
- subtitles and accessibility options;
- branching choices and conditions;
- interruption and resumption rules;
- facial/jaw events;
- mission actions;
- conversation cameras;
- history and replay support.

### 13.3 Cutscene system

Cutscenes should normally be rendered in-engine and represented as a sequence with independent tracks:

- camera;
- character/vehicle animation;
- transform and property tracks;
- dialogue and subtitles;
- sound and music;
- events and mission state;
- fades and post-processing;
- visibility and spawn control.

The sequence player must support skip behavior, checkpoint-safe finalization, missing-asset handling, and deterministic restoration. Skipping a cinematic must apply all required terminal gameplay state rather than merely stopping playback.

## 14. AI simulation

The city should use simulation levels rather than running every NPC at full complexity.

### 14.1 Suggested AI levels

- **Tier 0:** nearby important actors with full perception, animation, navigation, interaction, and combat;
- **Tier 1:** nearby ambient NPCs with simplified goals and reduced perception;
- **Tier 2:** distant sector agents with low-frequency logical movement;
- **Tier 3:** unloaded population represented by schedules and statistical state.

### 14.2 Pedestrian behavior

Pedestrian AI is scoped to **Mafia 1 fidelity**: roughly 10-20 simulated pedestrians near the player at once, not a citywide demographic simulation. Initial pedestrian behavior should include:

- sidewalk waypoint navigation;
- local avoidance;
- idle points and short routines;
- basic reactions to vehicles, horns, collisions, weapons, and police (mainly fleeing);
- safe despawn and rehydration;
- animation state integration.

Explicitly out of scope for now: statistical unloaded-population simulation and density budgets keyed to time-of-day/weather. Those model a living city at a scale Mafia 1 itself never simulated; park them as post-slice research.

### 14.3 Police and wanted response

Police behavior is scoped to **Mafia 1 fidelity**, not a GTA-style tiered wanted system:

- offense detection and witnesses (a witnessed traffic offense or crime is what triggers a response, matching Mafia 1's own rule);
- a chase with a single escalation level once triggered;
- arrest, surrender, or escape ends the encounter;
- rules preventing impossible omniscience (police only react to what a witness could plausibly have seen).

Explicitly out of scope for now: multi-tier wanted-star severity and decay, dispatch/search-area simulation, roadblocks, and detective/investigation persistence — that is GTA-style depth, parked as post-slice research (see plan group 40), not needed for a Mafia-1-faithful game. This is a later milestone relative to core gameplay; it should not delay the first mission slice.

## 15. Audio and music

CNA already offers low-level and media audio capabilities. Iron Shadows needs a game-level audio graph:

- master, music, dialogue, ambience, vehicle, effects, and UI buses;
- spatial emitters and listeners;
- attenuation and occlusion policy;
- ambient zones and transitions;
- footstep surface mapping;
- vehicle engine layers and cabin/exterior mixes;
- dialogue ducking;
- mission music states and transitions;
- radio station scheduling;
- streaming voice/music budgets;
- subtitles and accessibility synchronization.

A vehicle sound should be assembled from multiple layers rather than one loop: idle, load, RPM bands, exhaust, gear changes, tires, suspension, impacts, and interior filtering.

## 16. Save games and persistence

The current text save is deliberately transparent and testable. Production persistence should add:

- format versioning and migrations;
- atomic write and backup rotation;
- checksums and corruption detection;
- profile slots;
- world/mission/entity stable IDs;
- streamed-sector persistence;
- settings separate from campaign state;
- checkpoint snapshots;
- asynchronous thumbnail capture;
- clear compatibility policy between builds;
- tests for old versions and interrupted writes.

Save data must store logical state rather than raw pointers, addresses, backend resources, or transient physics handles.

## 17. Tools and production workflow

Iron Shadows deliberately does **not** build a second, bespoke editor suite on top of what already exists (no separate road/lane-graph editor, mission-graph editor, or cinematic timeline editor). Spatial content authoring already has a real tool — Mesh Craft, a genuine 3D scene editor with a viewport, gizmos, CSG, Walk Mode, and event-binding preview (section 3.3) — so building a second one would duplicate it. Non-spatial content (mission state graphs, dialogue) is hand-written JSON/XML/text data, which is tractable at the campaign scope in section 23 without a visual graph editor. Building a bespoke editor suite beyond Mesh Craft is exactly the kind of investment a small team should not make; it is parked as post-slice research, not v1 work.

What is still worth having, kept small and script-shaped rather than as GUI applications:

- an asset registry and provenance scanner (a validation script, per section 18);
- an MC3 schema validator (already exists: `scripts/validate-mc3.sh`);
- a conversion orchestrator/cache for the MC3 → GLB → CNJ path;
- at most one small debug/mission-state view — e.g. a text or overlay view of the current mission's state graph and objective — to make mission debugging tractable without a full mission-graph editor.

Tools should consume the same schemas as the game. Avoid creating editor-only representations that cannot be round-tripped or validated in CI.

## 18. Licensing and asset provenance

Every external asset must have a record containing at least:

```text
asset_id
source URL or source repository
original author/organization
license identifier and saved license evidence
acquisition date
commercial-use status
modification permission
redistribution permission
attribution requirement
AI-processing permission where relevant
local source path
content hash
reviewer and review status
```

A generated `THIRD_PARTY_ASSETS.md` should be produced from the registry for every release. The build should reject unknown production assets rather than silently ship them.

## 19. Performance strategy

Performance budgets must be defined before the city grows. Early targets should cover:

- CPU frame time by subsystem;
- GPU frame time by pass;
- draw calls and state changes;
- triangles/vertices by distance tier;
- texture and buffer memory;
- streaming bandwidth and pending work;
- active physics bodies and contacts;
- active characters and AI update rates;
- audio voices and streaming decoders;
- save size and checkpoint time.

Optimization priorities are likely to include instancing, sector batching, LOD, visibility, texture compression, asynchronous loading, AI throttling, collision simplification, and data-oriented hot loops. Backend micro-optimization should follow profiling rather than speculation.

## 20. Testing and quality

The repository should maintain several test layers:

1. unit tests for deterministic math, state machines, parsers, and serializers;
2. integration tests for asset conversion, save migration, physics queries, and mission flows;
3. headless or software smoke tests for startup and fixed-frame execution;
4. rendering reference tests on selected backends;
5. scenario tests using recorded inputs;
6. long-running soak tests for streaming, traffic, and save/load;
7. fuzz/property tests for untrusted data and schema parsers;
8. platform/build matrix checks.

Tests should distinguish framework/backend defects from game defects. Repro scenes should be minimal and stored with expected outputs where possible.

## 21. Current validation status

The generated project was validated against the supplied repositories as follows:

- all Iron Shadows `.cpp` translation units passed a C++23 syntax-only compile using the real CNA and sharp-runtime headers and software-backend definitions;
- the sample MC3 scene passed validation against the supplied Mesh Craft `mc3.xsd`;
- sharp-runtime configured and built successfully as a static library in a persistent validation build directory;
- the Iron Shadows core test executable was linked from the real project sources, CNA math/color implementation, and the built sharp-runtime library, and all tests passed;
- a standalone sharp-runtime file I/O smoke program wrote, read, and removed a file successfully;
- in this workspace, the full Iron Shadows `compile-software` preset configured and built end to end (780 targets, `-j4`, ccache), including `cna-extended` linking against the parent-provided `CNA` target, and `ctest` passed; see `docs/validation.md` for the current record. Earlier revisions of this document reported an incomplete CNA-linked build in a different validation environment with empty vendored SDL submodules — that limitation does not apply here.

The `dev-easygl`/`dev-vulkan` presets (real rendering backends) have not yet been build-verified in this workspace; only `compile-software` has been exercised end to end so far.

## 22. Risks and mitigations

### Risk: engine work overwhelms game production

Mitigation: define a vertical slice and reject framework work that does not unblock it.

### Risk: supporting every CNA backend delays the game

Mitigation: maintain explicit production, validation, experimental, historical, and diagnostic tiers.

### Risk: AI creates large amounts of unusable content

Mitigation: schema checks, budgets, provenance, automated conversion tests, and human art-direction review.

### Risk: MC3 semantics are lost through glTF

Mitigation: compile runtime metadata in parallel or implement a direct MC3/MCB scene package compiler.

### Risk: a second district reveals district-loading assumptions too late

Mitigation: build a second, genuinely different district (not just a bigger copy of the first) before assuming the loading-screen/district-boundary approach scales, even though it avoids full sector-streaming machinery.

### Risk: the full 15-20 mission, multi-district-plus-countryside campaign scope overwhelms a small team

Mitigation: build one district end to end first (section 23's vertical slice) and only then repeat that proven slice outward. Track scope against the plan's milestone gates rather than starting many districts/missions in parallel.

### Risk: reusing cna-extended's less battle-tested 3D layer surfaces bugs late

Mitigation: cna-extended's 2D port is well-trodden (MonoGame.Extended heritage); its `World3DEXT` addition is newer and less proven. Exercise the ECS/Transform3/collision/skinned-playback paths Iron Shadows actually needs early and with real content, rather than assuming they are as solid as the 2D core.

### Risk: save files depend on transient object addresses

Mitigation: stable IDs, explicit serialization contracts, migrations, and deterministic rehydration.

### Risk: vehicle physics consumes unlimited tuning time

Mitigation: ship a stable assisted raycast model before advanced simulation features.

### Risk: legal uncertainty around downloaded assets

Mitigation: allow-list licenses, save evidence, record hashes, and fail release builds on missing provenance.

### Risk: dependency archives omit required submodules

Mitigation: document recursive checkout requirements, add dependency preflight scripts, and pin known-good revisions.

## 23. Recommended vertical slice

The first decisive milestone should contain:

- approximately one 300×300 meter district slice;
- four to six streets and one intersection system;
- one garage, bar, warehouse, and small accessible interior;
- one player character with a shared production skeleton;
- one production-path vehicle;
- a handful of pedestrians and traffic vehicles;
- one dialogue encounter;
- one in-engine cutscene;
- one mission with a checkpoint and failure/retry behavior;
- one police or pursuit reaction in simplified form;
- MC3-authored environment converted and loaded through the runtime pipeline;
- audio ambience, footsteps, vehicle layers, dialogue, and mission music;
- performance and memory budgets met on the primary backend, targeting `docs/performance-targets.md`.

This slice proves the complete production loop. Expanding the city before this loop works would multiply unfinished systems and content rework.

The locked target for Iron Shadows is the full Mafia 1-equivalent campaign scope: multiple districts plus a countryside area, roughly 15-20 missions, connected by loading screens (section 8). This vertical slice is not a permanently tiny demo; it is the first of several district-sized repetitions that add up to that campaign, each one reusing the same proven systems rather than inventing new ones per district.

## 24. Immediate priorities

The most valuable next steps are:

1. Build the repository from a recursive CNA checkout and execute the current prototype (done: `compile-software` preset builds and passes tests with `cna-extended` linked).
2. Add a dependency preflight script that gives precise missing-submodule messages (done: `scripts/preflight.sh` also checks `cna-extended`).
3. Load one generated CNJ building at runtime and replace its procedural counterpart (done: `assets/source/mc3/warehouse.mc3.xml` -> `mc3togltf` -> `cna_tool_gltf_to_cnj` -> `Content.Load<Model>()`, with a procedural-box fallback if the asset is missing; see `docs/validation.md`. Still open: `PbrEffect` materials instead of the default `BasicEffect`, and deriving collision from the MC3 `collision` attribute instead of the pre-existing procedural AABB — see `plan/plan_39-vertical-slice-gates.md` gate M2).
4. Load one generated CNJ vehicle body and preserve current controls (done: the sedan is authored as four single-object MC3 files -- body/cabin/windshield/wheel -- converted and loaded via `Content.Load<Model>()`, composed with Iron Shadows' own per-part transforms in `PrototypeRenderer`, with a procedural fallback if any part is missing; `VehicleController` and its driving controls are untouched. Along the way, confirmed and documented a real gap: `cna_tool_gltf_to_cnj` does not bake per-object node transforms into vertex data, so a multi-object MC3 scene cannot currently be loaded as one positioned CNJ model -- see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md` `IS-10-004b`).
5. Introduce a production asset registry and package manifest (registry started: `assets/licenses/asset-registry.csv` now tracks the warehouse MC3 source; the package-manifest half of this item is still open, see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md`).
6. Select and prototype a physics library behind an abstraction (done: Jolt Physics v5.6.0, MIT, pinned in `THIRD_PARTY.md`; `IronShadows::Physics::PhysicsWorld` in `include/`/`src/Physics/` hides Jolt's own types behind a PIMPL boundary; character, trigger, raycast, and 4-wheel vehicle prototypes are all proven in `tests/PhysicsTests.cpp`, gate M4. Also done: `PlayerController`'s on-foot movement and `VehicleController`'s driving are now physics-driven -- a `JPH::CharacterVirtual` capsule and a `JPH::VehicleConstraint`/`WheeledVehicleController` respectively, world geometry collision comes from real static bodies built from `PrototypeWorld`'s colliders plus a dedicated ground plane. Vehicle engine/transmission/suspension tuning still uses Jolt's own generic defaults, not a deliberately-tuned "feel" -- unverified visually since this repository has no display/interactive test access; see `NEXT.md`).
7. Create a second, genuinely different district to validate the district-loading approach (section 8), not a bigger single streamed sector.
8. Replace window-title objectives with a minimal SpriteBatch/SpriteFont HUD.
9. Replace the prototype text dialogue with versioned JSON/XML data and stable IDs, in one language first (section 13.2).
10. Add a small in-engine cinematic sequence that hands control back to the mission.

## 25. Conclusion

The supplied technology stack is a credible foundation for an original open-city game, and it is more complete than an earlier pass assumed: CNA already contains enough real 3D, animation, audio, media, input, and backend infrastructure — including materials, shadows, instancing, and post-processing — to avoid starting from zero, and `cna-extended` already supplies the ECS/scene-hierarchy/collision/skinned-playback boilerplate a small team should not rewrite. sharp-runtime is useful for portable infrastructure and has been integrated into the scaffold. Mesh Craft/MC3 is the primary, manually-authored content path; AI assists as a co-author within that process rather than as an autonomous generator.

The guiding philosophy for Iron Shadows is: **full Mafia 1 (2002) content scope, at Mafia 1-era system fidelity — not a second engine, not modern-open-world-scale simulation.** Concretely, that means the full campaign target (multiple districts plus countryside, roughly 15-20 missions, section 23) built on discrete district loading rather than seamless streaming (section 8), traffic/pedestrian/police AI tuned to what Mafia 1 itself actually simulated rather than GTA-scale systems (sections 9.4, 14.2, 14.3), materials/shadows/instancing/post-processing integrated from CNA rather than redesigned (section 6.3), and content authored by hand in Mesh Craft/MC3 rather than generated by bespoke tooling (section 7.3, section 17). Over-simplifying past that point would make the game "Mafia lite," which is explicitly not the goal; over-building past it would repeat the mistake of designing a second engine or a modern open-world simulation nobody asked for.

The decisive engineering task is to integrate what already exists (CNA, cna-extended, Mesh Craft) into a disciplined game layer, not to endlessly expand low-level APIs before a mission exists. Iron Shadows should grow through vertical slices: each slice must integrate content creation, conversion, rendering, collision, gameplay, audio, mission state, and persistence, at the fidelity level locked in this document. The current repository is the first such scaffold and is designed to be replaced incrementally rather than discarded.
