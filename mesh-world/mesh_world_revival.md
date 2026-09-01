# Mesh World Revival

**Status:** Strategic design document and source material for a future implementation plan  
**Primary repositories:** Mesh World, Mesh Craft, CNA  
**Document language:** English  
**Purpose:** Define the agreed long-term direction for reviving Mesh World as a visually rich, modular, AI-assisted, offline-first procedural world system built on MC3 and CNA.

---

## 1. Executive Summary

Mesh World should be revived around a new content architecture rather than around an ever-growing collection of isolated procedural Lua scripts.

The central idea is:

> Mesh World generates the structure of the world procedurally, but fills that structure with reusable MC3 assets, modules, prefabs, and style-controlled variants.

The world should not be one gigantic hand-authored MC3 scene, nor should every object be generated from scratch at runtime. Instead, Mesh World should combine:

- procedural macro-world generation,
- generated terrain and hydrology,
- modular MC3 asset libraries,
- reusable global definitions,
- style-aware variant selection,
- chunk-based composition,
- LOD and GPU instancing,
- offline AI-assisted content generation,
- and progressively more advanced rendering through CNA and NOXNA graphics extensions.

MC3 remains the core scene and model representation. A new semantic `mc3.json` format should be added as the preferred format for AI generation, automation, patching, schema validation, and tool-to-tool exchange. `mc3.xml` should remain fully supported as a readable, compatible, hand-editable format. Both formats must map to the same internal `Mc3Document` data model and support semantic round-tripping.

Global MC3 libraries should hold reusable definitions such as windows, doors, roofs, balconies, streetlights, cars, trees, cave modules, rocks, furniture, and architectural details. Individual buildings and scenes should reference those definitions instead of duplicating their full geometry. Each definition family should contain multiple coherent variants and LOD levels so that a large city does not repeat the same window, door, tree, or vehicle everywhere.

Lua should not be deleted immediately, but it should no longer be treated as the only or primary future content-generation path. Existing Lua generators may remain as migration tools, offline generators, compatibility layers, or specialized procedural tools. The long-term runtime path should favor a C++ world composer selecting and placing validated MC3 assets and modules.

The poor current visual result is not primarily caused by MC3. A detailed MC3 castle already demonstrates that the format and renderer can produce a much richer scene when they are given strong composition, many meaningful details, varied materials, shadows, lights, and a well-chosen camera. The main problem is that current Mesh World output often uses sparse, primitive generators and weak spatial composition. The revival must therefore improve content, layout, rendering, and performance together.

---

## 2. Current Problem Statement

Mesh World contains substantial infrastructure, but the visible output is much weaker than the underlying technical scope suggests.

Typical visible problems include:

- large empty ground areas,
- a few isolated rectangular buildings,
- weak street frontage,
- insufficient sidewalks, curbs, crossings, and parcel logic,
- repetitive or placeholder materials,
- limited facade hierarchy,
- simplistic roofs,
- sparse props and vegetation,
- poor camera framing,
- flat lighting,
- weak shadows or no visible contact grounding,
- little distinction between districts,
- and limited use of the capabilities already present in MC3.

The detailed MC3 castle scene proves an important point:

> MC3 can already represent a visually rich scene made from many reusable definitions, instances, materials, textures, lights, cameras, transparent surfaces, emissive elements, and transformed primitives.

Therefore, Mesh World should not be redesigned around the assumption that MC3 is inherently too weak. The main issue is the quality and density of the generated content, how it is composed, and how it is rendered.

The revival must avoid repeating the earlier failure mode where infrastructure expands while visible output remains almost unchanged.

---

## 3. Revival Vision

The long-term target is a world that can contain:

- cities and villages,
- roads and public infrastructure,
- forests and fields,
- mountains and cliffs,
- rivers, lakes, coastlines, and waterfalls,
- caves, mines, and underground complexes,
- interiors,
- ruins, castles, bridges, farms, factories, stations, and landmarks,
- vehicles and street furniture,
- biome-specific vegetation and rocks,
- and a large library of reusable objects generated or improved with AI.

The world should remain deterministic where needed for testing and cached generation, but the persistent world itself may continue to use its existing non-reproducible or stateful design where appropriate.

The world must be built from layers:

```text
macro world structure
    ↓
terrain, climate, hydrology, roads, settlements, cave graphs
    ↓
chunks and semantic placement regions
    ↓
MC3 modules, prefabs, definitions, and variants
    ↓
LOD selection, instancing, material selection, lighting, and rendering
```

MC3 assets are the building blocks of the world, not a replacement for world generation itself.

---

## 4. Core Architectural Decisions

