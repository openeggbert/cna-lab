// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ChunkGenerator.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/TileCoord.hpp"
#include "PlanetWorld.hpp"
#include "WorldConfig.hpp"
#include "generators/map/PlanetGenerator.hpp"

namespace MeshWorld::Map { class MapPipeline; }

namespace MeshWorld {

// Logic behind the MeshWorldPlanet CLI (MAP13, M203/M204/hand-off demo),
// factored out of src/tools/planet_map.cpp so it's directly unit-testable
// (M211) without spawning the binary as a subprocess. planet_map.cpp's
// main() is a thin wrapper calling these functions plus I/O.

// PlanetParams is kept independent of WorldConfig (see PlanetGenerator.hpp) —
// translate at this call site, same as MapPipelineTests.cpp's make_params().
Map::PlanetParams planet_params_from_config(const WorldConfig& cfg);

bool is_existing_planet_world(const std::string& dir);

// Parsed command-line request: which world directory, which tile.
struct PlanetMapCliArgs {
    std::string    dir;
    Map::TileCoord tile{0, 0, 0};
    bool           stats{false};
    bool           ascii{false};
    bool           names{false};
    std::string    png_path;         // empty = --png not requested
    std::string    png_region_path;  // empty = --png-region not requested
    int            region_tiles{2};  // N for --png-region's NxN stitch, from `tile` as top-left
    std::string    geojson_path;     // empty = --geojson not requested
    std::string    mbtiles_path;     // empty = --mbtiles not requested
    bool           png_hillshade{false};  // --png-mode hillshade (default: biome coloring)
    bool           validate{false};  // --validate: check every persisted tile in the world dir
    bool           db_sizes{false};  // --db-sizes: report per-level/per-region DB file sizes
    std::string    legend_path;      // empty = --legend not requested (M279, MAP16)
};

// Returns {args, false} on a usage error (missing dir, malformed --tile,
// --level/--tile out of range for the quadtree).
std::pair<PlanetMapCliArgs, bool> parse_planet_map_args(int argc, char* argv[]);

// Pure computation over an already-generated payload — kept separate from
// printing so tests can assert on the numbers without capturing stdout.
struct TileSummary {
    int   land_cells{0};
    int   ocean_cells{0};
    float elevation_min{0.0f};
    float elevation_max{0.0f};
};

TileSummary summarize_tile(const Map::MapTilePayload& payload);

// One name per ZoneType ordinal (see include/ZoneType.hpp). Grown 12->52,
// M235 (MAP16, 2026-07-10) -- order and grouping must match the enum
// exactly (grassland/dry, forest, wetland, desert/arid, cold/high-
// elevation, volcanic/geothermal, aquatic/coastal, then empty last).
inline constexpr std::array<const char*, 52> ZONE_NAMES = {
    "city", "jungle", "desert", "forest", "ocean", "mountain",
    "tundra", "swamp", "cave", "meadow", "beach",

    "savanna", "steppe", "prairie", "chaparral", "shrubland",

    "taiga", "temperate_rainforest", "mixed_forest", "cloud_forest",
    "mangrove", "bamboo_forest", "riparian_forest", "tropical_dry_forest",

    "marsh", "floodplain", "bog", "muskeg",

    "dunes", "rocky_desert", "cold_desert", "salt_flat", "badlands",
    "mesa", "canyon", "oasis",

    "glacier", "permafrost", "alpine_meadow", "ice_cap",

    "volcanic", "geothermal", "ash_plain", "volcanic_island",

    "coral_reef", "kelp_forest", "deep_ocean", "lagoon", "fjord",
    "tidal_flat", "sea_cliff",

    "empty"
};

std::array<int, ZONE_NAMES.size()> biome_histogram(const Map::BiomeGrid& biome);

// One letter per ZoneType ordinal, same mapping src/tools/print_map.cpp uses
// for the legacy WorldMap zone grid (M206 — "extend MeshWorldMap" reuses that
// visualizer's exact letter scheme rather than inventing a new one).
char zone_ascii_char(int zone_ordinal);

// M206 — "--ascii": renders a tile's BiomeGrid as a space-separated grid of
// zone_ascii_char() letters, one row per line, terminated by a trailing '\n'.
std::string render_ascii_biome_map(const Map::BiomeGrid& biome);

// M209 — "--names": one entry per named MapFeature (border/river/mountain
// range/...) and per PlaceLabel (capital/city/town/...) in a tile's payload.
// `kind` is the FEATURE_NAMES string for a feature or PlaceLabel::kind for a
// label; unnamed features (name.empty()) are skipped.
struct NamedEntry {
    std::string kind;
    std::string name;
};

std::vector<NamedEntry> collect_names(const Map::MapTilePayload& payload);

void print_names_list(const Map::MapTilePayload& payload);

// One name per FeatureType ordinal (see Map::FeatureType in MapTilePayload.hpp).
inline constexpr std::array<const char*, 12> FEATURE_NAMES = {
    "river", "lake", "mountain_range", "coastline", "border", "city",
    "town",  "road", "other",          "continent", "street", "park"
};

std::array<int, FEATURE_NAMES.size()> feature_histogram(const std::vector<Map::MapFeature>& features);

void print_tile_summary(const std::string& dir, const PlanetWorld& world,
                         const Map::TileCoord& tile, const Map::MapTilePayload& payload,
                         bool freshly_generated);

// M207 — "--stats": continent/river/city counts + land ratio for one tile's
// payload (the payload the CLI already generated/loaded; not a whole-planet
// traversal). Printed in addition to, not instead of, print_tile_summary().
void print_feature_stats(const Map::MapTilePayload& payload);

// Result of handing one chunk off to the existing ChunkPipeline. Pure — no
// I/O; print_handoff_result() below does the printing/saving.
struct HandoffResult {
    ChunkCoord   chunk;
    ChunkContext ctx;
    std::string  mc3_xml;
};

// Picks one chunk from tile's chunk range and runs it through ChunkPipeline
// with `pipeline` attached (see M157/M159/M160 — this is what exercises
// map_context-driven zone/elevation as a running program).
HandoffResult run_handoff(const Map::TileCoord& tile, Map::MapPipeline& pipeline);

void print_handoff_result(const std::string& dir, const HandoffResult& result);

// M205 — "--png": renders a tile to an RGB raster image (biome-color grid
// plus a feature/label overlay), written out via the vendored
// stb_image_write.h. There is no font-rendering dependency in this project,
// so labels are drawn as small color-coded position markers, not literal
// text glyphs — see docs/map-generation.md.
struct RgbImage {
    int                       w{0}, h{0};
    std::vector<std::uint8_t> pixels;  // w*h*3 bytes, row-major, top-to-bottom
};

// One RGB color per ZoneType ordinal (see ZONE_NAMES above) — the base
// biome-color fill.
std::array<std::uint8_t, 3> zone_rgb_color(int zone_ordinal);

// One RGB color per FeatureType (see FEATURE_NAMES above) — the vector
// overlay drawn on top of the biome fill.
std::array<std::uint8_t, 3> feature_rgb_color(Map::FeatureType type);

// One RGB color per PlaceLabel::kind ("capital"/"city"/"town"/"village"/
// "country"/"continent"/...); unrecognized kinds get a neutral gray so a
// future label kind doesn't render invisibly.
std::array<std::uint8_t, 3> label_rgb_color(const std::string& kind);

// Renders payload.biome at `upscale`x its native grid resolution (each cell
// becomes an upscale x upscale block of solid color), then draws
// payload.features as polylines/point markers and payload.labels as small
// dots, mapping their world-meter coordinates into pixel space via
// payload.tile.world_bounds().
RgbImage render_png_biome_image(const Map::MapTilePayload& payload, int upscale = 8);

// M232 — "--png-mode hillshade": a grayscale directional shaded-relief
// render driven purely by payload.elevation (no biome/feature/label
// overlay), as an alternative to render_png_biome_image()'s biome-color
// fill. Each cell's brightness comes from a fixed upper-left light
// direction dotted with a normal estimated from its immediate elevation
// neighbors (steeper slope facing the light -> brighter; facing away ->
// darker). A perfectly flat grid renders as a uniform mid-gray.
//
// M343 (MAP22) — north/east/south/west are this tile's 4 neighboring
// tiles' own elevation grids (same convention MapTilePayload::edges/
// ChildGenerator.cpp's en/ee/es/ew use: north=this tile's own row gy=0
// side, so north's own BOTTOM row gy=north->h-1 is the true continuation;
// symmetric for the other 3). Optional (default nullptr) — every existing
// single-tile caller/test is unaffected. When absent, an edge row/column's
// central-difference slope estimate clamps to this tile's own boundary
// value instead (the pre-M343 behavior), which is a strictly WORSE
// one-sided estimate than the interior gets, producing a real, visible
// hillshade brightness seam at every tile boundary in a stitched multi-
// tile render — this tile's own elevation DATA is provably continuous
// across boundaries already (tests/MapPipelineTests.cpp's own
// SiblingSharedEdgeOrderIndependent*), so the seam was a rendering-only
// artifact, not a generation bug. Ignored (falls back to clamping) if a
// neighbor's grid is empty or a different height/width than this tile's
// own — a malformed/mismatched neighbor is treated as if none was passed.
RgbImage render_png_elevation_image(const Map::MapTilePayload& payload, int upscale = 8,
                                     const Map::FieldGrid* north = nullptr,
                                     const Map::FieldGrid* east  = nullptr,
                                     const Map::FieldGrid* south = nullptr,
                                     const Map::FieldGrid* west  = nullptr);

// Writes `image` to `path` as a PNG. Returns false on write failure (e.g. an
// unwritable path); the CLI reports that as an error.
bool write_png_file(const std::string& path, const RgbImage& image);

// M279 (MAP16, 2026-07-10) — "--legend": a grid of one color swatch per
// ZoneType ordinal (see ZONE_NAMES above), in ordinal order, `cols` wide.
// No text labels (this project has no font-rendering dependency, see
// render_png_biome_image()'s own header note on PlaceLabel markers) --
// pair with print_zone_legend_list() below to map swatch position -> name.
RgbImage render_zone_legend_image(int swatch_px = 40, int gap_px = 4, int cols = 8);

// Prints "ordinal  name  (r,g,b)" one line per ZoneType value to stdout,
// the textual half of --legend's output since the PNG itself carries no
// labels.
void print_zone_legend_list();

// M229 — stitches an n x n block of sibling tiles at `top_left.level` into
// one larger image, starting at `top_left` (inclusive), each tile rendered
// via render_png_biome_image() (generating/persisting it through `pipeline`
// if not already stored) and pasted into its grid cell. Tiles outside the
// level's valid [0, 2^level) index range are skipped (left blank); returns
// an empty (0x0) image if every requested tile is out of range.
RgbImage render_png_region_image(Map::MapPipeline& pipeline, const Map::TileCoord& top_left,
                                  int n, int upscale = 8);

// M230 — "--geojson": exports a tile's vector features (MapFeature) and
// place labels (PlaceLabel) as a GeoJSON-style FeatureCollection (map.md
// §10's already-designed vector format: coastlines/borders/rivers/roads as
// LineStrings, closed border/continent loops as Polygons, cities/labels as
// Points). Coordinates are the payload's own world-space meters, not
// lon/lat -- a "GeoJSON-style" custom format for an in-game planet, not a
// georeferenced real-world map (same "custom, compact" choice map.md §10
// already made for vector features).
std::string render_geojson(const Map::MapTilePayload& payload);

// Writes `text` to `path`. Returns false on write failure (e.g. an
// unwritable path).
bool write_text_file(const std::string& path, const std::string& text);

// M231 — packages every already-generated tile at `level` (enumerated via
// Map::MapTileStore::list_tiles(), so this does NOT generate anything new)
// into a single MBTiles-style SQLite file at `out_path`: the standard
// `metadata` (name/value) + `tiles` (zoom_level, tile_column, tile_row,
// tile_data) schema real MBTiles readers expect, `tile_row` in the TMS
// (Y-flipped) convention the spec uses. Each tile's image is
// render_png_biome_image() PNG-encoded directly to memory (never written to
// disk as a loose file). Returns the number of tiles packaged, or -1 if
// `out_path` couldn't be opened as a SQLite database.
int export_mbtiles(const std::string& world_dir, int level, const std::string& out_path,
                    int upscale = 8);

// M210 — "--validate": one already-generated (persisted) tile that failed
// MapValidator::validate(), plus why.
struct TileValidationIssue {
    Map::TileCoord           tile;
    std::vector<std::string> errors;
};

// M210 — result of validating every already-generated tile across every
// quadtree level a world directory actually has data for. `tiles_checked`
// counts every tile visited (pass or fail); `issues` holds only the ones
// that failed, in the order their level/tile were visited.
struct WorldValidationResult {
    int                               tiles_checked{0};
    std::vector<TileValidationIssue> issues;

