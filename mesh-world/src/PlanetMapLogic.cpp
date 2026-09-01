// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "PlanetMapLogic.hpp"

#include <mapbox/earcut.hpp>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "ChunkPipeline.hpp"
#include "Map/MapPipeline.hpp"
#include "Map/MapTileStore.hpp"
#include "Map/PlanetConstants.hpp"
#include "MapValidator.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"

// M205 — declarations only; the implementation lives in its own translation
// unit (src/vendor/stb_image_write_impl.cpp) so it can be compiled without
// -Wall -Wextra -Werror.
#include "stb_image_write.h"

namespace MeshWorld {

namespace {

// M205 raster helpers. Deliberately simple (Bresenham line, disc fill) —
// this is an offline-preview renderer, not a general 2D graphics library.
// filled-rect/blend/triangle-fill (M-fix) follow the same philosophy: just
// enough to fix a specific, confirmed rendering bug, not a general polygon
// rasterizer.

void set_pixel(RgbImage& img, int x, int y, const std::array<std::uint8_t, 3>& color) {
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
    const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.w) +
                              static_cast<std::size_t>(x)) * 3;
    img.pixels[idx + 0] = color[0];
    img.pixels[idx + 1] = color[1];
    img.pixels[idx + 2] = color[2];
}

void draw_disc(RgbImage& img, int cx, int cy, int radius, const std::array<std::uint8_t, 3>& color) {
    for (int oy = -radius; oy <= radius; ++oy)
        for (int ox = -radius; ox <= radius; ++ox)
            if (ox * ox + oy * oy <= radius * radius)
                set_pixel(img, cx + ox, cy + oy, color);
}

// M-fix (lake render): a solid w x h block, used to paint one Lake point as
// its own filled grid cell rather than connecting it to the next point with
// a line -- see render_png_biome_image()'s own Lake-handling comment for why.
void draw_filled_rect(RgbImage& img, int x0, int y0, int w, int h,
                       const std::array<std::uint8_t, 3>& color) {
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            set_pixel(img, x, y, color);
}

// M-fix (border fill): blends `color` into whatever is already at (x, y)
// instead of overwriting it, so a filled country polygon reads as a tint
// over the terrain underneath rather than a flat opaque shape.
void blend_pixel(RgbImage& img, int x, int y, const std::array<std::uint8_t, 3>& color, float alpha) {
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
    const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.w) +
                              static_cast<std::size_t>(x)) * 3;
    for (int ch = 0; ch < 3; ++ch) {
        const float blended = static_cast<float>(img.pixels[idx + static_cast<std::size_t>(ch)]) * (1.0f - alpha) +
                               static_cast<float>(color[static_cast<std::size_t>(ch)]) * alpha;
        img.pixels[idx + static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(std::round(blended));
    }
}

// M-fix (border fill): standard edge-function/half-space point-in-triangle
// scanline fill over the triangle's own bounding box -- used to rasterize
// each earcut-produced triangle of a Border polygon. Sign convention doesn't
// matter here (unlike a real 3D rasterizer) since every triangle is filled
// solid, not shaded by winding.
void fill_triangle_blend(RgbImage& img, const std::array<int, 2>& p0, const std::array<int, 2>& p1,
                          const std::array<int, 2>& p2, const std::array<std::uint8_t, 3>& color,
                          float alpha) {
    const int min_x = std::max(0, std::min({p0[0], p1[0], p2[0]}));
    const int max_x = std::min(img.w - 1, std::max({p0[0], p1[0], p2[0]}));
    const int min_y = std::max(0, std::min({p0[1], p1[1], p2[1]}));
    const int max_y = std::min(img.h - 1, std::max({p0[1], p1[1], p2[1]}));
    const auto edge = [](const std::array<int, 2>& a, const std::array<int, 2>& b, int px, int py) {
        return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
    };
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const int d0 = edge(p0, p1, x, y);
            const int d1 = edge(p1, p2, x, y);
            const int d2 = edge(p2, p0, x, y);
            const bool has_neg = (d0 < 0) || (d1 < 0) || (d2 < 0);
            const bool has_pos = (d0 > 0) || (d1 > 0) || (d2 > 0);
            if (!(has_neg && has_pos)) blend_pixel(img, x, y, color, alpha);
        }
    }
}