### 4.1 MC3 remains the canonical scene/model system

MC3 should remain the main representation for authored and generated 3D content.

It should continue to represent:

- primitives,
- meshes,
- transforms,
- groups,
- definitions,
- instances,
- materials,
- textures,
- lights,
- cameras,
- animations,
- metadata,
- and scene composition.

The format should evolve where necessary, but it should not be abandoned merely because current Mesh World scenes are visually weak.

### 4.2 Add a semantic `mc3.json` format

A new `mc3.json` representation should be introduced.

This must not be a mechanical XML-to-JSON conversion using awkward fields such as `@name` or string-encoded vectors. It should be a semantic JSON representation designed for:

- AI structured output,
- JSON Schema validation,
- programmatic generation,
- patching,
- content databases,
- compact recipes,
- and tooling.

Example:

```json
{
  "format": "mc3",
  "version": "0.4",
  "model": "residential_house_014",
  "unit": "meter",
  "imports": [
    {
      "namespace": "windows",
      "source": "mc3lib://urban/windows@1.3.0"
    },
    {
      "namespace": "doors",
      "source": "mc3lib://urban/doors@1.1.0"
    }
  ],
  "objects": [
    {
      "id": "front_wall",
      "type": "box",
      "position": [0, 1.5, 0],
      "size": [10, 3, 0.3],
      "material": "facade.plaster.beige"
    },
    {
      "id": "front_window_01",
      "type": "instance",
      "definition": "windows:double_classic_03",
      "transform": {
        "position": [-2.5, 1.7, -0.18],
        "rotation": [0, 0, 0],
        "scale": [1, 1, 1]
      },
      "materialOverrides": {
        "frame": "paint.white",
        "glass": "glass.clear"
      }
    }
  ]
}
```

### 4.3 XML and JSON use one internal data model

There must not be two independent implementations of scene semantics.

Both formats must parse into the same internal structures:

```cpp
Mc3Document
Mc3Object
Mc3Definition
Mc3Material
Mc3Transform
Mc3Animation
Mc3Metadata
```

The pipeline should be:

```text
mc3.xml ──┐
          ├──> Mc3Document AST ──> validator / compiler / renderer
mc3.json ─┘
```

Writers should support:

```text
Mc3Document
    ├──> Mc3XmlWriter
    ├──> Mc3JsonWriter
    └──> Mc3BinaryWriter / MCB compiler
```

Round-tripping must preserve scene meaning, references, transforms, materials, metadata, and hierarchy. It does not need to preserve XML whitespace, attribute ordering, or comments exactly.

### 4.4 Global libraries of reusable definitions

MC3 definitions should be reusable across files through global libraries.

Examples:

```text
mc3lib://urban/windows
mc3lib://urban/doors
mc3lib://urban/roofs
mc3lib://urban/facades
mc3lib://urban/buildings
mc3lib://urban/vehicles
mc3lib://urban/street-furniture

mc3lib://nature/trees-temperate
mc3lib://nature/trees-conifer
mc3lib://nature/rocks
mc3lib://nature/cliffs
mc3lib://nature/forest-floor

mc3lib://caves/tunnels
mc3lib://caves/chambers
mc3lib://caves/props
mc3lib://caves/crystals

mc3lib://interiors/furniture
mc3lib://interiors/lighting
mc3lib://interiors/appliances
```

An individual house should reference definitions such as:

```text
windows:double_classic_03
doors:residential_wood_02
roofs:gable_clay_04
props:mailbox_01
furniture:dining_table_03
```

The house should not redefine the full geometry of every reusable component.

### 4.5 Definitions must have variants

Every major asset family should support coherent variants.

Examples:

```text
window.residential.double.classic_01
window.residential.double.classic_02
window.residential.double.modern_01
window.shopfront.large.urban_03

door.residential.wood_panel_01
door.residential.wood_glass_02
door.apartment.shared_entry_01

streetlamp.classic_01
streetlamp.modern_02

car.hatchback.compact_01
car.sedan.family_03
car.van.delivery_02
```

Variation must be style-controlled. A single house should not randomly use unrelated windows on every opening. Instead, it should select one compatible window family, one door family, one roof family, and a coherent material palette.

### 4.6 Lua is demoted, not immediately deleted

Lua generators should not be removed recklessly.

They may remain useful for:

- converting existing procedural generators into static assets,
- offline content generation,
- migration,
- specialized parametric objects,
- tests,
- and experimentation.

However, the future runtime composition path should favor:

```text
C++ world generator
    ↓
semantic world data
    ↓
asset and variant selection
    ↓
MC3 scene composition
    ↓
compiled runtime representation
```