    bool ok() const { return issues.empty(); }
};

// M210 — validates every persisted tile in `world_dir` via MapValidator,
// across every quadtree level that already has a map_level{N}.db file
// (discovered by directory scan, same as MAP15 M231's list_tiles() use --
// this does NOT generate anything new, and deliberately does not probe
// levels 0..MAX_LEVEL blindly, since MapTileStore's constructor creates a
// DB file on first open and this is meant to be read-only over what's
// already there). A world directory with no map_level*.db files yet (or
// that doesn't exist) yields an empty, "ok" result -- consistent with
// export_mbtiles's "nothing to do" behavior for an ungenerated level.
WorldValidationResult validate_world(const std::string& world_dir);

// Prints validate_world()'s result: a summary line, then each failing
// tile's coordinate and error list.
void print_world_validation_result(const WorldValidationResult& result);

// M223 — one database file's size, for the --db-sizes report.
struct DbSizeEntry {
    std::string   label;       // filename relative to world_dir, e.g. "map_level4.db"
                                // or "models/2_-1.db"
    std::uint64_t size_bytes{0};
};

// M223 — per-level map tile DBs and per-region MAP11 model-placement shards
// found in a world directory, plus their total. Discovered by directory
// scan (same "don't assume a fixed level/region list" approach as M210's
// discover_persisted_levels()) -- a world that never touched MAP11 model
// placements simply has an empty region_dbs.
struct DbSizeReport {
    std::vector<DbSizeEntry> map_level_dbs;  // sorted by level
    std::vector<DbSizeEntry> region_dbs;     // sorted by (rx, rz)
    std::uint64_t            total_bytes{0};
};

// M223 — "--db-sizes": reports the on-disk size of every map_level{N}.db
// and models/<rx>_<rz>.db file already in `world_dir`. Read-only (uses
// std::filesystem::file_size(), never opens the files as SQLite
// databases) -- safe to run against a world directory another process has
// open. A world directory with nothing generated yet (or that doesn't
// exist) yields an empty report with total_bytes == 0.
DbSizeReport audit_db_sizes(const std::string& world_dir);

// Prints audit_db_sizes()'s result: one line per DB file (human-readable
// size), grouped map-levels-then-regions, plus a total.
void print_db_size_report(const DbSizeReport& report);

} // namespace MeshWorld
