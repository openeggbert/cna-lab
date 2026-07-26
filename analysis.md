# Iron Shadows Technical and Product Analysis

## Document status

- Project name: **Iron Shadows** (provisional and intentionally easy to rename)
- Fictional prototype city: **Iron City**
- Repository purpose: establish an original C++ open-city action-adventure foundation on CNA and sharp-runtime
- Primary content-authoring path: Mesh Craft / MC3, with glTF/GLB as interchange and CNA CNJ as a runtime model format
- Current implementation stage: playable technical scaffold, not a production game
- License of original repository code: MIT

## 1. Executive summary

Iron Shadows is technically feasible as an original historical open-city action-adventure using the supplied CNA, sharp-runtime, and Mesh Craft repositories. The available code already covers a meaningful portion of the low-level platform:

- CNA provides a real game loop, input, 2D and 3D graphics abstractions, vertex/index buffers, textures, render targets, effects, models, skeletal animation support, audio, media playback, multiple graphics backends, and conversion tooling.
- sharp-runtime provides broad `System.*`-style C++ facilities that are useful for file access, serialization, collections, threading, tasks, networking, XML, JSON, time, diagnostics, and other non-rendering infrastructure.
- Mesh Craft and MC3 provide a construction-oriented scene language with primitives, extrusions, materials, object hierarchies, definitions and instances, CSG, collision metadata, animation data, areas, actions, triggers, Lua integration, and glTF/GLB export.

The missing part is not “a graphics API.” The main missing investment is a reusable **open-city game layer above CNA**:

- world partitioning and streaming;
- production physics integration;
- vehicle simulation;
- player and character controllers;
- animation graphs and inverse kinematics;
- pedestrian, traffic, police, and combat AI;
- missions, dialogue, cutscenes, checkpoints, and save migration;
- navigation data and road/lane graphs;
- scalable asset packaging and licensing provenance;
- profiling, tools, automated validation, and content-production workflows.

The correct strategy is therefore to keep CNA general-purpose and XNA-compatible, place advanced rendering capabilities in CNA EXT where appropriate, and build Iron Shadows as a separate game and game-engine layer. The repository created with this analysis follows that separation.

The first production proof should not be an entire city. It should be one complete vertical slice containing one city block, one drivable vehicle, one accessible interior, several characters, a dialogue, an in-engine cutscene, a short mission, a save checkpoint, and the complete MC3-to-runtime asset path.

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

Mesh Craft should be treated as an authoring and procedural-content system. Its preview/action runner is useful for authoring feedback but should not silently become the complete production game runtime. Production mission, physics, streaming, and AI semantics should be owned by the game layer.

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
- documentation, licensing policy, formatting configuration, CMake presets, helper scripts, and a large plan.

The procedural renderer is temporary. Its job is to provide an immediate place to test player motion, camera behavior, mission logic, and vehicle controls before the production asset loader is complete.

## 5. Recommended architecture

The dependency direction should remain simple and enforceable:

```text
sharp-runtime
      ↓
     CNA + CNA EXT
      ↓
Iron Shadows reusable game systems
      ↓
Iron Shadows content and campaign
```

A future workspace may split the reusable layer into its own repository, but it is premature to create too many repositories before subsystem boundaries stabilize. Within the current repository, target-level and directory-level separation should be used first.

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

#### CNA EXT

- capabilities that are generally reusable but exceed XNA 4.0;
- PBR materials and effects;
- modern scene/model/animation helpers;
- instancing, LOD, advanced shadows, render graphs, post-processing, streaming-friendly resource APIs;
- explicit backend capability queries;
- reusable modern graphics features without game-specific story logic.

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

A practical progression is:

1. procedural debug geometry;
2. static CNJ models and materials;
3. texture loading and PBR material assignment;
4. sun and one shadow path;
5. instancing for repeated city props;
6. LOD groups and frustum culling;
7. skinned characters;
8. post-processing, fog, color grading, and atmosphere;
9. decals, particles, weather, and scalable local lights;
10. profiling and backend parity.

A historical city does not require every modern effect to look convincing. Strong art direction, stable shadows, fog, restrained PBR materials, ambient audio, color grading, and coherent lighting are higher priorities than feature-count maximalism.

### 6.3 CNA EXT recommendations

Candidate reusable types include:

```text
CNA::Framework::Graphics::EXT::Material
CNA::Framework::Graphics::EXT::MaterialInstance
CNA::Framework::Graphics::EXT::Scene
CNA::Framework::Graphics::EXT::SceneNode
CNA::Framework::Graphics::EXT::Mesh
CNA::Framework::Graphics::EXT::LodGroup
CNA::Framework::Graphics::EXT::InstanceBatch
CNA::Framework::Graphics::EXT::AnimationClip
CNA::Framework::Graphics::EXT::AnimationGraph
CNA::Framework::Graphics::EXT::ShadowRenderer
CNA::Framework::Graphics::EXT::PostProcessGraph
CNA::Framework::Graphics::EXT::ResourceUploadQueue
```

These should only move into CNA EXT when they are genuinely general-purpose and have at least two realistic consumers or a clear framework-level contract. Iron City mission logic, police AI, and vehicle gameplay must never enter CNA EXT.

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

### 7.3 AI-generated content

AI is well suited to generating large amounts of draft content, but every asset must pass deterministic validation. Recommended gates include:

- legal source and license check;
- schema/format validation;
- scale, axis, pivot, and naming validation;
- triangle, vertex, material, texture, and bone budgets;
- non-manifold and degenerate geometry checks;
- UV and texture-reference checks;
- collision-proxy checks;
- skeleton and animation convention checks;
- LOD generation and visual review;
- runtime load test;
- provenance record and content hash.