The eventual removal of Lua from critical runtime paths must happen only after equivalent functionality exists and all active generators are migrated or intentionally retired.

### 4.7 World generation remains procedural and hybrid

The entire planet must not be treated as a collection of giant prefabs.

The recommended architecture is hybrid:

- macro world generation remains procedural,
- terrain is generated or streamed by chunk,
- MC3 assets represent reusable visible content,
- placement systems decide where assets belong,
- and larger structures are assembled from modules.

---

## 5. Asset Categories

The content system should distinguish four major asset levels.

### 5.1 Atomic assets

Small reusable objects:

- windows,
- doors,
- lamps,
- signs,
- chairs,
- tables,
- barrels,
- crates,
- trees,
- rocks,
- bollards,
- hydrants,
- road markings,
- roof vents,
- chimneys.

### 5.2 Modules

Parts designed to connect to other parts:

- facade bays,
- floor segments,
- wall corners,
- roof segments,
- bridge segments,
- tunnel sections,
- cave junctions,
- cliff sections,
- fence segments,
- road intersections,
- sidewalk corners.

### 5.3 Prefabs

Larger reusable assemblies:

- detached houses,
- row houses,
- apartment buildings,
- shops,
- barns,
- chapels,
- towers,
- bridges,
- cave rooms,
- mine entrances,
- small ruins,
- ponds,
- playgrounds.

### 5.4 Kits and style packs

Families of compatible parts:

- Central European residential kit,
- modern urban kit,
- industrial kit,
- medieval town kit,
- rural farm kit,
- temperate forest kit,
- alpine mountain kit,
- limestone cave kit,
- volcanic cave kit.

A kit should define compatibility rules, palette choices, preferred dimensions, and allowed combinations.

---

## 6. Asset Metadata Requirements

Every reusable definition should include enough metadata for automated placement and selection.

Recommended metadata:

- unique stable ID,
- category,
- subcategory,
- semantic tags,
- style tags,
- region and period tags,
- nominal size,
- exact bounding box,
- origin and pivot,
- front/up orientation,
- anchor points,
- sockets,
- material slots,
- collision shape or collision proxy,
- clearance volume,
- supported LOD levels,
- triangle count,
- object count,
- instancing eligibility,
- shadow policy,
- maximum visibility distance,
- rarity or selection weight,
- license and provenance,
- source generator or AI request hash,
- semantic version,
- content hash,
- validation status,
- thumbnail/preview reference.

Example:

```json
{
  "id": "window.residential.double.classic_03",
  "category": "window",
  "tags": [
    "residential",
    "double",
    "classic",
    "central-europe",
    "wood-frame"
  ],
  "nominalSize": [1.4, 1.6, 0.16],
  "facing": "-Z",
  "bounds": {
    "min": [-0.7, -0.8, -0.08],
    "max": [0.7, 0.8, 0.08]
  },
  "materialSlots": ["frame", "glass", "hardware"],
  "lods": {
    "near": "window.residential.double.classic_03.lod0",
    "medium": "window.residential.double.classic_03.lod1",
    "far": "window.residential.double.classic_03.lod2"
  }
}
```

Socket examples:

```json
{
  "sockets": {
    "wallAnchor": [0, 0, 0],
    "handle": [0.38, 1.0, -0.08],
    "interiorSide": [0, 0, 0.2],
    "exteriorSide": [0, 0, -0.2]
  }
}
```

Clearance metadata is particularly important for furniture, doors, roads, and vehicles.

---

## 7. Library Imports and Dependency Resolution

A source scene or model should be able to import global libraries.

Example XML concept:

```xml
<imports>
  <import namespace="city"
          source="mc3lib://city-core@3.2.1"
          hash="sha256:..."/>
</imports>
```

Example JSON concept:

```json
{
  "imports": [
    {
      "namespace": "city",
      "source": "mc3lib://city-core@3.2.1",
      "hash": "sha256:..."
    }
  ]
}
```

Instances may then reference:

```text
city:window.residential.double_04
city:streetlamp.classic_02
```

The resolver must support:

- namespaces,
- semantic versions,
- content hashes,
- dependency graphs,
- cycle detection,
- missing dependency errors,
- deterministic resolution,
- and optional version locking.

The compiler must include only used definitions and their recursive dependencies when creating a standalone asset.

Example:

```text
library contains 50,000 definitions
house uses 24 definitions
compiled standalone house includes only those 24 and their dependencies
```

---

## 8. Proposed File Types

Recommended content types:

```text
.mc3.json       semantic AI/tool authoring format
.mc3.xml        compatible readable scene/model format
.mc3lib.json    reusable definition library
.mc3lib.xml     optional readable library representation
.mcb            compiled binary runtime asset
.mc3pack        package containing libraries, textures, metadata, and previews
```

`mc3.json` should be preferred for AI and automation. `mc3.xml` should remain first-class and supported indefinitely.

---

## 9. AI-Assisted Content Factory

AI generation should be an offline development and content-production tool, not a runtime dependency of Mesh World.

### 9.1 Goals

The AI pipeline should generate:

- atomic asset definitions,
- modular kits,
- complete prefabs,
- building compositions,
- prop families,
- LOD variants,
- material variations,
- and selected landmark scenes.

### 9.2 Preferred output

AI should normally generate strict semantic `mc3.json`, not raw XML.

Reasons:

- structured output support,
- natural vector arrays,
- strong JSON Schema enforcement,
- easier patching,
- easier validation,
- simpler automated repair,
- and better integration with databases.

Direct XML generation may remain useful for human-readable examples and exceptional landmarks, but it should not be the default bulk-generation path.

### 9.3 AI request contract

The model should receive:

1. a compact MC3 generation contract,
2. the relevant JSON Schema,
3. only the relevant subset of the asset catalogue,
4. dimensional and style constraints,
5. object, instance, and triangle budgets,
6. output requirements,
7. and a small number of high-quality examples.

Example request:

```text
Create a three-floor Central European corner apartment building.

Allowed definition families:
- window.classic.*
- door.apartment.*
- balcony.iron.*
- roof.clay.*
- chimney.masonry.*
- shopfront.urban.*

Return strict mc3.json.
Do not redefine imported objects.
Use instances wherever possible.
Maximum 120 unique primitives.
Maximum 350 instances.
Provide near and medium LOD references.
```

### 9.4 Generation pipeline

```text
content request
    ↓
request queue
    ↓
cheap AI model first
    ↓
strict mc3.json output
    ↓
JSON Schema validation
    ↓
Mc3Document parsing
    ↓
reference and bounds validation
    ↓
MC3 XML/MCB compilation
    ↓
preview rendering
    ↓
automated quality checks
    ├── accepted
    ├── repair request
    └── escalation to a stronger model
```

### 9.5 Caching

Cache key should include:

```text
SHA-256(
    provider +
    model +
    prompt_version +
    mc3_contract_hash +
    schema_hash +
    material_catalogue_hash +
    relevant_definition_catalogue_hash +
    requested_object_json +
    seed
)
```

The same request should never be regenerated unnecessarily.

### 9.6 Suggested persistence

A SQLite-based content store can hold:

```text
model_request
model_recipe
model_definition
model_variant
model_dependency
validation_result
render_preview
generation_attempt
model_tag
material_slot
lod_mapping
```

Git should contain curated source assets, schemas, and selected canonical models. Large generated libraries may be packed into content databases or `mc3pack` archives.

---

## 10. Hybrid World Composition

### 10.1 Macro-world layer

The macro-world system should continue to generate:

- continents,
- climate,
- biomes,
- elevation,
- hydrology,
- rivers,
- lakes,
- coastlines,
- mountain ranges,
- settlement locations,
- road networks,
- and cave graphs.

These are not individual MC3 prefabs.

### 10.2 Terrain layer

Terrain should be generated or streamed as:

- heightfield meshes,
- chunked terrain meshes,
- voxel or implicit surfaces where required,
- cliff geometry,
- cave volume geometry,
- and terrain material layers.

MC3 may represent generated terrain chunks, but mountains should not normally be selected as giant isolated prefab models.

### 10.3 Asset placement layer

The placement system chooses assets based on:

- biome,
- slope,
- altitude,
- soil,
- moisture,
- road distance,
- district type,
- parcel size,
- building style,
- local density,
- visibility,
- rarity,
- and neighboring content.

The system must generate relationships, not only isolated objects.

---

## 11. Cities and Villages

Cities are an ideal use case for MC3 asset composition.

The generation order should be street-first:

```text
road network
    ↓
intersections and crossings
    ↓
sidewalks and curbs
    ↓
blocks
    ↓
parcels
    ↓
building envelopes and setbacks
    ↓
building prefab or modular composition
    ↓
facade details
    ↓
street furniture, trees, vehicles, and signs
```

The city system should select coherent style profiles:

```json
{
  "style": {
    "region": "central_europe",
    "period": "1890_1930",
    "wealth": "middle",
    "facade": "stucco",
    "windowFamily": "classic_wood",
    "roofFamily": "clay_gable"
  }
}
```

