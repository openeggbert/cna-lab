# MeshWorld — Planetary Map Generation

This document is the user/dev guide for the planetary map subsystem (`namespace
MeshWorld::Map`, `include/Map/` + `src/Map/`) and its CLI driver, `MeshWorldPlanet`.
For the subsystem's design rationale and full architecture, see `map.md` (repo root);
for the M-series task breakdown, see `plan.md`'s "Map mechanism" section. This doc
covers what's already built and how to use it, not design history.

## Overview

MeshWorld generates two things at two different scales:

- **Chunks** (`namespace MeshWorld`, flat) — 3D scene geometry, `chunk_size_m` (default
  64m) squares, output as MC3 XML. This is the original, still-active generator system;
  see the main [README](../README.md) and `docs/architecture.md`.
- **The planetary map** (`namespace MeshWorld::Map`) — a quadtree of 2D tiles sitting
  *above* chunks. Level 0 is the whole planet (~22,585 km); each deeper level halves
  the tile edge, down to `MAX_LEVEL` (18), where tiles hand off to the chunk system.
  Every tile carries elevation/temperature/moisture/biome grids plus vector features
  (rivers, roads, borders, mountain ranges, coastlines) and place labels (capitals,
  cities, towns).

The map layer is optional and additive: chunk generators that don't read
`ChunkContext.map_context` behave exactly as before. It persists to per-level SQLite
files (`map_level{z}.db` in the world directory) and is **not reproducible from a
fixed seed** — a world's state comes from what's actually been generated and saved,
not from re-deriving it.

## `MeshWorldPlanet` — headless CLI driver

```bash
./build/MeshWorldPlanet <world-dir> [--level Z --tile X,Y] [--stats] [--ascii] [--names]
                         [--png out.png [--png-mode biome|hillshade]]
                         [--png-region out.png [--region-tiles N]]
                         [--geojson out.geojson] [--mbtiles out.mbtiles]
                         [--validate] [--db-sizes]
```

Creates a fresh planet world in `<world-dir>` if it doesn't already contain a
`world.json` with planet fields; otherwise reopens the existing one. `--level`/`--tile`
descends the quadtree to that tile, generating any missing ancestors along the way
(Lua-first at levels with a registered generator, C++ fallback otherwise); with no
flags, level 0 (the whole planet) is used. Every invocation also hands one chunk from
the tile's range off to the legacy `ChunkPipeline` and writes the resulting MC3 scene
to `<world-dir>/handoff_chunk_X_Y.mc3.xml` — this is the vertical-slice demo that
exercises the full planet-to-chunk descent as a running program.

### Inspection flags

| Flag | Adds |
|---|---|
| `--stats` | Land ratio + continent/river/mountain-range/city/town/border/road counts for the tile. |
| `--ascii` | The tile's `BiomeGrid` as a letter grid (same zone-letter scheme as `MeshWorldMap`). |
| `--names` | Every named `MapFeature`/`PlaceLabel` in the tile (continents, rivers, borders, cities, …). |

### Export flags

| Flag | Produces |
|---|---|
| `--png out.png` | An image render of the tile — see below. |
| `--png-region out.png --region-tiles N` | An NxN block of sibling tiles stitched into one image. Always biome-mode — `--png-mode hillshade` has no effect here, only on plain `--png`. |
| `--geojson out.geojson` | The tile's vector features/labels as a GeoJSON-style `FeatureCollection`. |
| `--mbtiles out.mbtiles` | Every already-generated tile at `--level` packaged into one MBTiles-style SQLite file. |
| `--legend out.png` | A color-swatch key for all 52 `ZoneType` values (M279) — see below. |

Re-running any export flag against the same output path overwrites it cleanly — these
are meant to be re-run as a world grows, not one-shot.

### Diagnostic flags

Read-only over the whole world directory (not just `--tile`) — neither generates
anything new.

| Flag | Reports |
|---|---|
| `--validate` | Runs `MapValidator` over every already-persisted tile at every level; prints any structural errors and exits non-zero if any tile fails. |
| `--db-sizes` | On-disk size of every `map_level{N}.db` and `models/<rx>_<rz>.db` (MAP11 region shard) file already in the world directory, plus a total. |

## `--png`: image rendering

`--png out.png` renders `payload.biome` at 8x its native grid resolution (each cell
becomes an 8x8 block of solid color), with two selectable modes (`--png-mode`):

