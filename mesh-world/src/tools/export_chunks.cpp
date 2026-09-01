// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldExport — generates all chunks for a world and writes mc3.xml files.
// With --mcb: also compiles each .xml to .mcb binary format via MeshCraft Mcb.
// With --validate: runs MC3Validator (via ChunkDiagnostics, R108) on every
// chunk and prints any errors/warnings found.
// With --force: ignores the on-disk cache and regenerates every chunk, even
// if a "<x>_<y>.mc3.xml" already exists in output_dir from a prior run.
// With --threads N: generates N chunks concurrently. Each worker thread owns
// its own ChunkCache/PlanetWorld/MapPipeline/ChunkPipeline -- none of those
// are thread-safe to share, the same reason WorldStreamer's own worker pool
// (src/WorldStreamer.cpp) gives each of its threads a private set too. The
// underlying MapTileStore (SQLite, WAL mode + a construction mutex + a busy
// timeout) is already built to support exactly this: several connections
// targeting the same map_level{z}.db file concurrently.
//
// Usage:
//   MeshWorldExport [--mcb] [--validate] [--force] [--threads N] [world.json] [output_dir]
//
// Defaults:
//   world.json   examples/world.json
//   output_dir   output/chunks
//   threads      1

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <optional>
#include <filesystem>
#include "ContentPackLoader.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ChunkPipeline.hpp"
#include "ChunkDiagnostics.hpp"
#include "ChunkCoord.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "Map/MapPipeline.hpp"
#include "PlanetMapLogic.hpp"  // is_existing_planet_world()/planet_params_from_config()
#include "PlanetWorld.hpp"
#include "BuiltinMaterials.hpp"
#include "BuiltinStyles.hpp"
#include "ComposerAssets.hpp"
#include "ObjectDefinitionLibrary.hpp"

#ifdef MESH_WORLD_HAS_MCB
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#endif

namespace fs = std::filesystem;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--mcb] [--validate] [--force] [--threads N] [world.json] [output_dir]\n"
              << "  --mcb       Also compile each .xml to .mcb (requires MeshCraft Mcb)\n"
              << "  --validate  Run MC3Validator on every chunk, print errors/warnings\n"
              << "  --force     Ignore the on-disk cache, regenerate every chunk\n"
              << "  --threads N Generate N chunks concurrently (default: 1)\n";
}

int main(int argc, char* argv[]) {
    bool        want_mcb      = false;
    bool        want_validate = false;
    bool        want_force    = false;
    int         want_threads  = 1;
    std::string config_path;   // empty = not yet set
    std::string output_dir;    // empty = not yet set

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mcb") {
            want_mcb = true;
        } else if (arg == "--validate") {
            want_validate = true;
        } else if (arg == "--force") {
            want_force = true;
        } else if (arg == "--threads") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --threads requires a value\n";
                return 1;
            }
            want_threads = std::atoi(argv[++i]);
            if (want_threads < 1) {
                std::cerr << "Error: --threads must be a positive integer (got \""
                          << argv[i] << "\")\n";
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (config_path.empty()) {
            config_path = arg;
        } else {
            output_dir = arg;
        }
    }

    if (config_path.empty()) config_path = "examples/world.json";
    if (output_dir.empty())  output_dir  = "output/chunks";

    // §5 #17 fix: populate LuaGeneratorRegistry before generating any chunk.
    MeshWorld::ContentPackLoader{}.load_auto(".", "meshworld_content.sqlite");

    // G11 fix (2026-07-11, procedural-model-generator-roadmap): this binary
    // generates real chunks via ChunkPipeline below, which can dispatch to
    // ParkGenerator/RoadGenerator/SmallHouseBlockGenerator -- previously
    // neither MaterialRegistry nor StyleRegistry was ever populated here.
    MeshWorld::register_builtin_materials();
    MeshWorld::register_builtin_styles();

    // R113 v1 -- populate ObjectDefinitionLibrary explicitly (its usual
    // lazy-load trigger, ObjectBoundingBox.cpp's object_height_m(), may
    // not have run yet at this point) before AssetRegistry/
    // StyleProfileRegistry, which look up its definitions by id.
    MeshWorld::ObjectDefinitionLibrary::instance().load_all();
    MeshWorld::register_composer_assets();