A building may be:

- a complete prefab,
- a massing shell with modular facade bays,
- a combination of floor modules,
- or a unique landmark scene.

The system should support:

- detached houses,
- row houses,
- apartment buildings,
- corner buildings,
- shops,
- civic buildings,
- stations,
- factories,
- schools,
- hospitals,
- churches,
- castles,
- and landmarks.

The system must avoid the current pattern of a few fixed houses placed independently in a chunk.

---

## 12. Forests and Natural Vegetation

A forest should not normally be one giant `forest_01.mc3` prefab.

It should be generated from:

- terrain,
- tree instances,
- shrub instances,
- grass and ground-cover systems,
- rocks,
- fallen logs,
- stumps,
- mushrooms,
- clearings,
- trails,
- and biome-specific clutter.

Example:

```text
biome = conifer_forest
    → select 8 spruce variants
    → select 3 pine variants
    → select 4 stump variants
    → select 6 rock variants
    → select 5 shrub variants
    → distribute by slope, altitude, moisture, and canopy density
```

Large vegetation counts require:

- GPU instancing,
- LOD,
- impostors or billboards where appropriate,
- frustum culling,
- optional occlusion culling,
- and clustered draw submission.

---

## 13. Mountains, Cliffs, and Rocks

Mountains should use procedural terrain for their macro shape.

MC3 asset libraries should provide:

- cliff wall segments,
- rock outcrops,
- ledges,
- boulders,
- scree fields,
- snow caps or snow detail modules,
- ice formations,
- mountain vegetation,
- paths,
- bridges,
- shelters,
- and cave entrances.

The recommended structure is:

```text
procedural mountain terrain
    +
cliff and rock modules
    +
material layering
    +
biome-specific vegetation
    +
LOD and distance simplification
```

This avoids visible seams and excessive repetition caused by placing entire mountain prefabs.

---

## 14. Caves, Mines, and Underground Worlds

Caves are well suited to modular MC3 composition.

Libraries can include:

- straight tunnel segments,
- curved tunnels,
- ascending and descending tunnels,
- T-junctions,
- four-way junctions,
- small chambers,
- large halls,
- cave entrances,
- shafts,
- pits,
- underground lakes,
- bridges,
- stalactites,
- stalagmites,
- crystals,
- mine supports,
- rails,
- doors,
- torches,
- industrial equipment.

A cave system should be generated as a semantic graph and then realized using compatible modules.

The system must validate:

- connection sockets,
- tunnel alignment,
- traversability,
- vertical transitions,
- collision-free placement,
- minimum clearance,
- and closed geometry where needed.

---

## 15. Interiors

Interiors can use the same asset architecture.

A building interior may be generated from:

- room shells,
- wall and doorway modules,
- stairs,
- furniture kits,
- appliances,
- lights,
- decorations,
- and semantic room rules.

Examples:

```text
kitchen
    → counters along valid walls
    → sink near plumbing socket
    → table in clearance area
    → chairs around table
    → refrigerator with door clearance

bedroom
    → bed against a suitable wall
    → wardrobe with access clearance
    → bedside table
    → lamp
```

Interiors are important, but exterior city quality, streets, and building composition should be stabilized first.

---

## 16. Rendering Through CNA and NOXNA

Mesh Craft and Mesh World run on CNA, the C++ reimplementation of XNA 4.0. CNA already targets or experiments with multiple backends, including:

- SDL renderer,
- OpenGL ES,
- Vulkan,
- bgfx,
- and partial WebGPU support.

The long-term plan is to preserve XNA 4.0 compatibility while adding advanced graphics through NOXNA extensions.

### 16.1 Compatibility and extensions

Classic XNA-style API remains available:

```text
GraphicsDevice
BasicEffect
Texture2D
Model
VertexBuffer
RenderTarget2D
```

Modern features may be exposed through NOXNA systems such as:

```text
NOXNA::PbrMaterial
NOXNA::RenderGraph
NOXNA::ShadowSystem
NOXNA::PostProcessStack
NOXNA::ComputeShader
NOXNA::GpuInstancing
NOXNA::TerrainRenderer
NOXNA::VegetationRenderer
NOXNA::ClusteredLighting
NOXNA::GpuCulling
```

Names and APIs are illustrative and must be designed in CNA rather than copied blindly.

### 16.2 PBR materials

Future MC3 material support should include:

- base color,
- normal maps,
- metallic/roughness,
- ambient occlusion,
- emissive maps,
- height or parallax maps where supported,
- environment reflections,
- texture scaling,
- material layering,
- and weather-dependent parameters.

Example semantic material data:

