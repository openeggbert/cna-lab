// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldPlanet — headless CLI driver for the planetary map subsystem
// (MAP13, M203/M204 + hand-off demo). Creates or opens a PlanetWorld and
// prints a summary of one tile — level 0 (the whole planet) by default, or
// an arbitrary tile via --level/--tile. Then hands one chunk from that
// tile's range off to the existing ChunkPipeline (with the MapPipeline
// attached) and writes the resulting MC3 scene to disk. Part of the MAP0
// vertical slice: this is what actually *runs* the full planet→chunk descent
// as a program, rather than only exercising it in unit tests.
//
// Usage:
//   MeshWorldPlanet <world-dir> [--level Z --tile X,Y] [--stats] [--ascii] [--names]
//                    [--png out.png [--png-mode biome|hillshade]]
//                    [--png-region out.png [--region-tiles N]]
//                    [--geojson out.geojson] [--mbtiles out.mbtiles] [--validate]
//                    [--db-sizes]
//
// Creates a fresh planet world in <world-dir> if it doesn't already contain a
// world.json with planet fields; otherwise reopens the existing one (revisit
// consistency comes from persistence, never from re-deriving a fixed seed).
// --level/--tile descends to that quadtree tile (generating any missing
// ancestors along the way, same as MapPipeline::get() always does); with no
// flags, level 0 (the planet root) is used, matching M203's behavior.
// --stats (M207) additionally prints land ratio + continent/river/mountain-
// range/city/town/border/road counts for that one tile's payload.
// --ascii (M206) additionally prints that tile's BiomeGrid as a letter grid,
// reusing MeshWorldMap's (src/tools/print_map.cpp) zone-letter scheme.
// --names (M209) additionally lists every named MapFeature/PlaceLabel found
// in that one tile's payload (continents, rivers, borders, cities, ...).
// --png out.png (M205) additionally renders that tile to an image: the
// biome-color grid plus a feature/label overlay (rivers/roads/borders/
// coastlines as lines, place labels as color-coded markers).
// --png-mode biome|hillshade (M232) selects --png's render: "biome"
// (default) is M205's biome-color + overlay image; "hillshade" is a
// grayscale directional shaded-relief render driven purely by elevation.
// --png-region out.png [--region-tiles N] (M229) renders an NxN block of
// sibling tiles (default 2x2), starting at --level/--tile as the top-left
// corner, stitched into one larger image -- useful at levels > 0 where a
// single tile is just part of a continent/region, not the whole planet.
// --geojson out.geojson (M230) additionally exports that tile's vector
// features/labels as a GeoJSON-style FeatureCollection (map.md §10).
// --mbtiles out.mbtiles (M231) packages every already-generated tile at
// --level (not just the one --tile) into a single MBTiles-style SQLite file
// -- it does not generate anything new, only what's already persisted.
// --validate (M210) checks every already-generated tile in the whole world
// directory (every persisted level, not just --level) via MapValidator and
// prints any structural errors found; exits non-zero if any tile fails.
// --db-sizes (M223) reports the on-disk size of every map_level{N}.db and
// models/<rx>_<rz>.db (MAP11 region shard) file already in the world
// directory, plus a total -- read-only, does not generate anything new.
//
// The actual logic lives in include/PlanetMapLogic.hpp + src/PlanetMapLogic.cpp
// (part of MeshWorldLib) so tests/PlanetMapLogicTests.cpp (M211) can call it
// directly instead of spawning this binary as a subprocess.

#include <iostream>

#include "BuiltinMaterials.hpp"
#include "ContentPackLoader.hpp"
#include "Map/MapPipeline.hpp"
#include "PlanetMapLogic.hpp"
#include "PlanetWorld.hpp"

using namespace MeshWorld;

