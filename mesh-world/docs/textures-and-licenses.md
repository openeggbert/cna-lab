# MeshWorld — Textures, Materials, and Licenses

All material identifiers used by generators must be registered in `MaterialRegistry`. All texture URIs must be registered in `TextureRegistry`. This enables license tracking, asset resolution, and downstream MCB compilation.

**Reality-check note (2026-07-11):** the actual, current implementation is simpler than the
`TextureRegistry`-with-separate-`base_color_texture`-indirection design this doc originally
described — `TextureRegistry`/`TextureEntry` (`src/TextureRegistry.cpp`) exist but are
unpopulated/unused (`register_texture()` has no caller anywhere, see `NEXT.md` §5 #8). What
actually runs: `MaterialEntry::texture_uri` (`include/MaterialRegistry.hpp`) is a plain
`std::string`, set directly by `BuiltinMaterials.cpp`'s `reg_tex()` helper (an **absolute**
filesystem path as of `1f5c5f5`, see `NEXT.md` §5 #34 for why), and injected into a rendered
document's `Mc3Material`/`Mc3Texture` by `WorldRenderer::inject_materials()`
(`src/WorldRenderer.cpp`), not `Mc3DocumentBuilder`. The `AssetLicenseInfo`/`MaterialEntry`
shapes below are still accurate; the `TextureRegistry` indirection they imply is not
currently exercised by anything.

## Texture asset provenance (2026-07-11)

8 of 9 built-in textured materials use real 512×512 CC0 (public domain) photo textures from
[ambientCG.com](https://ambientcg.com) — no attribution legally required, listed here for
provenance anyway:

| `assets/textures/` file | ambientCG asset ID |
|---|---|
| `grass.png` | `Grass005` |
| `asphalt.png` | `Asphalt031` |
| `brick.png` | `Bricks097` |
| `concrete.png` | `Concrete034` |
| `plaster.png` | `Plaster001` |
| `roof_tile.png` | `RoofingTiles013A` |
| `sand.png` | `Ground093C` |
| `wood.png` | `WoodFloor064` |

`water.png` remains a 4×4 solid-color placeholder — ambientCG (a photogrammetry-focused
site) has no genuine liquid-water surface material; the existing `water` material's own
low-roughness (glossy) flat color already reads reasonably water-like without a texture.

## AssetLicenseInfo

```cpp
// include/AssetLicenseInfo.hpp
struct AssetLicenseInfo {
    std::string license;       // "CC0", "CC-BY-4.0", "MIT", "proprietary", "unknown"
    std::string author;        // artist or source name; empty if CC0
    std::string source_url;    // URL where the asset originates
    std::string attribution;   // display text required for CC-BY variants
};
```

## MaterialRegistry

```cpp
// include/MaterialRegistry.hpp
struct MaterialEntry {
    std::string    id;
    float          roughness{0.8f};
    float          metallic{0.0f};
    std::array<float,4> base_color{0.8f, 0.8f, 0.8f, 1.0f};
    std::string    base_color_texture;  // TextureRegistry id, may be empty
    AssetLicenseInfo license;
};

class MaterialRegistry {
public:
    static MaterialRegistry& instance();
    void register_material(MaterialEntry entry);
    const MaterialEntry* get(const std::string& id) const;  // null if missing
    std::vector<MaterialEntry> all() const;
    bool has(const std::string& id) const;
};
```

Usage from a generator (at startup / static init):

```cpp
MaterialRegistry::instance().register_material({
    .id        = "brick_red",
    .roughness = 0.85f,
    .base_color = {0.65f, 0.22f, 0.12f, 1.0f},
    .license   = { "CC0", "", "", "" },
});
```

The `MC3Writer` (or `Mc3DocumentBuilder`) emits a warning when it encounters a material ID that is not registered. The warning is non-fatal but printed to stderr.

## TextureRegistry

```cpp
// include/TextureRegistry.hpp
struct TextureEntry {
    std::string id;
    std::string uri;        // relative path, e.g. "assets/textures/brick_red.png"
    std::string wrap_u{"repeat"};
    std::string wrap_v{"repeat"};
    std::string filter{"linear"};
    std::string color_space{"srgb"};
    AssetLicenseInfo license;
};

class TextureRegistry {
public:
    static TextureRegistry& instance();
    void register_texture(TextureEntry entry);
    const TextureEntry* get(const std::string& id) const;
    std::vector<TextureEntry> all() const;
};
```

## Pre-registered materials (required by existing generators)

| ID | Description | License |
|----|-------------|---------|
| `grass_park` | Bright park grass | CC0 |
| `grass_garden` | Residential garden grass | CC0 |
| `grass_strip` | Roadside grass strip | CC0 |
| `asphalt` | Road surface | CC0 |
| `pavement_slab` | Sidewalk concrete slabs | CC0 |
| `cobblestone` | Old-town stone paving | CC0 |
| `stone_curb` | Raised kerb stone | CC0 |
| `road_marking_white` | White road marking paint | CC0 |
| `metal_grate` | Drain/grate metal | CC0 |
| `metal_lamp` | Simple lamp post metal | CC0 |
| `metal_lamp_ornate` | Cast iron decorative lamp | CC0 |
| `path_gravel` | Gravel garden path | CC0 |
| `path_stone` | Stone garden path | CC0 |
| `water` | Water surface | CC0 |
| `stone_granite` | Granite stone | CC0 |
| `stone_light` | Light limestone | CC0 |
| `flower_red` | Red flower bed | CC0 |
| `flower_yellow` | Yellow flower bed | CC0 |
| `flower_white` | White flower bed | CC0 |
| `brick_red` | Red brick facade | CC0 |
| `plaster_cream` | Cream plaster facade | CC0 |
| `plaster_yellow` | Yellow plaster facade | CC0 |
| `roof_tile_red` | Red clay roof tiles | CC0 |
| `roof_tile_grey` | Grey slate roof tiles | CC0 |
| `wood_fence` | Wooden garden fence | CC0 |
| `stone_post` | Stone gate post | CC0 |

## MeshWorldMaterials tool

`MeshWorldMaterials` binary (implemented in M13) prints all registered materials:

```
$ ./MeshWorldMaterials
ID                   Roughness  Metallic  License    Author
--------------------------------------------------------------
asphalt              0.95       0.00      CC0
brick_red            0.85       0.00      CC0
cobblestone          0.90       0.00      CC0
...
```

## License compliance policy

- All built-in materials use CC0 (public domain). No attribution required.
- If a material references a CC-BY texture, `AssetLicenseInfo::attribution` must be non-empty.
- Proprietary materials must never be committed to the public repo; they belong in `config/materials.local.json` (gitignored).
- The `MeshWorldMaterials --check` flag reports any material with `license = "unknown"` as a warning.