```json
{
  "shader": "noxna.pbr.standard",
  "baseColorTexture": "brick_base",
  "normalTexture": "brick_normal",
  "metallicRoughnessTexture": "brick_mr",
  "aoTexture": "brick_ao",
  "emissiveTexture": "brick_emissive",
  "uvScale": [0.5, 0.5],
  "doubleSided": false
}
```

### 16.3 Shadows and contact grounding

Important future features:

- cascaded shadow maps,
- stable sun shadows,
- PCF or PCSS filtering,
- point and spot light shadows where affordable,
- contact shadows,
- and shadow LOD policies.

### 16.4 Ambient occlusion

SSAO, GTAO, or an equivalent system can significantly improve:

- wall-ground contact,
- window frames,
- vehicle underbodies,
- intersections between props,
- cave depth,
- and architectural readability.

### 16.5 Post-processing

Potential NOXNA features:

- HDR,
- tone mapping,
- exposure,
- bloom,
- color grading,
- temporal anti-aliasing,
- depth of field for showcase cameras,
- screen-space reflections where suitable,
- motion blur only where justified,
- and optional cinematic effects.

### 16.6 Atmosphere and weather

Potential systems:

- physical sky,
- atmospheric scattering,
- volumetric fog,
- height fog,
- clouds,
- light shafts,
- rain,
- wet surfaces,
- snow accumulation,
- dust,
- and weather transitions.

### 16.7 GPU-driven rendering

Large worlds require:

- GPU instancing,
- indirect drawing,
- frustum culling,
- occlusion culling,
- compute-based instance selection,
- GPU LOD selection,
- texture arrays or atlases,
- shared geometry buffers,
- clustered or tiled lighting,
- and efficient material batching.

Example:

```text
one tree definition
× 20,000 instances
× several LOD levels
→ a small number of efficient GPU batches
```

---

## 17. Backend Capability Tiers

Not every CNA backend must expose the same visual quality.

### Tier 0: compatibility / fallback

Likely targets:

- SDL renderer,
- very limited hardware,
- debugging.

Possible features:

- simple materials,
- limited lighting,
- no advanced shadows,
- aggressive LOD,
- reduced object density.

### Tier 1: portable 3D

Likely targets:

- OpenGL ES,
- WebGPU on modest devices,
- lower-end bgfx paths.

Possible features:

- basic PBR,
- one shadow map or reduced cascades,
- lightweight AO,
- GPU instancing,
- limited local lights,
- simple post-processing.

### Tier 2: advanced

Likely targets:

- Vulkan,
- advanced bgfx backends,
- capable WebGPU implementations,
- capable desktop OpenGL where maintained.

Possible features:

- full PBR,
- cascaded shadows,
- clustered lighting,
- compute shaders,
- GPU culling,
- volumetric effects,
- dense vegetation,
- advanced terrain,
- modern post-processing.

The content system must specify fallback behavior so the same world can render at multiple quality levels.

---

## 18. LOD, Instancing, and Performance

A richer asset library will fail without explicit performance architecture.

Every major asset should define LOD behavior.

Example building detail policy:

```text
near LOD
    full facade modules
    window frames
    doors
    balconies
    gutters
    signs
    local lights

medium LOD
    simplified windows
    reduced props
    merged facade details
    fewer shadows

far LOD
    building massing
    roof silhouette
    simplified material
    no small props
```

A castle-level number of objects per building is acceptable only at near distance and only where instancing and batching make it affordable.

The system should track budgets for:

- unique primitives,
- instances,
- vertices,
- triangles,
- material changes,
- lights,
- shadow casters,
- transparent objects,
- and draw submissions.

Budgets should be configurable by backend tier and distance.

---

## 19. Visual Quality Strategy

The revival must use a bounded vertical slice before attempting global quality.

Recommended first target:

- a deterministic 3×3 or 4×4 chunk city showcase,
- one main street,
- one intersection,
- sidewalks and curbs,
- a pedestrian crossing,
- traffic lights,
- 15–30 buildings,
- several building families,
- parked and moving vehicles,
- trees and street furniture,
- a landmark,
- coherent materials,
- pedestrian and overview cameras,
- and multiple LOD levels.

The showcase must be visually inspected and used for before/after comparisons.

The first 100 hours of revival work should prioritize visible results rather than new unrelated infrastructure.

Suggested high-level allocation:

- generation path diagnostics and showcase setup,
- full transforms where missing,
- street and parcel composition,
- a small set of high-quality building families,
- facade and roof modules,
- asset library loading and variant selection,
- shadows, camera, and material improvements,
- props and vegetation,
- LOD and validation.

