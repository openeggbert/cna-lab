# People asset pipeline

## Runtime contract

People is a 2D sprite game. The runtime consumes textures, sprite sheets,
atlases, and metadata. It never requires source meshes, Blender, a 3D renderer,
skeletal 3D animation, or a depth buffer for world geometry.

Offline 3D is an optional manufacturing technique:

```text
original model or procedural scene
  -> fixed orthographic rig
  -> four directional transparent renders
  -> trim and normalize without moving the contact anchor
  -> atlas and metadata
  -> provenance validation
  -> Content/Sprites
```

Procedural placeholder sprites are the baseline until owned final art exists.

## Version 1 visual standard

The standard is original and selected for readable modern displays rather than
compatibility with another game's measurements.

| Property | People v1 rule |
|---|---|
| Logical floor tile | 1 x 1 world units |
| Base tile footprint | 96 px wide x 48 px high at zoom 1.0 |
| Projection | orthographic 2:1 dimetric, conventionally described as game isometric |
| Horizontal half-tile | 48 px |
| Vertical half-tile | 24 px |
| Camera yaw | 45 degrees plus view rotation 0/90/180/270 degrees |
| Camera elevation | 30 degrees above the ground plane for matching offline renders |
| Directions | `North`, `East`, `South`, `West`, indexed 0..3 clockwise |
| Object scale | authored against the 96 px tile diamond; real-world proportions may be exaggerated for readability |
| Floor contact | exact projected footprint plane, never inferred from opaque bounds |
| Sprite anchor | bottom-center at the primary footprint contact point unless metadata declares a multi-tile anchor |
| Background | fully transparent RGBA outside object coverage |
| Working render | at least 2x target linear dimensions; downsample once into delivery size |
| Alpha | straight-alpha source; processing trims RGB fringes and emits the runtime convention CNA validation selects |
| Light | warm key from world north-west and above; soft neutral fill; consistent across all rotations |
| Shadows | separate optional sprite layer, anchored in world space; no baked shadow outside declared bounds for placement-critical art |
| Cropping | common logical canvas across all four rotations, or per-view crop with explicit identical contact anchors |
| Delivery | lossless PNG until an atlas/container format is deliberately validated |

Camera elevation is 30 degrees because a 45-degree yaw with that elevation
produces the selected 2:1 ground diamond. This is a dimetric projection, though
"isometric" remains the conventional genre and code terminology.

## Object deliverables

A rotatable object named `chair_warmwood` ultimately produces:

```text
assets/source/objects/chair_warmwood/...
assets/prompts/objects/chair_warmwood.md
assets/generated/objects/chair_warmwood/raw_*.png
assets/processed/objects/chair_warmwood/chair_warmwood_r0.png
assets/processed/objects/chair_warmwood/chair_warmwood_r1.png
assets/processed/objects/chair_warmwood/chair_warmwood_r2.png
assets/processed/objects/chair_warmwood/chair_warmwood_r3.png
Content/Sprites/Objects/chair_warmwood.png
Content/Objects/chair_warmwood.json
```

Metadata must define sprite region, direction, pixel anchor, logical footprint,
sort anchor, optional shadow region, source scale, and object-state variants.
Multi-tile objects may later use segments or multiple sort points; a single
oversized sprite must not be allowed to make depth ordering nondeterministic.

## Character progression

1. Procedural four-direction colored humanoid with idle and walk frames.
2. Owned full-body four-direction sprite sheets for essential interactions.
3. Expand animation clips only as interactions require them.
4. Evaluate layered body/hair/clothing/accessory compositing against atlas cost,
   anchor drift, lighting mismatch, and animation authoring burden.
5. Prefer offline-flattened combinations if layer alignment or draw-call cost
   makes runtime composition fragile.

No avatar customization system is built before resident movement, routing, and
the first object interaction work.

## AI-assisted art rules

AI assistance is allowed only with a recorded right to use and redistribute
the outputs. Prompts describe independent art direction and ordinary objects;
they must not request exact copies of identifiable commercial assets.

Acceptable brief:

> Warm turn-of-the-millennium PC isometric domestic furniture, chunky readable
> silhouette, honey-oak dining chair, orthographic transparent render, People
> v1 lighting and four-view rig.

Unacceptable brief:

> Make the exact chair, interface, character, or icon from a named commercial
> game.

Generated output is quarantined under `assets/generated/` until provenance,
visual originality, alpha, anchors, scale, rotations, and redistribution terms
pass review.

## Provenance record

Each source/output group records:

- stable asset ID and display name;
- creator and source owner;
- generator tool/model/version when known;
- generation or acquisition date;
- exact prompt or prompt file;
- source model and its license if derived from 3D;
- scripts and transformations applied;
- hashes and resulting files;
- license and attribution requirements;
- reviewer and approval state;
- rejection/replacement history.

Provenance is content data, not an optional comment in a commit message.

## Reproducible tooling roadmap

- `tools/render-object/`: validate rig, invoke optional headless Blender,
  render four views, and emit anchors.
- `tools/render-character/`: consistent character rig and clip/frame contract.
- `tools/process-sprites/`: trim, normalize, detect alpha fringes, pack atlases,
  and update metadata.
- `tools/validate-assets/`: reject missing provenance, dimensions, directions,
  anchors, licenses, or manifest entries.

Blender and generators are contributor tools only. Ordinary players and source
builders receive processed 2D assets.