- **`biome`** (default) — the biome-color grid, plus a feature/label overlay: rivers/
  roads/borders/coastlines drawn as lines, place labels as small color-coded dots.
  There's no font-rendering dependency in this project, so labels are positions, not
  literal text glyphs. **2026-07-10 update (MAP22):** the biome-color grid itself is
  bilinearly blended between neighboring cells' colors in cell-center space (M344) —
  a pixel deep inside a cell still resolves to that cell's pure color, but pixels near
  a shared boundary blend, replacing the previous hard, blocky per-cell edges
  (especially visible now that MAP16 raised the biome count to 52). Overlapping label
  markers are also decluttered (M346): a lower-priority label (continent > country >
  capital > city > town > village > river > other) whose marker would overlap an
  already-drawn higher-priority one's is silently skipped, so a crowded low-zoom
  render doesn't stack same-size dots into an unreadable blob.
- **`hillshade`** — a grayscale directional shaded-relief render driven purely by
  `payload.elevation`, independent of biome/features/labels. Per-cell brightness comes
  from a fixed upper-left light direction (the conventional azimuth=315° GIS default)
  dotted with a normal estimated from immediate elevation neighbors; flat terrain
  renders as a uniform mid-gray, slopes facing the light are brighter than slopes
  facing away. **2026-07-10 update (MAP22, M343):** at a tile's own edge, `--png
  --png-mode hillshade` now fetches the REAL 4 neighboring tiles (via
  `MapPipeline::get()`) and uses their true boundary elevation for the slope estimate
  there instead of clamping onto this tile's own boundary value — the previous
  clamped estimate was a real, visible brightness seam at every tile edge, even
  though the underlying elevation data itself was already seamless across tiles.
  (`--png-region` doesn't support hillshade mode at all — it always renders `biome`,
  regardless of `--png-mode` — so this fix is specific to single-tile `--png` calls.)

PNG encoding is via a vendored single header, `stb_image_write.h` (public domain),
fetched at CMake configure time and SHA256-pinned — see `CMakeLists.txt`. This is the
project's only image dependency; don't introduce a second one without a reason.

`--png-region out.png --region-tiles N` (default `N=2`) stitches an NxN block of
sibling tiles, starting at `--level`/`--tile` as the top-left corner, into one larger
image — useful at levels > 0, where a single tile is only part of a continent/region,
not the whole planet (level 0 alone already *is* the whole planet). Tiles outside the
level's valid `[0, 2^level)` index range are left blank rather than erroring.

## `--geojson`: vector export

Exports `payload.features` and `payload.labels` as a GeoJSON-style `FeatureCollection`
(the vector format `map.md` §10 designed for this project — custom, not tied to a
particular GIS tool). Geometry type follows point count:

- A single-point feature or any label → `Point`.
- An open multi-point path (rivers, roads, mountain ranges, and coastlines that run
  off the tile's own edge — the common case, see below) → `LineString`.
- A closed loop (first point repeated as the last) → `Polygon` (one ring).
  `Countries::grow()`'s own border/region polygons always close this way; **since
  2026-07-10 (MAP22, M342)**, `FeatureType::Coastline` features do too, but only when
  the traced coastline (an island, say) is fully enclosed within one tile — a
  coastline that crosses the tile boundary is an open `LineString` instead (`Map::
  Coastline::trace()`'s own Eulerian-path algorithm produces both shapes depending on
  the underlying terrain).

**2026-07-10 update (MAP22, M346):** every label `Point` feature also carries a
`labelPriority` integer property (lower = more important: continent=0, country=1,
capital=2, city=3, town=4, village=5, river=6, other=7) — the same ranking `--png`'s
own label-declutter pass uses, exposed here so an external GIS consumer can replicate
that same declutter logic at its own chosen zoom level instead of `--png`'s fixed
resolution. Unlike `--png` (which drops overlapping lower-priority markers outright),
`--geojson` keeps every label — a vector consumer can zoom/declutter dynamically, so
dropping data here would be presumptuous — except TRUE exact duplicates (same
name+kind+position), which are silently merged into one feature.

Coordinates are the payload's own world-space **meters**, not longitude/latitude —
this describes a fictional planet, not a georeferenced real-world location, so there's
no CRS to conform to. Features with no points (nothing to draw) are skipped.

## `--mbtiles`: tile package export

Packages every tile **already persisted** at `--level` (it does not generate anything
new — enumerated via `Map::MapTileStore::list_tiles()`) into a single
[MBTiles](https://github.com/mapbox/mbtiles-spec)-style SQLite file: the standard
`metadata` (name/value pairs) and `tiles` (`zoom_level`, `tile_column`, `tile_row`,
`tile_data`) schema real MBTiles readers (QGIS, TileMill, etc.) expect. Each tile's
image is the same `--png` biome render, PNG-encoded straight to memory — no loose
per-tile files are written. `tile_row` uses the MBTiles spec's TMS convention (Y axis
flipped relative to this project's own `TileCoord.y`).

## `--legend`: zone color legend

`--legend out.png` (M279, MAP16, 2026-07-10) renders one color swatch per `ZoneType`
ordinal (`PlanetMapLogic.hpp`'s `ZONE_NAMES`, currently 52 values — see `map.md`'s
biome section for the full climate-family breakdown), arranged in ordinal order, 8
per row by default. Since a 52-color biome map (up from the original 12) is no longer
self-explanatory at a glance, and this project has no font-rendering dependency to
draw text labels directly on the image (same constraint `--png`'s place-label markers
already work around), `--legend` also prints the ordinal → name → RGB mapping to
stdout as the swatch grid's textual key — run it alongside the PNG, don't rely on the
image alone.

## Zone → chunk generator dispatch audit (M280, 2026-07-10; updated M349, 2026-07-10)

`ChunkGenerator::get_generator()` (`src/ChunkGenerator.cpp`)'s zone-open fallback —
the generator used when nothing more specific (an exact zone+region match, or a
region-only match) applies — has an explicit `case` for exactly the original 10
non-city, non-empty `ZoneType` values (`forest`, `jungle`, `desert`, `cave`,
`mountain`, `meadow`, `beach`, `ocean`, `swamp`, `tundra`). **At M280's original audit,
all 40 new values added by M235 fell through to `EmptyGenerator`** — there was no
`case` for any of them.

**2026-07-10 update (MAP20 M326):** `coral_reef`/`kelp_forest` are no longer part of
that "all 40" group — they now have a real `case ZoneType::coral_reef: case
ZoneType::kelp_forest: return &s_ocean;` dispatch (`ChunkGenerator.cpp`), plus real
coral/kelp `ObjectDefinitionLibrary` shapes for `OceanGenerator` to place. The other
38 new values still fall through to `EmptyGenerator` — MAP17 (wiring the map layer
into more chunk generators) turned out to be about `ChunkContext.map_context`
*content* reads, not zone-dispatch *routing*, so it didn't change this table's own
scope; MAP21 made `ZoneType::cave` (one of the ORIGINAL 10, not a new MAP16 value)
*reachable* from real terrain via `BiomeClassifier::classify()`, but `cave`'s own
dispatch `case` had already existed since MAP11 — not a new routing change either.
The table below still records the most sensible *existing* generator each remaining
new biome's fallback could reasonably be pointed at as a stopgap, grouped by climate
family — useful context for whoever picks up wiring more of them, not a claim that
this routing exists in code yet (except the `coral_reef`/`kelp_forest` row, now
marked done):

| Climate family | New `ZoneType` values | Nearest existing generator |
|---|---|---|
| Grassland/dry | `savanna`, `steppe`, `prairie`, `chaparral`, `shrubland` | `MeadowGenerator` (not yet wired) |
| Forest | `taiga`, `temperate_rainforest`, `mixed_forest`, `cloud_forest`, `mangrove`, `bamboo_forest`, `riparian_forest`, `tropical_dry_forest` | `ForestGenerator` (`mangrove` is arguably closer to `SwampGenerator`) (not yet wired) |
| Wetland | `marsh`, `floodplain`, `bog`, `muskeg` | `SwampGenerator` (not yet wired) |
| Desert/arid | `dunes`, `rocky_desert`, `cold_desert`, `salt_flat`, `badlands`, `mesa`, `canyon`, `oasis` | `DesertGenerator` (not yet wired) |
| Cold/high-elevation | `glacier`, `permafrost`, `alpine_meadow`, `ice_cap` | `TundraGenerator` (`alpine_meadow` is arguably closer to `MeadowGenerator`) (not yet wired) |
| Volcanic/geothermal | `volcanic`, `geothermal`, `ash_plain`, `volcanic_island` | `MountainGenerator` (not yet wired) |
| Aquatic/coastal | `coral_reef`, `kelp_forest` | `OceanGenerator` — **wired (MAP20, M326)** |
| Aquatic/coastal (remaining) | `deep_ocean`, `lagoon`, `fjord`, `tidal_flat`, `sea_cliff` | `OceanGenerator` (`tidal_flat`/`sea_cliff` are arguably closer to `BeachGenerator`) (not yet wired) |

## File formats & storage (summary)

See `map.md` §10 for the full rationale. In short:

| Data | Format |
|---|---|
| Vector features (in-memory / persisted) | Custom GeoJSON-style JSON (`Map::MapPayloadCodec`) |
| Per-tile scalar fields (elevation, temperature, moisture, biome) | Float/int grids, JSON-encoded |
| Tile storage | SQLite per level, `map_level{z}.db` (MBTiles-adjacent: one SQLite-of-tiles file per level) |
| Raster preview export | PNG (`--png`), same convention slippy-map tile servers use |
| Tile package export | MBTiles (`--mbtiles`) — the real spec, for use outside this project |