#ifndef MESH_WORLD_HAS_MCB
    if (want_mcb) {
        std::cerr << "Error: --mcb requested but MeshCraft Mcb was not available at build time.\n";
        return 1;
    }
#endif

    std::cout << "MeshWorldExport — chunk generation tool\n";
    std::cout << "Config : " << config_path << "\n";
    std::cout << "Output : " << output_dir  << "\n";
    if (want_mcb)   std::cout << "MCB    : enabled\n";
    if (want_force) std::cout << "Force  : enabled (ignoring cache)\n";
    if (want_threads > 1) std::cout << "Threads: " << want_threads << "\n";
    std::cout << "\n";

    MeshWorld::WorldConfig cfg;
    if (!cfg.load_from_file(config_path)) {
        std::cerr << "Error: cannot load " << config_path << "\n";
        return 1;
    }

    std::cout << "World  : " << cfg.name   << "\n";
    std::cout << "Seed   : " << cfg.seed   << "\n";
    std::cout << "Grid   : " << cfg.grid_w << " x " << cfg.grid_h
              << " = " << (cfg.grid_w * cfg.grid_h) << " chunks\n";
    std::cout << "Mode   : " << (cfg.procedural ? "procedural" : "config-driven") << "\n\n";

    MeshWorld::WorldMap      map(cfg);
    if (want_validate) {
        const auto road_issues = map.validate_road_network();
        if (!road_issues.empty()) {
            std::cerr << "Road-network validation failed (" << road_issues.size() << " issue(s)):\n";
            for (const auto& issue : road_issues)
                std::cerr << "  " << issue.x << "," << issue.y << ": " << issue.message << "\n";
            return 1;
        }
    }

    // Attach the planetary map layer (Map::MapPipeline) so ChunkContext.map_context
    // is populated for exported chunks too, the same hand-off MeshWorldPlanet's CLI
    // and MeshWorldApp already exercise (§5 #2's last remaining gap). This top-level
    // check+create (does the world dir already exist at all) runs once, single-
    // threaded, before any worker below opens its own connection to it.
    const std::string map_dir = output_dir + "/map";
    if (!MeshWorld::is_existing_planet_world(map_dir))
        MeshWorld::PlanetWorld::create_new(map_dir, cfg);

    const int total = cfg.grid_w * cfg.grid_h;
    int mcb_written = 0;
    int mcb_errors  = 0;

    std::atomic<int> done{0};
    std::atomic<int> validate_chunks_with_errors{0};
    std::atomic<int> validate_chunks_with_warnings{0};

    std::mutex print_mutex;   // guards stdout writes shared across worker threads
    std::mutex zone_mutex;    // guards zone_counts
    std::map<std::string, int> zone_counts;

    // Flatten the grid into a work list, then hand out indices dynamically
    // (an atomic cursor) rather than a static per-thread split -- chunk cost
    // varies a lot by zone (a city block vs. an empty tile), so a thread
    // that finishes its share early can pick up more instead of idling.
    std::vector<MeshWorld::ChunkCoord> work;
    work.reserve(total);
    for (int y = 0; y < cfg.grid_h; ++y)
        for (int x = 0; x < cfg.grid_w; ++x)
            work.push_back({x, y});
    std::atomic<std::size_t> next_index{0};

    // T236/T243 -- each worker owns its own ChunkCache/PlanetWorld/MapPipeline/
    // ChunkPipeline. None of those are safe to share across threads (see this
    // file's own top comment); the underlying MapTileStore (SQLite, WAL mode +
    // a construction mutex + a busy timeout, src/Map/MapTileStore.cpp) is
    // already built for exactly this: several connections targeting the same
    // map_level{z}.db file concurrently, the same pattern WorldStreamer's own
    // worker pool already relies on. cfg/map are read-only after construction
    // (WorldMap builds its whole grid_ once, in its own constructor) and the
    // process-wide MaterialRegistry/LuaGeneratorRegistry/StyleRegistry
    // singletons were already fully populated above, before any thread here
    // starts -- concurrent reads of those are safe (LuaGeneratorRegistry's own
    // header documents this contract explicitly).
    auto worker = [&]() {
        MeshWorld::ChunkCache cache(output_dir);
        MeshWorld::PlanetWorld      map_world    = MeshWorld::PlanetWorld::open_existing(map_dir);
        MeshWorld::Map::MapPipeline map_pipeline{map_world, MeshWorld::planet_params_from_config(cfg)};
        MeshWorld::ChunkPipeline    pipeline(cfg, map, cache, &map_pipeline);

        // T243 -- the "N / total (%)" progress line is driven by
        // ChunkPipeline's own progress callback (one call per get(), whether
        // it was a cache hit or real generation). Each worker's own callback
        // shares the same atomic `done` counter and print_mutex.
        pipeline.set_progress_callback([&](const MeshWorld::ChunkCoord&, MeshWorld::ChunkDiagnostics::Source) {
            const int d = ++done;
            if (d % 50 == 0 || d == total) {
                std::lock_guard<std::mutex> lk(print_mutex);
                std::cout << "\r  " << std::setw(3) << d << " / " << total
                          << "  (" << (d * 100 / total) << "%)" << std::flush;
            }
        });

        for (;;) {
            const std::size_t idx = next_index.fetch_add(1);
            if (idx >= work.size()) break;
            const auto [x, y] = work[idx];

            if (want_validate) {
                MeshWorld::ChunkDiagnostics diag;
                pipeline.get(x, y, &diag, want_force);
                if (!diag.validation_errors.empty()) {
                    ++validate_chunks_with_errors;
                    std::lock_guard<std::mutex> lk(print_mutex);
                    std::cout << "\n  ERROR " << MeshWorld::ChunkCoord{x, y}.to_string()
                              << " (" << diag.generator_id << "):\n";
                    for (const auto& e : diag.validation_errors)
                        std::cout << "    " << e << "\n";
                }
                if (!diag.validation_warnings.empty()) {
                    ++validate_chunks_with_warnings;
                    std::lock_guard<std::mutex> lk(print_mutex);
                    std::cout << "\n  WARNING " << MeshWorld::ChunkCoord{x, y}.to_string()
                              << " (" << diag.generator_id << "):\n";
                    for (const auto& w : diag.validation_warnings)
                        std::cout << "    " << w << "\n";
                }
            } else {
                pipeline.get(x, y, want_force);
            }
            const auto ci = map.info(x, y);
            {
                std::lock_guard<std::mutex> lk(zone_mutex);
                zone_counts[MeshWorld::to_string(ci.zone)]++;
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(want_threads);
    for (int i = 0; i < want_threads; ++i)
        workers.emplace_back(worker);
    for (auto& t : workers)
        t.join();

    std::cout << "\n";

    if (want_validate) {
        std::cout << "\nValidation: " << validate_chunks_with_errors << " chunk(s) with errors, "
                  << validate_chunks_with_warnings << " chunk(s) with warnings (of " << total << " total)\n";
    }

    // ── Optional MCB compilation pass ────────────────────────────────────────
#ifdef MESH_WORLD_HAS_MCB
    if (want_mcb) {
        std::cout << "\nCompiling XML → MCB...\n";
        for (int y = 0; y < cfg.grid_h; ++y) {
            for (int x = 0; x < cfg.grid_w; ++x) {
                MeshWorld::ChunkCoord coord{x, y};
                fs::path xml_path = fs::path(output_dir) /
                                    (coord.to_string() + ".mc3.xml");
                fs::path mcb_path = fs::path(output_dir) /
                                    (coord.to_string() + ".mcb");
                try {
                    auto doc = MeshCraft::Mc3::Mc3Document::loadFromFile(xml_path);
                    MeshCraft::Mcb::saveToFile(doc, mcb_path);
                    ++mcb_written;
                } catch (const std::exception& e) {
                    std::cerr << "\n  ERROR " << coord.to_string()
                              << ": " << e.what() << "\n";
                    ++mcb_errors;
                }
            }
        }
        std::cout << "MCB: " << mcb_written << " written";
        if (mcb_errors) std::cout << ", " << mcb_errors << " errors";
        std::cout << "\n";
    }
#endif

    std::cout << "\nZone distribution:\n";
    for (const auto& [name, count] : zone_counts) {
        std::cout << "  " << std::setw(16) << std::left << name
                  << count << " chunks\n";
    }

    std::cout << "\nExported " << done << " chunks to " << output_dir << "/\n";
    return (mcb_errors > 0 || validate_chunks_with_errors > 0) ? 1 : 0;
}