int main(int argc, char* argv[]) {
    const auto [args, ok] = parse_planet_map_args(argc, argv);
    if (!ok) {
        std::cerr << "Usage: MeshWorldPlanet <world-dir> [--level Z --tile X,Y] [--stats] [--ascii] "
                      "[--names] [--png out.png [--png-mode biome|hillshade]] "
                      "[--png-region out.png [--region-tiles N]] "
                      "[--geojson out.geojson] [--mbtiles out.mbtiles] [--legend out.png] "
                      "[--validate] [--db-sizes]\n";
        return 1;
    }

    register_builtin_materials();

    // §5 #17 fix: Map::MapPipeline consults the same LuaGeneratorRegistry
    // the legacy ChunkPipeline does -- without this, every Lua map generator
    // (planet.lua/continent.lua/.../city.lua) silently falls back to C++.
    ContentPackLoader{}.load_auto(".", "meshworld_content.sqlite");

    PlanetWorld world = is_existing_planet_world(args.dir)
                            ? PlanetWorld::open_existing(args.dir)
                            : PlanetWorld::create_new(args.dir);
    Map::MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const bool freshly_generated = !world.tile_store(args.tile.level).has(args.tile);
    const Map::MapTilePayload payload = pipeline.get(args.tile);

    print_tile_summary(args.dir, world, args.tile, payload, freshly_generated);
    if (args.stats) print_feature_stats(payload);
    if (args.ascii) {
        std::cout << "\n--- ASCII biome map (M206) ---\n";
        std::cout << render_ascii_biome_map(payload.biome);
    }
    if (args.names) print_names_list(payload);
    if (!args.png_path.empty()) {
        // M343 (MAP22) -- fetch the 4 real neighboring tiles' elevation
        // grids so render_png_elevation_image()'s hillshade can use a true
        // central-difference slope estimate at this tile's own edges
        // instead of clamping (which produced a visible brightness seam at
        // every tile boundary in a stitched multi-tile render -- the
        // elevation DATA itself was already seamless, only the shading
        // estimate wasn't). Only fetched in hillshade mode -- the biome
        // render doesn't need them, so this doesn't cost anything extra on
        // the common path. Off-planet neighbors (tile at the edge of the
        // whole map) simply aren't fetched; render_png_elevation_image()
        // falls back to its pre-M343 clamping there, same as before.
        Map::FieldGrid north_e, east_e, south_e, west_e;
        if (args.png_hillshade) {
            const int64_t span = int64_t{1} << args.tile.level;
            const auto fetch_neighbor = [&](int64_t dx, int64_t dy) -> Map::FieldGrid {
                const int64_t nx = args.tile.x + dx, ny = args.tile.y + dy;
                if (nx < 0 || nx >= span || ny < 0 || ny >= span) return {};
                return pipeline.get({args.tile.level, nx, ny}).elevation;
            };
            north_e = fetch_neighbor(0, -1);
            east_e  = fetch_neighbor(1, 0);
            south_e = fetch_neighbor(0, 1);
            west_e  = fetch_neighbor(-1, 0);
        }
        const RgbImage image = args.png_hillshade
            ? render_png_elevation_image(payload, /*upscale=*/8, &north_e, &east_e, &south_e, &west_e)
            : render_png_biome_image(payload);
        const bool     saved = write_png_file(args.png_path, image);
        std::cout << "\n--- PNG render (M205" << (args.png_hillshade ? "/M232 hillshade" : "") << ") ---\n";
        std::cout << "  " << (saved ? "saved" : "FAILED to save") << ": " << args.png_path
                   << " (" << image.w << "x" << image.h << ")\n";
        if (!saved) return 1;
    }
    if (!args.png_region_path.empty()) {
        const RgbImage image = render_png_region_image(pipeline, args.tile, args.region_tiles);
        const bool     saved = write_png_file(args.png_region_path, image);
        std::cout << "\n--- PNG region render (M229) ---\n";
        std::cout << "  " << (saved ? "saved" : "FAILED to save") << ": " << args.png_region_path
                   << " (" << image.w << "x" << image.h << ", " << args.region_tiles << "x"
                   << args.region_tiles << " tiles from level " << args.tile.level << " ("
                   << args.tile.x << "," << args.tile.y << "))\n";
        if (!saved) return 1;
    }
    if (!args.geojson_path.empty()) {
        const std::string geojson = render_geojson(payload);
        const bool        saved   = write_text_file(args.geojson_path, geojson);
        std::cout << "\n--- GeoJSON export (M230) ---\n";
        std::cout << "  " << (saved ? "saved" : "FAILED to save") << ": " << args.geojson_path
                   << " (" << geojson.size() << " bytes)\n";
        if (!saved) return 1;
    }
    if (!args.mbtiles_path.empty()) {
        const int packaged = export_mbtiles(args.dir, args.tile.level, args.mbtiles_path);
        std::cout << "\n--- MBTiles export (M231) ---\n";
        std::cout << "  " << (packaged >= 0 ? "saved" : "FAILED to save") << ": " << args.mbtiles_path
                   << " (" << packaged << " tiles from level " << args.tile.level << ")\n";
        if (packaged < 0) return 1;
    }
    if (!args.legend_path.empty()) {
        const RgbImage image = render_zone_legend_image();
        const bool     saved = write_png_file(args.legend_path, image);
        std::cout << "\n--- Zone legend (M279) ---\n";
        std::cout << "  " << (saved ? "saved" : "FAILED to save") << ": " << args.legend_path
                   << " (" << image.w << "x" << image.h << ", " << ZONE_NAMES.size() << " swatches)\n";
        print_zone_legend_list();
        if (!saved) return 1;
    }
    if (args.validate) {
        const WorldValidationResult result = validate_world(args.dir);
        print_world_validation_result(result);
        if (!result.ok()) return 1;
    }
    if (args.db_sizes) {
        print_db_size_report(audit_db_sizes(args.dir));
    }

    const HandoffResult handoff = run_handoff(args.tile, pipeline);
    print_handoff_result(args.dir, handoff);

    return 0;
}