// Bresenham's line algorithm with a square "brush" of the given thickness.
void draw_line(RgbImage& img, int x0, int y0, int x1, int y1,
               const std::array<std::uint8_t, 3>& color, int thickness) {
    const int dx  = std::abs(x1 - x0);
    const int sx  = x0 < x1 ? 1 : -1;
    const int dy  = -std::abs(y1 - y0);
    const int sy  = y0 < y1 ? 1 : -1;
    int       err = dx + dy;
    const int half = std::max(0, thickness / 2);

    for (;;) {
        for (int oy = -half; oy <= half; ++oy)
            for (int ox = -half; ox <= half; ++ox)
                set_pixel(img, x0 + ox, y0 + oy, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// M231 — stbi_write_png_to_func()'s callback, appending encoded bytes
// straight into a std::vector so a tile's PNG never touches disk as a loose
// file before going into the MBTiles SQLite blob.
void append_to_vector(void* context, void* data, int size) {
    auto*        out   = static_cast<std::vector<unsigned char>*>(context);
    const auto*  bytes = static_cast<unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

std::vector<unsigned char> encode_png_to_memory(const RgbImage& image) {
    std::vector<unsigned char> out;
    stbi_write_png_to_func(&append_to_vector, &out, image.w, image.h, /*comp=*/3, image.pixels.data(),
                            /*stride_in_bytes=*/image.w * 3);
    return out;
}

// Runs `sql` with no bound parameters and no result rows expected (DDL).
void exec_or_throw(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("export_mbtiles: " + msg);
    }
}

void insert_mbtiles_metadata(sqlite3* db, const std::string& name, const std::string& value) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO metadata (name, value) VALUES (?, ?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// M210 — every quadtree level `world_dir` already has a map_level{N}.db file
// for, ascending by level. Found by directory scan rather than probing
// levels 0..MAX_LEVEL, since MapTileStore's constructor creates the DB file
// on first open and validate_world() must not conjure new (empty) levels
// into existence as a side effect of checking what's already there.
std::vector<int> discover_persisted_levels(const std::string& world_dir) {
    static const std::string kPrefix = "map_level";
    static const std::string kSuffix = ".db";

    std::vector<int> levels;
    std::error_code  ec;
    for (const auto& entry : std::filesystem::directory_iterator(world_dir, ec)) {
        if (ec) break;
        const std::string name = entry.path().filename().string();
        if (name.size() <= kPrefix.size() + kSuffix.size()) continue;
        if (name.compare(0, kPrefix.size(), kPrefix) != 0) continue;
        if (name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) continue;

        const std::string number = name.substr(kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
        const bool all_digits = !number.empty() &&
            std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        if (all_digits) levels.push_back(std::stoi(number));
    }
    std::sort(levels.begin(), levels.end());
    return levels;
}

} // namespace

Map::PlanetParams planet_params_from_config(const WorldConfig& cfg) {
    Map::PlanetParams p;
    p.planet_size_m  = cfg.planet_size_m;
    p.continents_min = cfg.continents_min;
    p.continents_max = cfg.continents_max;
    p.sea_level_m    = cfg.sea_level_m;
    p.equator_temp_c = cfg.equator_temp_c;
    p.pole_temp_c    = cfg.pole_temp_c;
    return p;
}

bool is_existing_planet_world(const std::string& dir) {
    WorldConfig probe;
    return probe.load_from_file(dir + "/world.json") && probe.world_entropy != 0;
}

std::pair<PlanetMapCliArgs, bool> parse_planet_map_args(int argc, char* argv[]) {
    PlanetMapCliArgs args;
    if (argc < 2) return {args, false};
    args.dir = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--level" && i + 1 < argc) {
            args.tile.level = std::stoi(argv[++i]);
        } else if (arg == "--tile" && i + 1 < argc) {
            const std::string xy    = argv[++i];
            const auto        comma = xy.find(',');
            if (comma == std::string::npos) return {args, false};
            args.tile.x = std::stoll(xy.substr(0, comma));
            args.tile.y = std::stoll(xy.substr(comma + 1));
        } else if (arg == "--stats") {
            args.stats = true;
        } else if (arg == "--ascii") {
            args.ascii = true;
        } else if (arg == "--names") {
            args.names = true;
        } else if (arg == "--validate") {
            args.validate = true;
        } else if (arg == "--db-sizes") {
            args.db_sizes = true;
        } else if (arg == "--png" && i + 1 < argc) {
            args.png_path = argv[++i];
        } else if (arg == "--png-region" && i + 1 < argc) {
            args.png_region_path = argv[++i];
        } else if (arg == "--region-tiles" && i + 1 < argc) {
            args.region_tiles = std::stoi(argv[++i]);
        } else if (arg == "--geojson" && i + 1 < argc) {
            args.geojson_path = argv[++i];
        } else if (arg == "--mbtiles" && i + 1 < argc) {
            args.mbtiles_path = argv[++i];
        } else if (arg == "--legend" && i + 1 < argc) {
            args.legend_path = argv[++i];
        } else if (arg == "--png-mode" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "biome") {
                args.png_hillshade = false;
            } else if (mode == "hillshade") {
                args.png_hillshade = true;
            } else {
                return {args, false};
            }
        } else {
            return {args, false};
        }
    }

    if (!args.png_region_path.empty() && args.region_tiles < 1) return {args, false};
    if (args.tile.level < 0 || args.tile.level > Map::MAX_LEVEL) return {args, false};
    const int64_t span = int64_t{1} << args.tile.level;
    if (args.tile.x < 0 || args.tile.x >= span) return {args, false};
    if (args.tile.y < 0 || args.tile.y >= span) return {args, false};

    return {args, true};
}

TileSummary summarize_tile(const Map::MapTilePayload& payload) {
    TileSummary s;
    bool first = true;
    for (int gy = 0; gy < payload.biome.h; ++gy) {
        for (int gx = 0; gx < payload.biome.w; ++gx) {
            if (is_ocean_family(static_cast<ZoneType>(payload.biome.at(gx, gy))))
                ++s.ocean_cells;
            else
                ++s.land_cells;

            const float e = payload.elevation.at(gx, gy);
            if (first) { s.elevation_min = s.elevation_max = e; first = false; }
            else {
                s.elevation_min = std::min(s.elevation_min, e);
                s.elevation_max = std::max(s.elevation_max, e);
            }
        }
    }
    return s;
}

std::array<int, ZONE_NAMES.size()> biome_histogram(const Map::BiomeGrid& biome) {
    std::array<int, ZONE_NAMES.size()> hist{};
    for (const std::uint8_t v : biome.data)
        if (v < hist.size()) ++hist[v];
    return hist;
}

std::array<int, FEATURE_NAMES.size()> feature_histogram(const std::vector<Map::MapFeature>& features) {
    std::array<int, FEATURE_NAMES.size()> hist{};
    for (const Map::MapFeature& f : features) {
        const auto ordinal = static_cast<std::size_t>(f.type);
        if (ordinal < hist.size()) ++hist[ordinal];
    }
    return hist;
}

// Same letter-per-ZoneType mapping as src/tools/print_map.cpp's zone_char(),
// indexed by ordinal instead of switching on the enum (that file's WorldMap
// grid and this one's BiomeGrid both ultimately index the same ZoneType).
char zone_ascii_char(int zone_ordinal) {
    // Grown 12->52, M235 (MAP16, 2026-07-10): the 11 remaining unused
    // uppercase letters (A,G,H,I,K,L,N,P,Q,R,U,W,X,Y,Z minus one, since
    // there are 15 free uppercase but only 40 new entries need 25 more
    // after that) come first, then lowercase a-y for the rest -- purely
    // for uniqueness, not mnemonics (same as several of the original 12,
    // e.g. V=cave, E=meadow). Order must match ZONE_NAMES exactly.
    static constexpr std::array<char, ZONE_NAMES.size()> kChars = {
        'C', 'J', 'D', 'F', 'O', 'M', 'T', 'S', 'V', 'E', 'B',

        'A', 'G', 'H', 'I', 'K',

        'L', 'N', 'P', 'Q', 'R', 'U', 'W', 'X',

        'Y', 'Z', 'a', 'b',

        'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',

        'k', 'l', 'm', 'n',

        'o', 'p', 'q', 'r',

        's', 't', 'u', 'v', 'w', 'x', 'y',

        '.'
    };
    return (zone_ordinal >= 0 && static_cast<std::size_t>(zone_ordinal) < kChars.size())
               ? kChars[static_cast<std::size_t>(zone_ordinal)]
               : '?';
}

std::string render_ascii_biome_map(const Map::BiomeGrid& biome) {
    std::string out;
    for (int gy = 0; gy < biome.h; ++gy) {
        for (int gx = 0; gx < biome.w; ++gx) {
            if (gx > 0) out += ' ';
            out += zone_ascii_char(biome.at(gx, gy));
        }
        out += '\n';
    }
    return out;
}

std::vector<NamedEntry> collect_names(const Map::MapTilePayload& payload) {
    std::vector<NamedEntry> out;
    for (const Map::MapFeature& f : payload.features) {
        if (f.name.empty()) continue;
        const auto ordinal = static_cast<std::size_t>(f.type);
        const std::string kind = ordinal < FEATURE_NAMES.size() ? FEATURE_NAMES[ordinal] : "other";
        out.push_back({kind, f.name});
    }
    for (const Map::PlaceLabel& l : payload.labels) {
        if (l.name.empty()) continue;
        out.push_back({l.kind, l.name});
    }
    return out;
}

void print_names_list(const Map::MapTilePayload& payload) {
    const auto entries = collect_names(payload);

    std::cout << "\n--- Names (M209) ---\n";
    if (entries.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    for (const NamedEntry& e : entries)
        std::cout << "  " << e.kind << ": " << e.name << "\n";
}

void print_feature_stats(const Map::MapTilePayload& payload) {
    const TileSummary s     = summarize_tile(payload);
    const int         total = s.land_cells + s.ocean_cells;
    const auto        hist  = feature_histogram(payload.features);

    std::cout << "\n--- Stats (M207) ---\n";
    std::cout << "  land_ratio:     " << (total > 0 ? 100.0 * s.land_cells / total : 0.0) << "%\n";
    std::cout << "  continents:     " << hist[static_cast<std::size_t>(Map::FeatureType::Continent)] << "\n";
    std::cout << "  rivers:         " << hist[static_cast<std::size_t>(Map::FeatureType::River)] << "\n";
    std::cout << "  mountain_ranges:" << hist[static_cast<std::size_t>(Map::FeatureType::MountainRange)] << "\n";
    std::cout << "  cities:         " << hist[static_cast<std::size_t>(Map::FeatureType::City)] << "\n";
    std::cout << "  towns:          " << hist[static_cast<std::size_t>(Map::FeatureType::Town)] << "\n";
    std::cout << "  borders:        " << hist[static_cast<std::size_t>(Map::FeatureType::Border)] << "\n";
    std::cout << "  roads:          " << hist[static_cast<std::size_t>(Map::FeatureType::Road)] << "\n";
}

void print_tile_summary(const std::string& dir, const PlanetWorld& world,
                         const Map::TileCoord& tile, const Map::MapTilePayload& payload,
                         bool freshly_generated) {
    const TileSummary s     = summarize_tile(payload);
    const int         total = s.land_cells + s.ocean_cells;

    std::cout << "MeshWorldPlanet — " << dir << "\n";
    std::cout << "  world_entropy:  " << world.world_entropy() << "\n";
    std::cout << "  planet_size_m:  " << world.config().planet_size_m << "\n";
    std::cout << "  tile:           z=" << tile.level << " x=" << tile.x << " y=" << tile.y
               << " (" << (freshly_generated ? "freshly generated" : "loaded from DB") << ")\n";
    std::cout << "  generator:      " << payload.generator << "\n";
    std::cout << "  features:       " << payload.features.size() << "\n";
    std::cout << "  grid:           " << payload.elevation.w << "x" << payload.elevation.h << "\n";
    std::cout << "  land/ocean:     " << s.land_cells << "/" << s.ocean_cells << " cells ("
               << (total > 0 ? 100.0 * s.land_cells / total : 0.0) << "% land)\n";
    std::cout << "  elevation:      [" << s.elevation_min << ", " << s.elevation_max << "] m\n";

    std::cout << "  biome histogram:";
    const auto hist = biome_histogram(payload.biome);
    bool       any  = false;
    for (std::size_t i = 0; i < hist.size(); ++i) {
        if (hist[i] > 0) {
            std::cout << " " << ZONE_NAMES[i] << "=" << hist[i];
            any = true;
        }
    }
    if (!any) std::cout << " (none)";
    std::cout << "\n";
}

// WorldConfig/WorldMap here are minimal/default — their zone assignment is
// irrelevant because map_context overrides ctx.zone once available (M157);
// ChunkCoord values from deep tiles fall outside the default 20x20 WorldMap
// grid, which is fine (WorldMap::info() returns an empty ChunkInfo for
// out-of-bounds chunks, and that's exactly what gets overridden).
HandoffResult run_handoff(const Map::TileCoord& tile, Map::MapPipeline& pipeline) {
    const WorldConfig chunk_cfg;
    const WorldMap     chunk_map(chunk_cfg);
    ChunkPipeline       chunk_pipeline(chunk_cfg, chunk_map, ChunkCache{}, &pipeline);

    const Map::ChunkRange range = tile.chunk_range(chunk_cfg.chunk_size_m);
    const ChunkCoord chunk{static_cast<int32_t>(range.x_min), static_cast<int32_t>(range.y_min)};

    HandoffResult r;
    r.chunk   = chunk;
    r.mc3_xml = chunk_pipeline.get(chunk.x, chunk.y);
    r.ctx     = chunk_pipeline.build_context(chunk.x, chunk.y);
    return r;
}

std::array<std::uint8_t, 3> zone_rgb_color(int zone_ordinal) {
    // Grown 12->52, M235 (MAP16, 2026-07-10): a coherent palette designed
    // as one pass across all 40 new families (rather than 40 independent
    // picks in 40 separate later tasks) so hues stay distinct both from
    // the original 12 and from each other within a family. Order must
    // match ZONE_NAMES/the ZoneType enum exactly.
    static constexpr std::array<std::array<std::uint8_t, 3>, ZONE_NAMES.size()> kColors = {{
        {150, 150, 150},  // city
        {10, 100, 10},    // jungle
        {210, 180, 90},   // desert
        {34, 120, 34},    // forest
        {30, 90, 200},    // ocean
        {120, 110, 100},  // mountain
        {210, 220, 225},  // tundra
        {85, 95, 60},     // swamp
        {40, 40, 45},     // cave
        {140, 200, 90},   // meadow
        {235, 215, 150},  // beach

        {200, 190, 100},  // savanna
        {180, 170, 110},  // steppe
        {170, 195, 100},  // prairie
        {160, 150, 80},   // chaparral
        {150, 140, 90},   // shrubland

        {20, 80, 60},     // taiga
        {15, 90, 50},     // temperate_rainforest
        {45, 110, 45},    // mixed_forest
        {40, 130, 90},    // cloud_forest
        {30, 100, 80},    // mangrove
        {90, 150, 50},    // bamboo_forest
        {50, 130, 70},    // riparian_forest
        {110, 130, 50},   // tropical_dry_forest

        {100, 110, 70},   // marsh
        {130, 140, 90},   // floodplain
        {70, 80, 55},     // bog
        {60, 85, 70},     // muskeg

        {225, 195, 120},  // dunes
        {170, 140, 100},  // rocky_desert
        {180, 175, 150},  // cold_desert
        {235, 230, 220},  // salt_flat
        {160, 110, 80},   // badlands
        {180, 120, 70},   // mesa
        {150, 95, 65},    // canyon
        {90, 180, 120},   // oasis

        {225, 240, 250},  // glacier
        {160, 165, 155},  // permafrost
        {170, 200, 140},  // alpine_meadow
        {245, 250, 255},  // ice_cap

        {80, 40, 30},     // volcanic
        {200, 100, 60},   // geothermal
        {100, 90, 85},    // ash_plain
        {60, 70, 55},     // volcanic_island

        {60, 180, 190},   // coral_reef
        {40, 110, 110},   // kelp_forest
        {10, 40, 110},    // deep_ocean
        {70, 190, 200},   // lagoon
        {50, 90, 130},    // fjord
        {160, 170, 140},  // tidal_flat
        {110, 100, 95},   // sea_cliff

        {245, 245, 245},  // empty
    }};
    return (zone_ordinal >= 0 && static_cast<std::size_t>(zone_ordinal) < kColors.size())
               ? kColors[static_cast<std::size_t>(zone_ordinal)]
               : std::array<std::uint8_t, 3>{255, 0, 255};  // unknown ordinal -> magenta
}

std::array<std::uint8_t, 3> feature_rgb_color(Map::FeatureType type) {
    switch (type) {
        case Map::FeatureType::River:         return {30, 110, 230};
        case Map::FeatureType::Lake:          return {20, 80, 180};
        case Map::FeatureType::MountainRange: return {139, 90, 43};
        case Map::FeatureType::Coastline:     return {10, 10, 10};
        case Map::FeatureType::Border:        return {200, 30, 30};
        case Map::FeatureType::City:          return {240, 240, 240};
        case Map::FeatureType::Town:          return {200, 200, 200};
        case Map::FeatureType::Road:          return {90, 60, 30};
        case Map::FeatureType::Continent:     return {34, 100, 34};
        case Map::FeatureType::Street:        return {120, 120, 120};
        case Map::FeatureType::Park:          return {34, 139, 34};
        case Map::FeatureType::Other:         break;
    }
    return {200, 0, 200};
}

std::array<std::uint8_t, 3> label_rgb_color(const std::string& kind) {
    if (kind == "capital")   return {255, 215, 0};
    if (kind == "city")      return {255, 255, 255};
    if (kind == "town")      return {200, 200, 200};
    if (kind == "village")   return {160, 160, 160};
    if (kind == "country")   return {255, 140, 0};
    if (kind == "continent") return {220, 20, 60};
    if (kind == "river")     return {135, 206, 235};
    return {90, 90, 90};
}

// M346 (MAP22) -- importance ranking for label declutter (lower = more
// important, drawn/kept first). Mirrors label_rgb_color()'s own already-
// established kind ordering above rather than inventing a separate scheme.
int label_priority(const std::string& kind) {
    if (kind == "continent") return 0;
    if (kind == "country")   return 1;
    if (kind == "capital")   return 2;
    if (kind == "city")      return 3;
    if (kind == "town")      return 4;
    if (kind == "village")   return 5;
    if (kind == "river")     return 6;
    return 7;
}

RgbImage render_png_biome_image(const Map::MapTilePayload& payload, int upscale) {
    upscale = std::max(1, upscale);

    RgbImage img;
    img.w = payload.biome.w * upscale;
    img.h = payload.biome.h * upscale;
    img.pixels.assign(static_cast<std::size_t>(img.w) * static_cast<std::size_t>(img.h) * 3, 0);

    // M344 (MAP22) — bilinearly blend each pixel between the biome colors
    // of the up to 4 grid cells nearest to it (in CELL-CENTER space, not
    // cell-corner space), rather than flat-filling every pixel in a cell's
    // own upscale×upscale block with that one cell's color. A pixel at the
    // exact center of its own cell's block still resolves to that cell's
    // pure color (weight 1.0 for itself, 0 for every neighbor); only
    // pixels nearer a cell's own edge pick up a real blend toward whatever
    // sits across it — this is what turns the previous hard, blocky
    // biome-color edges (especially visible now that MAP16 raised the
    // biome count to 52, giving many more distinct adjacent colors to
    // clash at every boundary) into a smooth transition zone. Grid-edge
    // cells clamp to themselves (no lookahead across the tile's own
    // boundary, same convention Coastline::trace()/M343 already use).
    if (payload.biome.w > 0 && payload.biome.h > 0) {
        const int bw = payload.biome.w;
        const int bh = payload.biome.h;
        for (int py = 0; py < img.h; ++py) {
            for (int px = 0; px < img.w; ++px) {
                const float fx = (static_cast<float>(px) + 0.5f) / static_cast<float>(upscale) - 0.5f;
                const float fy = (static_cast<float>(py) + 0.5f) / static_cast<float>(upscale) - 0.5f;
                const int   gx0 = static_cast<int>(std::floor(fx));
                const int   gy0 = static_cast<int>(std::floor(fy));
                const float tx  = fx - static_cast<float>(gx0);
                const float ty  = fy - static_cast<float>(gy0);
                const int   gx0c = std::clamp(gx0, 0, bw - 1);
                const int   gx1c = std::clamp(gx0 + 1, 0, bw - 1);
                const int   gy0c = std::clamp(gy0, 0, bh - 1);
                const int   gy1c = std::clamp(gy0 + 1, 0, bh - 1);

                const auto c00 = zone_rgb_color(payload.biome.at(gx0c, gy0c));
                const auto c10 = zone_rgb_color(payload.biome.at(gx1c, gy0c));
                const auto c01 = zone_rgb_color(payload.biome.at(gx0c, gy1c));
                const auto c11 = zone_rgb_color(payload.biome.at(gx1c, gy1c));

                std::array<std::uint8_t, 3> color{};
                for (int ch = 0; ch < 3; ++ch) {
                    const float top = static_cast<float>(c00[static_cast<std::size_t>(ch)]) * (1.0f - tx) +
                                       static_cast<float>(c10[static_cast<std::size_t>(ch)]) * tx;
                    const float bot = static_cast<float>(c01[static_cast<std::size_t>(ch)]) * (1.0f - tx) +
                                       static_cast<float>(c11[static_cast<std::size_t>(ch)]) * tx;
                    const float blended = top * (1.0f - ty) + bot * ty;
                    color[static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(std::round(blended));
                }
                set_pixel(img, px, py, color);
            }
        }
    }

    // MapFeature/PlaceLabel coordinates are absolute world-space meters
    // (Hydrology::trace(), Settlements::appendLabels(), ... all populate
    // them via world_x()/world_z() over the tile's own world_bounds()), so
    // mapping into this image's pixel space is a straight normalize-by-tile-
    // extent, not a grid-index lookup.
    const Map::WorldBounds b  = payload.tile.world_bounds();
    const double           bw = b.max_x - b.min_x;
    const double           bh = b.max_z - b.min_z;
    const auto to_px = [&](const std::array<double, 2>& p) -> std::pair<int, int> {
        const double u = bw > 0.0 ? (p[0] - b.min_x) / bw : 0.0;
        const double v = bh > 0.0 ? (p[1] - b.min_z) / bh : 0.0;
        return {static_cast<int>(u * img.w), static_cast<int>(v * img.h)};
    };

    const int line_thickness = upscale >= 4 ? 2 : 1;
    constexpr float kBorderFillAlpha = 0.18f;
    for (const Map::MapFeature& f : payload.features) {
        const auto color = feature_rgb_color(f.type);

        // M-fix (lake render): Lake.points is every wet grid cell (Hydrology::
        // fill_basin()'s priority-flood pop order / city.lua's/metro.lua's own
        // near-sea-level scan, see their header comments) -- an unordered
        // interior cell CLOUD, not a traced shoreline ring. Connecting them
        // with the polyline path below draws a chaotic tangle of lines across
        // the whole wet area (the original "looks terrible" bug). Paint each
        // point as its own filled, cell-sized block instead: adjacent wet
        // cells merge into one solid blob, which is what this data actually
        // represents. Falls through to nothing else for Lake -- no line, no
        // single-point disc.
        if (f.type == Map::FeatureType::Lake) {
            for (const auto& p : f.points) {
                const auto [x, y] = to_px(p);
                draw_filled_rect(img, x - upscale / 2, y - upscale / 2, upscale, upscale, color);
            }
            continue;
        }

        // M-fix (border fill): unlike Lake above, Country::border (Countries::
        // trace_owner_border()) IS a real ordered polygon ring, so it's a
        // genuine earcut use case -- triangulate in pixel space and blend a
        // light tint over the territory so a country's extent reads as a
        // shape, not just an outline. The outline itself still gets drawn by
        // the ordinary polyline path just below, on top of this tint.
        if (f.type == Map::FeatureType::Border && f.points.size() >= 3) {
            std::vector<std::array<double, 2>> px_ring;
            px_ring.reserve(f.points.size());
            for (const auto& p : f.points) {
                const auto [x, y] = to_px(p);
                px_ring.push_back({static_cast<double>(x), static_cast<double>(y)});
            }
            const std::vector<std::vector<std::array<double, 2>>> polygon{px_ring};
            const std::vector<std::uint32_t> tri = mapbox::earcut<std::uint32_t>(polygon);
            for (std::size_t t = 0; t + 2 < tri.size(); t += 3) {
                const auto& pa = px_ring[tri[t]];
                const auto& pb = px_ring[tri[t + 1]];
                const auto& pc = px_ring[tri[t + 2]];
                fill_triangle_blend(img, {static_cast<int>(pa[0]), static_cast<int>(pa[1])},
                                     {static_cast<int>(pb[0]), static_cast<int>(pb[1])},
                                     {static_cast<int>(pc[0]), static_cast<int>(pc[1])}, color,
                                     kBorderFillAlpha);
            }
        }

        if (f.points.size() >= 2) {
            for (std::size_t i = 0; i + 1 < f.points.size(); ++i) {
                const auto [x0, y0] = to_px(f.points[i]);
                const auto [x1, y1] = to_px(f.points[i + 1]);
                draw_line(img, x0, y0, x1, y1, color, line_thickness);
            }
        } else if (f.points.size() == 1) {
            const auto [x, y] = to_px(f.points[0]);
            draw_disc(img, x, y, std::max(2, upscale / 2), color);
        }
    }

    // M346 (MAP22) -- avoid drawing overlapping label markers: sort by
    // importance (label_priority(), matching label_rgb_color()'s own
    // established kind ordering) then greedily keep a label only if its
    // marker wouldn't overlap an already-kept, higher-priority one's
    // marker in PIXEL space. A crowded low-zoom render (many settlements
    // compressed into a small image) would otherwise draw many same-size
    // discs stacked on top of each other into an unreadable blob. GeoJSON
    // export (render_geojson(), below) keeps every label unconditionally
    // -- a vector consumer can zoom/declutter dynamically, a fixed-
    // resolution PNG cannot -- but tags each with this same priority so a
    // consumer CAN choose to replicate this exact declutter logic itself.
    std::vector<std::size_t> label_order(payload.labels.size());
    for (std::size_t i = 0; i < label_order.size(); ++i) label_order[i] = i;
    std::stable_sort(label_order.begin(), label_order.end(), [&](std::size_t a, std::size_t b) {
        return label_priority(payload.labels[a].kind) < label_priority(payload.labels[b].kind);
    });

    const int label_radius   = std::max(2, upscale / 3);
    const int min_separation = 2 * label_radius;
    const int min_sep_sq     = min_separation * min_separation;
    std::vector<std::pair<int, int>> kept_label_px;
    kept_label_px.reserve(payload.labels.size());
    for (std::size_t idx : label_order) {
        const Map::PlaceLabel& l      = payload.labels[idx];
        const auto [x, y]             = to_px(l.pos);
        bool collides = false;
        for (const auto& [kx, ky] : kept_label_px) {
            const int dx = x - kx, dy = y - ky;
            if (dx * dx + dy * dy < min_sep_sq) { collides = true; break; }
        }
        if (collides) continue;
        draw_disc(img, x, y, label_radius, label_rgb_color(l.kind));
        kept_label_px.push_back({x, y});
    }

    return img;
}

RgbImage render_png_elevation_image(const Map::MapTilePayload& payload, int upscale,
                                     const Map::FieldGrid* north, const Map::FieldGrid* east,
                                     const Map::FieldGrid* south, const Map::FieldGrid* west) {
    upscale = std::max(1, upscale);
    const Map::FieldGrid& e = payload.elevation;

    RgbImage img;
    img.w = e.w * upscale;
    img.h = e.h * upscale;
    img.pixels.assign(static_cast<std::size_t>(img.w) * static_cast<std::size_t>(img.h) * 3, 0);
    if (e.empty()) return img;

    // A neighbor only usable if it's a real, matching-sized grid -- a
    // mismatched or empty one is treated exactly like "no neighbor passed".
    const auto usable = [&](const Map::FieldGrid* n) {
        return n != nullptr && !n->empty() && n->w == e.w && n->h == e.h;
    };
    const bool has_n = usable(north), has_e = usable(east), has_s = usable(south), has_w = usable(west);

    // Fixed light direction from the upper-left, normalized (-1,-1,1) --
    // the conventional azimuth=315deg GIS hillshade default.
    constexpr float kLightX = -0.5773503f, kLightY = -0.5773503f, kLightZ = 0.5773503f;
    // Relates one grid cell's elevation delta (meters) to one grid cell's
    // run for the surface-normal estimate; larger = flatter-looking (less
    // exaggerated relief). Tuned for this generator's typical elevation
    // range (roughly [-4000, 4000] m over a 64-cell tile), not a literal
    // real-world meters-per-cell distance (FieldGrid doesn't carry one).
    constexpr float kReliefScale = 50.0f;

    for (int gy = 0; gy < e.h; ++gy) {
        for (int gx = 0; gx < e.w; ++gx) {
            // M343 (MAP22) — a true central-difference slope estimate needs
            // one sample on EACH side of (gx,gy); at this tile's own edge,
            // "the other side" is the matching row/column of the real
            // neighboring tile (when passed) instead of clamping back onto
            // this tile's own boundary value (a one-sided, systematically
            // different estimate that produced a visible brightness seam
            // in a stitched multi-tile render).
            const float left_val  = (gx > 0)          ? e.at(gx - 1, gy)
                                   : has_w             ? west->at(west->w - 1, gy)
                                                        : e.at(0, gy);
            const float right_val = (gx < e.w - 1)     ? e.at(gx + 1, gy)
                                   : has_e             ? east->at(0, gy)
                                                        : e.at(e.w - 1, gy);
            const float up_val    = (gy > 0)          ? e.at(gx, gy - 1)
                                   : has_n             ? north->at(gx, north->h - 1)
                                                        : e.at(gx, 0);
            const float down_val  = (gy < e.h - 1)     ? e.at(gx, gy + 1)
                                   : has_s             ? south->at(gx, 0)
                                                        : e.at(gx, e.h - 1);
            const float dzdx = (right_val - left_val) * 0.5f;
            const float dzdy = (down_val - up_val) * 0.5f;

            float nx = -dzdx, ny = -dzdy, nz = kReliefScale;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;

            const float brightness = std::clamp(nx * kLightX + ny * kLightY + nz * kLightZ, 0.0f, 1.0f);
            const auto  gray       = static_cast<std::uint8_t>(brightness * 255.0f);
            const std::array<std::uint8_t, 3> color{gray, gray, gray};

            for (int oy = 0; oy < upscale; ++oy)
                for (int ox = 0; ox < upscale; ++ox)
                    set_pixel(img, gx * upscale + ox, gy * upscale + oy, color);
        }
    }

    return img;
}

bool write_png_file(const std::string& path, const RgbImage& image) {
    if (image.w <= 0 || image.h <= 0) return false;
    return stbi_write_png(path.c_str(), image.w, image.h, /*comp=*/3, image.pixels.data(),
                           /*stride_in_bytes=*/image.w * 3) != 0;
}

RgbImage render_zone_legend_image(int swatch_px, int gap_px, int cols) {
    const int n    = static_cast<int>(ZONE_NAMES.size());
    const int rows = (n + cols - 1) / cols;

    RgbImage img;
    img.w = cols * swatch_px + (cols + 1) * gap_px;
    img.h = rows * swatch_px + (rows + 1) * gap_px;
    img.pixels.assign(static_cast<std::size_t>(img.w) * static_cast<std::size_t>(img.h) * 3, 30);

    for (int ordinal = 0; ordinal < n; ++ordinal) {
        const int   col   = ordinal % cols;
        const int   row   = ordinal / cols;
        const int   x0    = gap_px + col * (swatch_px + gap_px);
        const int   y0    = gap_px + row * (swatch_px + gap_px);
        const auto  color = zone_rgb_color(ordinal);
        for (int oy = 0; oy < swatch_px; ++oy)
            for (int ox = 0; ox < swatch_px; ++ox)
                set_pixel(img, x0 + ox, y0 + oy, color);
    }
    return img;
}

void print_zone_legend_list() {
    for (int ordinal = 0; ordinal < static_cast<int>(ZONE_NAMES.size()); ++ordinal) {
        const auto c = zone_rgb_color(ordinal);
        std::cout << "  " << ordinal << "\t" << ZONE_NAMES[static_cast<std::size_t>(ordinal)]
                  << "\t(" << static_cast<int>(c[0]) << "," << static_cast<int>(c[1]) << ","
                  << static_cast<int>(c[2]) << ")\n";
    }
}

RgbImage render_png_region_image(Map::MapPipeline& pipeline, const Map::TileCoord& top_left,
                                  int n, int upscale) {
    n       = std::max(1, n);
    upscale = std::max(1, upscale);
    const int64_t span = int64_t{1} << top_left.level;

    std::vector<RgbImage> tiles(static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
    std::vector<bool>     present(tiles.size(), false);
    int                   tile_w = 0, tile_h = 0;

    for (int dy = 0; dy < n; ++dy) {
        for (int dx = 0; dx < n; ++dx) {
            const int64_t tx = top_left.x + dx;
            const int64_t ty = top_left.y + dy;
            if (tx < 0 || tx >= span || ty < 0 || ty >= span) continue;  // out of range, left blank

            const Map::MapTilePayload payload = pipeline.get({top_left.level, tx, ty});
            RgbImage                  tile_image = render_png_biome_image(payload, upscale);
            if (tile_w == 0) { tile_w = tile_image.w; tile_h = tile_image.h; }

            const std::size_t idx = static_cast<std::size_t>(dy) * static_cast<std::size_t>(n) +
                                     static_cast<std::size_t>(dx);
            tiles[idx]   = std::move(tile_image);
            present[idx] = true;
        }
    }

    RgbImage canvas;
    if (tile_w == 0) return canvas;  // every requested tile was out of range

    canvas.w = tile_w * n;
    canvas.h = tile_h * n;
    canvas.pixels.assign(static_cast<std::size_t>(canvas.w) * static_cast<std::size_t>(canvas.h) * 3, 0);

    for (int dy = 0; dy < n; ++dy) {
        for (int dx = 0; dx < n; ++dx) {
            const std::size_t idx = static_cast<std::size_t>(dy) * static_cast<std::size_t>(n) +
                                     static_cast<std::size_t>(dx);
            if (!present[idx]) continue;
            const RgbImage& tile_image = tiles[idx];
            for (int y = 0; y < tile_image.h; ++y) {
                for (int x = 0; x < tile_image.w; ++x) {
                    const std::size_t src = (static_cast<std::size_t>(y) * static_cast<std::size_t>(tile_image.w) +
                                              static_cast<std::size_t>(x)) * 3;
                    const int cx = dx * tile_w + x;
                    const int cy = dy * tile_h + y;
                    const std::size_t dst = (static_cast<std::size_t>(cy) * static_cast<std::size_t>(canvas.w) +
                                              static_cast<std::size_t>(cx)) * 3;
                    canvas.pixels[dst + 0] = tile_image.pixels[src + 0];
                    canvas.pixels[dst + 1] = tile_image.pixels[src + 1];
                    canvas.pixels[dst + 2] = tile_image.pixels[src + 2];
                }
            }
        }
    }
    return canvas;
}

std::string render_geojson(const Map::MapTilePayload& payload) {
    using json = nlohmann::json;

    json features = json::array();

    for (const Map::MapFeature& f : payload.features) {
        if (f.points.empty()) continue;  // no geometry to export

        const auto        ordinal = static_cast<std::size_t>(f.type);
        const std::string kind    = ordinal < FEATURE_NAMES.size() ? FEATURE_NAMES[ordinal] : "other";

        json properties = {{"kind", kind}, {"name", f.name}};
        for (const auto& [key, value] : f.attributes) properties[key] = value;

        json geometry;
        if (f.points.size() == 1) {
            geometry = {{"type", "Point"}, {"coordinates", json::array({f.points[0][0], f.points[0][1]})}};
        } else {
            json coords = json::array();
            for (const auto& pt : f.points) coords.push_back(json::array({pt[0], pt[1]}));
            // Countries::grow() (MAP9) closes a border/region loop by
            // repeating its first point as the last; treat that shape as a
            // filled Polygon ring, any other multi-point path as a LineString.
            const bool closed = f.points.size() >= 4 && f.points.front()[0] == f.points.back()[0] &&
                                 f.points.front()[1] == f.points.back()[1];
            geometry = closed ? json{{"type", "Polygon"}, {"coordinates", json::array({coords})}}
                               : json{{"type", "LineString"}, {"coordinates", coords}};
        }

        features.push_back(
            json{{"type", "Feature"}, {"properties", std::move(properties)}, {"geometry", std::move(geometry)}});
    }

    // M346 (MAP22) -- GeoJSON keeps every label (unlike the fixed-resolution
    // PNG render above, a vector consumer can zoom/declutter dynamically,
    // so dropping data here would be presumptuous), except true exact
    // duplicates (same name+kind+position) -- a real data-hygiene issue
    // distinct from "would visually collide at some zoom level", e.g. two
    // independent label sources both appending the same settlement.
    // labelPriority uses the same importance ranking the PNG render's own
    // declutter pass does, so a consumer wanting matching behavior doesn't
    // need to reimplement the ranking itself.
    std::vector<std::array<double, 2>> seen_positions;
    std::vector<std::string>           seen_keys;
    for (const Map::PlaceLabel& l : payload.labels) {
        const std::string key = l.kind + "\x1f" + l.name;
        bool duplicate = false;
        for (std::size_t i = 0; i < seen_keys.size(); ++i) {
            if (seen_keys[i] == key && seen_positions[i][0] == l.pos[0] && seen_positions[i][1] == l.pos[1]) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;
        seen_keys.push_back(key);
        seen_positions.push_back(l.pos);

        json geometry   = {{"type", "Point"}, {"coordinates", json::array({l.pos[0], l.pos[1]})}};
        json properties = {{"kind", l.kind}, {"name", l.name}, {"labelPriority", label_priority(l.kind)}};
        features.push_back(
            json{{"type", "Feature"}, {"properties", std::move(properties)}, {"geometry", std::move(geometry)}});
    }

    const json root = {{"type", "FeatureCollection"}, {"features", std::move(features)}};
    return root.dump(2);
}

bool write_text_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out << text;
    return out.good();
}

int export_mbtiles(const std::string& world_dir, int level, const std::string& out_path, int upscale) {
    const Map::MapTileStore              store(world_dir, level);
    const std::vector<Map::TileCoord>    tiles = store.list_tiles();

    // Re-running --mbtiles against the same out_path is a normal "refresh
    // the export" workflow, not an error -- start from a clean file rather
    // than failing on CREATE TABLE against a previous run's leftover schema.
    std::error_code ec;
    std::filesystem::remove(out_path, ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(out_path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }

    exec_or_throw(db, "CREATE TABLE metadata (name TEXT, value TEXT);");
    exec_or_throw(db,
                  "CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, "
                  "tile_data BLOB);");
    exec_or_throw(db, "CREATE UNIQUE INDEX tile_index ON tiles (zoom_level, tile_column, tile_row);");

    insert_mbtiles_metadata(db, "name", "MeshWorld planet level " + std::to_string(level));
    insert_mbtiles_metadata(db, "format", "png");
    insert_mbtiles_metadata(db, "minzoom", std::to_string(level));
    insert_mbtiles_metadata(db, "maxzoom", std::to_string(level));

    const int64_t span = int64_t{1} << level;

    sqlite3_stmt* insert_stmt = nullptr;
    sqlite3_prepare_v2(
        db, "INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?);", -1,
        &insert_stmt, nullptr);

    int packaged = 0;
    for (const Map::TileCoord& tile : tiles) {
        const auto payload = store.load(tile);
        if (!payload) continue;  // listed but failed to decode -- skip, don't abort the whole export

        const RgbImage                    image    = render_png_biome_image(*payload, upscale);
        const std::vector<unsigned char>  png_data = encode_png_to_memory(image);
        // MBTiles uses the TMS tiling scheme (Y axis flipped relative to
        // this project's own XYZ-style TileCoord.y), per the spec's tiles
        // table convention.
        const int64_t tms_row = span - 1 - tile.y;

        sqlite3_reset(insert_stmt);
        sqlite3_bind_int(insert_stmt, 1, level);
        sqlite3_bind_int64(insert_stmt, 2, tile.x);
        sqlite3_bind_int64(insert_stmt, 3, tms_row);
        sqlite3_bind_blob(insert_stmt, 4, png_data.data(), static_cast<int>(png_data.size()), SQLITE_TRANSIENT);
        if (sqlite3_step(insert_stmt) == SQLITE_DONE) ++packaged;
    }
    sqlite3_finalize(insert_stmt);
    sqlite3_close(db);
    return packaged;
}

WorldValidationResult validate_world(const std::string& world_dir) {
    WorldValidationResult result;
    const MapValidator     validator;

    for (int level : discover_persisted_levels(world_dir)) {
        const Map::MapTileStore store(world_dir, level);
        for (const Map::TileCoord& tile : store.list_tiles()) {
            const auto payload = store.load(tile);
            if (!payload) continue;  // listed but failed to decode -- same skip export_mbtiles uses

            ++result.tiles_checked;
            ValidationResult tile_result = validator.validate(*payload);
            if (!tile_result.ok) result.issues.push_back({tile, std::move(tile_result.errors)});
        }
    }

    return result;
}

void print_world_validation_result(const WorldValidationResult& result) {
    std::cout << "\n--- Validation (M210) ---\n";
    std::cout << "  tiles checked:  " << result.tiles_checked << "\n";
    if (result.ok()) {
        std::cout << "  all tiles valid\n";
        return;
    }
    std::cout << "  " << result.issues.size() << " tile(s) with errors:\n";
    for (const TileValidationIssue& issue : result.issues) {
        std::cout << "  L" << issue.tile.level << " (" << issue.tile.x << "," << issue.tile.y << "):\n";
        for (const std::string& error : issue.errors) std::cout << "    - " << error << "\n";
    }
}

DbSizeReport audit_db_sizes(const std::string& world_dir) {
    DbSizeReport report;

    for (int level : discover_persisted_levels(world_dir)) {
        const std::string    path = world_dir + "/map_level" + std::to_string(level) + ".db";
        std::error_code       ec;
        const std::uintmax_t  size = std::filesystem::file_size(path, ec);
        if (ec) continue;  // listed by discover_persisted_levels() but gone by the time we stat it
        report.map_level_dbs.push_back({"map_level" + std::to_string(level) + ".db",
                                         static_cast<std::uint64_t>(size)});
        report.total_bytes += static_cast<std::uint64_t>(size);
    }

    // M11's region-sharded model-placement DBs (RegionShard.hpp's own
    // "<world_dir>/models/<rx>_<rz>.db" convention) -- a world that never
    // touched MAP11 model placements simply has no models/ directory yet,
    // same "nothing generated -> empty result" treatment as the map-level
    // scan above.
    const std::filesystem::path models_dir = std::filesystem::path(world_dir) / "models";
    std::error_code             dir_ec;
    for (const auto& entry : std::filesystem::directory_iterator(models_dir, dir_ec)) {
        if (dir_ec) break;
        const std::string name = entry.path().filename().string();
        if (name.size() <= 3 || name.compare(name.size() - 3, 3, ".db") != 0) continue;
        if (name.find('_') == std::string::npos) continue;  // not "<rx>_<rz>.db" shaped

        std::error_code       size_ec;
        const std::uintmax_t  size = std::filesystem::file_size(entry.path(), size_ec);
        if (size_ec) continue;
        report.region_dbs.push_back({"models/" + name, static_cast<std::uint64_t>(size)});
        report.total_bytes += static_cast<std::uint64_t>(size);
    }
    std::sort(report.region_dbs.begin(), report.region_dbs.end(),
              [](const DbSizeEntry& a, const DbSizeEntry& b) { return a.label < b.label; });

    return report;
}

namespace {
// M223 — human-readable byte count (e.g. "12.3 KB"), matching the scale
// most world-directory DB files actually fall in -- no need for anything
// past MB/GB for a display helper like this.
std::string format_bytes(std::uint64_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB"};
    double                        value    = static_cast<double>(bytes);
    std::size_t                   unit     = 0;
    while (value >= 1024.0 && unit + 1 < std::size(kUnits)) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(unit == 0 ? 0 : 1);
    oss << value << " " << kUnits[unit];
    return oss.str();
}
} // namespace

void print_db_size_report(const DbSizeReport& report) {
    std::cout << "\n--- DB size audit (M223) ---\n";
    if (report.map_level_dbs.empty() && report.region_dbs.empty()) {
        std::cout << "  (no map_level*.db or models/*.db files found)\n";
        return;
    }
    for (const DbSizeEntry& e : report.map_level_dbs)
        std::cout << "  " << e.label << ": " << format_bytes(e.size_bytes) << "\n";
    for (const DbSizeEntry& e : report.region_dbs)
        std::cout << "  " << e.label << ": " << format_bytes(e.size_bytes) << "\n";
    std::cout << "  total: " << format_bytes(report.total_bytes) << " ("
               << (report.map_level_dbs.size() + report.region_dbs.size()) << " file(s))\n";
}

void print_handoff_result(const std::string& dir, const HandoffResult& result) {
    const std::string path = dir + "/handoff_chunk_" + std::to_string(result.chunk.x) + "_" +
                              std::to_string(result.chunk.y) + ".mc3.xml";
    std::ofstream out(path);
    out << result.mc3_xml;

    std::cout << "\n--- Chunk hand-off demo (MAP10) ---\n";
    std::cout << "  chunk:          (" << result.chunk.x << ", " << result.chunk.y << ")\n";
    std::cout << "  map_context:    available=" << (result.ctx.map_context.available ? "true" : "false")
               << " zone=" << to_string(result.ctx.zone)
               << " elevation=" << result.ctx.map_context.elevation_m << "m\n";
    std::cout << "  mc3 bytes:      " << result.mc3_xml.size() << "\n";
    std::cout << "  saved to:       " << path << "\n";
}

} // namespace MeshWorld