A single convincing city block is more valuable than a planet full of sparse boxes.

---

## 20. Migration Strategy

### Stage 1: audit and preserve

- identify active C++ and Lua generators,
- trace the complete runtime generation path,
- identify dead or unused generators,
- identify current MC3 limitations versus wrapper limitations,
- preserve working behavior,
- establish visual and structural baselines.

### Stage 2: introduce shared data foundations

- formalize `Mc3Document` as the single semantic model,
- add `mc3.json` parser and writer,
- add JSON Schema,
- verify XML/JSON semantic round-trip,
- introduce library imports and resolver design.

### Stage 3: create global libraries

- windows,
- doors,
- roofs,
- facade modules,
- street furniture,
- vehicles,
- trees,
- rocks,
- cave modules,
- interior furniture.

### Stage 4: compose a city showcase

- C++ world composer selects assets,
- existing Lua may be used temporarily where useful,
- validate that active runtime uses the new path,
- generate deterministic screenshots and metrics.

### Stage 5: demote runtime Lua

- migrate active generators,
- retain offline conversion tools,
- remove runtime dependencies only after coverage exists,
- document retired generators.

### Stage 6: expand to nature and underground worlds

- forest placement,
- cliffs and rock modules,
- cave graph realization,
- biome kits,
- LOD and instancing.

### Stage 7: modern rendering

- PBR material pipeline,
- shadows,
- AO,
- post-processing,
- atmosphere,
- GPU-driven rendering,
- backend capability tiers.

---

## 21. Validation and Testing

The new system requires multiple validation layers.

### 21.1 Schema validation

- XSD for XML,
- JSON Schema for JSON,
- strict rejection of unknown or invalid fields where appropriate.

### 21.2 Semantic validation

- unique IDs,
- valid references,
- no missing definitions,
- valid material slots,
- valid texture references,
- finite transform values,
- valid bounds,
- no cyclic library dependencies,
- valid LOD mappings.

### 21.3 Spatial validation

- building-road intersection detection,
- parcel containment,
- socket alignment,
- cave connection alignment,
- furniture clearance,
- terrain penetration limits,
- object out-of-bounds detection,
- cross-chunk road continuity.

### 21.4 Performance validation

- object counts,
- triangle counts,
- material counts,
- instance counts,
- draw-call estimates,
- shadow-caster counts,
- LOD reduction ratios.

### 21.5 Visual validation

Automated checks cannot fully determine visual quality.

Human or image-assisted review should verify:

- roof correctness,
- visible geometry intersections,
- camera framing,
- repeated patterns,
- material coherence,
- lighting quality,
- density,
- and style consistency.

The system should render thumbnails from standardized cameras for asset review.

---

## 22. Recommended Diagnostics

Mesh World should expose diagnostics showing:

- generator or composer responsible for each chunk,
- selected biome and district,
- selected style profile,
- selected asset IDs and variants,
- fallback reasons,
- LOD level,
- object and triangle counts,
- material count,
- light count,
- validation warnings,
- unresolved assets,
- and library versions.

This information may appear in:

- metadata,
- logs,
- a debug HUD,
- a chunk inspector,
- or exported reports.

Fallback behavior must never be silent.

---

## 23. Major Risks

### 23.1 Too many assets without good placement rules

A library of 100,000 objects will not automatically create a believable world.

Mitigation:

- semantic metadata,
- style profiles,
- placement rules,
- bounded kits,
- and deterministic testing.

### 23.2 Visual variety becoming visual chaos

Randomly mixing unrelated windows, roofs, and materials will look worse than repetition.

Mitigation:

- style families,
- compatibility tags,
- building-level palette selection,
- district-level rules.

### 23.3 Performance collapse

Dense assets can create excessive draw calls and memory use.

Mitigation:

- definitions and instances,
- LOD,
- batching,
- GPU instancing,
- streaming,
- budgets,
- and compiled MCB assets.

### 23.4 Premature deletion of Lua

Removing Lua before migration may destroy useful capabilities.

Mitigation:

- demote first,
- migrate gradually,
- preserve offline tooling,
- remove only after coverage exists.

### 23.5 Two incompatible MC3 implementations

Independent XML and JSON semantics would create long-term inconsistency.

Mitigation:

- one `Mc3Document` model,
- shared validators,
- shared compiler,
- round-trip tests.

### 23.6 Renderer work hiding content problems

PBR and post-processing cannot fix an empty scene.

Mitigation:

- first build a strong vertical slice,
- improve content and composition before relying on advanced rendering.

### 23.7 Content work hiding renderer problems