AI should generate reusable elements and generators before it generates a monolithic city. The scalable hierarchy is:

```text
small components → prefabs → buildings/road modules → blocks → sectors → districts → world assembly
```

## 8. World structure and streaming

The city must not be a single model or a single always-loaded scene. A practical hierarchy is:

```text
World
 └── Region
      └── District
           └── Sector
                ├── static geometry
                ├── instances
                ├── collision
                ├── navigation
                ├── traffic graph
                ├── ambient zones
                └── interactive entities
```

Initial sectors can be 128×128 or 256×256 meters, adjusted after profiling. The game should separate logical simulation range, physics range, render range, and audio range.

A possible first streaming policy is:

- near: full geometry, physics, characters, interactions, and high-frequency AI;
- medium: lower LOD, reduced animation/AI frequency, simplified audio;
- far: proxy geometry, skyline, simplified traffic representation;
- unloaded: persistent logical state only.

Resource loading should use workers for file parsing/decompression and a render-thread upload queue for GPU resources. Cancellation and priority changes must be designed early; otherwise driving quickly through the city will create stale work and memory spikes.

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

The first traffic implementation can be intentionally limited:

- spline/lane following;
- distance keeping;
- signal compliance;
- simple intersection reservation;
- obstacle braking;
- route selection from a lane graph;
- despawn/respawn outside the player's attention.

Complex passing, parking maneuvers, emergency yielding, and accident recovery should be added only after the core traffic flow is stable.

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

CNA already contains the foundations for skinned models and animation clips. A production game still needs a higher-level animation system:

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
- localized text keys;
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

Initial pedestrian behavior should include:

- spawn and destination selection;
- sidewalk navigation;
- local avoidance;
- idle points and short routines;
- reactions to vehicles, horns, collisions, weapons, and police;
- safe despawn and rehydration;
- animation state integration.

### 14.3 Police and wanted response

Police behavior should be layered:

- offense detection and witnesses;
- wanted severity and decay;
- dispatch and search areas;
- traffic stops;
- pursuit and road response;
- arrest, surrender, escape, and mission override states;
- rules preventing impossible omniscience.

This is a later milestone; it should not delay the first mission slice.

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

A project of this scale needs tools as first-class deliverables:

- asset registry and provenance scanner;
- MC3 schema validator;
- conversion orchestrator and cache;
- city/sector visualizer;
- road and lane graph editor;
- collision and trigger overlay;
- mission graph validator and debugger;
- dialogue/localization editor;
- cinematic timeline editor;
- navigation bake and debug view;
- performance capture and sector-memory report;
- deterministic replay/input capture;
- automated screenshot and scene test runner.

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
- the Iron Shadows full CMake configure reached CNA dependency processing but could not complete with the supplied CNA ZIP because its vendored SDL/SDL_image/SDL_mixer submodule directories are empty and no compatible system SDL3 package was available in the validation environment.

Therefore the repository is source-validated and its sharp-runtime integration has been executed, but the full CNA-linked executable was not falsely claimed as built or run. A normal CNA checkout with initialized submodules, plus EasyGL for the EasyGL preset, is required for the complete build.

## 22. Risks and mitigations

### Risk: engine work overwhelms game production

Mitigation: define a vertical slice and reject framework work that does not unblock it.

### Risk: supporting every CNA backend delays the game

Mitigation: maintain explicit production, validation, experimental, historical, and diagnostic tiers.

### Risk: AI creates large amounts of unusable content

Mitigation: schema checks, budgets, provenance, automated conversion tests, and human art-direction review.

### Risk: MC3 semantics are lost through glTF

Mitigation: compile runtime metadata in parallel or implement a direct MC3/MCB scene package compiler.

### Risk: city streaming is added too late

Mitigation: make the second map larger than one always-loaded sector and enforce sector ownership early.

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
- performance and memory budgets met on the primary backend.

This slice proves the complete production loop. Expanding the city before this loop works would multiply unfinished systems and content rework.

## 24. Immediate priorities

The most valuable next steps are:

1. Build the repository from a recursive CNA checkout and execute the current prototype.
2. Add a dependency preflight script that gives precise missing-submodule messages.
3. Load one generated CNJ building at runtime and replace its procedural counterpart.
4. Load one generated CNJ vehicle body and preserve current controls.
5. Introduce a production asset registry and package manifest.
6. Select and prototype a physics library behind an abstraction.
7. Create a second world sector to force streaming design.
8. Replace window-title objectives with a minimal SpriteBatch/SpriteFont HUD.
9. Replace the prototype text dialogue with versioned JSON/XML data and stable IDs.
10. Add a small in-engine cinematic sequence that hands control back to the mission.

## 25. Conclusion

The supplied technology stack is a credible foundation for an original open-city game. CNA already contains enough real 3D, animation, audio, media, input, and backend infrastructure to avoid starting from zero. sharp-runtime is useful for portable infrastructure and has been integrated into the scaffold. Mesh Craft/MC3 is particularly promising for AI-assisted constructional content, provided that it is surrounded by validation, packaging, and metadata-preservation tools.

The decisive engineering task is to build a disciplined game layer above CNA, not to endlessly expand low-level APIs before a mission exists. Iron Shadows should grow through vertical slices: each slice must integrate content creation, conversion, rendering, collision, gameplay, audio, mission state, and persistence. The current repository is the first such scaffold and is designed to be replaced incrementally rather than discarded.