Dense geometry can still look flat without shadows, AO, and good materials.

Mitigation:

- evolve content and renderer in coordinated stages.

---

## 24. Plan Derivation Structure

A future `plan.md` should derive tasks from this document in dependency order.

Suggested workstreams:

### R0 — Audit, baseline, and diagnostics

- trace current generation paths,
- establish build and test baseline,
- create deterministic city showcase,
- add generator/asset diagnostics,
- capture visual baseline.

### R1 — MC3 semantic core

- formalize internal data model,
- add `mc3.json`,
- add JSON Schema,
- add XML/JSON round-trip tests,
- define canonical serialization rules.

### R2 — Libraries and imports

- define `mc3lib`,
- namespace and version rules,
- dependency resolver,
- content hashing,
- standalone compilation.

### R3 — Asset metadata and registry

- categories,
- bounds,
- sockets,
- material slots,
- LOD metadata,
- style tags,
- validation state,
- preview references.

### R4 — Urban core libraries

- windows,
- doors,
- roofs,
- facade modules,
- street infrastructure,
- cars,
- props.

### R5 — C++ world composer

- parcel-driven building placement,
- style profile selection,
- asset variant selection,
- cross-chunk continuity,
- fallback behavior.

### R6 — Vertical city showcase

- one rich intersection,
- buildings,
- traffic lights,
- crossing,
- sidewalks,
- vehicles,
- cameras,
- metrics,
- visual QA.

### R7 — AI content factory

- request queue,
- prompt contract,
- structured output,
- validation,
- repair loop,
- caching,
- SQLite storage,
- preview rendering.

### R8 — Nature libraries and composition

- trees,
- shrubs,
- rocks,
- forest floor,
- biome placement,
- instancing and LOD.

### R9 — Mountains and cliffs

- terrain integration,
- cliff modules,
- rock outcrops,
- snow and alpine kits,
- seam and slope rules.

### R10 — Caves and underground composition

- cave graph,
- modular tunnels,
- chambers,
- socket validation,
- traversal checks,
- lighting and props.

### R11 — Runtime compilation and packaging

- MCB compilation,
- `mc3pack`,
- dependency pruning,
- streaming,
- cache invalidation,
- asset versioning.

### R12 — CNA/NOXNA rendering integration

- PBR,
- shadows,
- AO,
- post-processing,
- atmosphere,
- GPU instancing,
- GPU culling,
- backend tiers.

### R13 — Migration and cleanup

- migrate active Lua generators,
- retain useful offline scripts,
- remove obsolete runtime paths,
- update documentation,
- archive superseded systems.

Each plan task should include:

- deliverable,
- rationale,
- dependencies,
- probable files/subsystems,
- automated verification,
- visual verification where applicable,
- and completion evidence.

Tasks must be small enough to implement and verify independently. The plan must not contain vague tasks such as “make the world realistic” or “implement CityEngine features.”

---

## 25. Definition of Success

The revival is succeeding when:

- Mesh World no longer renders sparse test scenes as its normal city output,
- a deterministic city showcase looks visibly rich and coherent,
- buildings reference reusable global definitions,
- multiple style-controlled variants are used without visual chaos,
- `mc3.json` and `mc3.xml` round-trip through one data model,
- libraries resolve deterministically,
- standalone assets include only required dependencies,
- AI-generated content passes schema and semantic validation,
- forests are built from instanced biome assets,
- mountains combine procedural terrain with modular detail,
- caves are assembled from validated modules,
- runtime Lua is no longer required for the primary world path,
- LOD and instancing keep dense scenes performant,
- and CNA/NOXNA progressively improves lighting, materials, atmosphere, and GPU utilization.

The ultimate goal is not to imitate CityEngine, Unreal Engine, or another product exactly. The goal is to build a distinctive, offline-first, modular procedural world system centered on CNA, Mesh Craft, MC3, and Mesh World.

---

## 26. Final Direction

The agreed direction can be summarized as follows:

> Mesh World will procedurally generate the semantic structure of a planet and compose its visible content from reusable, style-aware MC3 assets and modules. MC3 XML remains supported, while semantic MC3 JSON becomes the preferred AI and automation format. Global definition libraries provide variants, metadata, sockets, LODs, and material slots. A C++ world composer becomes the primary runtime path, while Lua is gradually demoted to migration and offline tooling. AI generates validated assets offline and stores them in a cached content system. CNA and NOXNA extensions progressively add modern PBR, shadows, AO, atmosphere, post-processing, instancing, culling, and GPU-driven rendering. The revival begins with one excellent vertical slice and expands only after its composition, validation, and performance are proven.

