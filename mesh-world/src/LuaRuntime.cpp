// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "LuaRuntime.hpp"
#include "Mc3SceneBuilder.hpp"
#include "MapBuilder.hpp"
#include "Naming.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "Map/Noise.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "ContainmentRuleRegistry.hpp"
#include "StyleRegistry.hpp"
#include "Style.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace MeshWorld {

// ---------------------------------------------------------------------------
// Lua table → JSON string (for scene:setMetadata)
// ---------------------------------------------------------------------------

namespace {

std::string lua_value_to_json(const sol::object& val, int depth);

std::string lua_table_to_json(const sol::table& t, int depth) {
    if (depth > 12) return "{}";

    // Determine if it's a sequence (array) or map (object).
    std::size_t seq_len = t.size();
    bool is_array = seq_len > 0;
    if (is_array) {
        for (auto& kv : t) {
            if (!kv.first.template is<lua_Integer>()) { is_array = false; break; }
        }
    }

    std::string out;
    if (is_array) {
        out += '[';
        for (std::size_t i = 1; i <= seq_len; ++i) {
            if (i > 1) out += ',';
            out += lua_value_to_json(t[i], depth + 1);
        }
        out += ']';
    } else {
        out += '{';
        bool first = true;
        for (auto& kv : t) {
            if (!kv.first.template is<std::string>()) continue;
            if (!first) out += ',';
            first = false;
            out += '"';
            out += kv.first.template as<std::string>();
            out += "\":";
            out += lua_value_to_json(kv.second, depth + 1);
        }
        out += '}';
    }
    return out;
}

std::string lua_escape_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += static_cast<char>(c);
    }
    return out;
}

std::string lua_value_to_json(const sol::object& val, int depth) {
    if (val.is<bool>()) {
        return val.as<bool>() ? "true" : "false";
    }
    if (val.is<lua_Integer>()) {
        return std::to_string(val.as<lua_Integer>());
    }
    if (val.is<double>()) {
        std::ostringstream ss;
        ss << val.as<double>();
        return ss.str();
    }
    if (val.is<std::string>()) {
        return '"' + lua_escape_string(val.as<std::string>()) + '"';
    }
    if (val.is<sol::table>()) {
        return lua_table_to_json(val.as<sol::table>(), depth);
    }
    return "null";
}

// ---------------------------------------------------------------------------
// Helper: read float from sol::object (handles both integer and float)
// ---------------------------------------------------------------------------
float obj_float(const sol::object& o, float def = 0.0f) {
    if (o.is<lua_Integer>()) return static_cast<float>(o.as<lua_Integer>());
    if (o.is<double>())      return static_cast<float>(o.as<double>());
    return def;
}

float tbl_float(const sol::table& t, int idx, float def = 0.0f) {
    sol::object o = t[idx];
    return obj_float(o, def);
}

float tbl_float(const sol::table& t, const char* key, float def = 0.0f) {
    sol::object o = t[key];
    return obj_float(o, def);
}

// Same as obj_float()/tbl_float() above, but returning double — needed for
// world-coordinate map points (table_to_points() below): planet-scale
// coordinates reach ~22.5 million meters, where float32's ~7 significant
// digits gives only ~2-4 m of resolution, silently swallowing any
// sub-meter epsilon a caller (e.g. MapValidator's half-open tile-bounds
// convention) relies on. MapFeature::points is already
// std::vector<std::array<double,2>> (MapTilePayload.hpp) — this closes the
// gap between that and this conversion layer, it doesn't widen anything
// downstream.
double obj_double(const sol::object& o, double def = 0.0) {
    if (o.is<lua_Integer>()) return static_cast<double>(o.as<lua_Integer>());
    if (o.is<double>())      return o.as<double>();
    return def;
}

double tbl_double(const sol::table& t, int idx, double def = 0.0) {
    sol::object o = t[idx];
    return obj_double(o, def);
}

std::string tbl_str(const sol::table& t, const char* key, const std::string& def = "") {
    sol::object o = t[key];
    if (o.is<std::string>()) return o.as<std::string>();
    return def;
}

// Lua sequence table {a, b, c, ...} -> std::vector<float>.
std::vector<float> table_to_float_vec(const sol::table& t) {
    std::vector<float> out;
    out.reserve(t.size());
    for (std::size_t i = 1; i <= t.size(); ++i)
        out.push_back(tbl_float(t, static_cast<int>(i)));
    return out;
}

// Lua sequence table {{x,z}, {x,z}, ...} -> std::vector<std::array<double,2>>.
std::vector<std::array<double, 2>> table_to_points(const sol::table& t) {
    std::vector<std::array<double, 2>> out;
    out.reserve(t.size());
    for (std::size_t i = 1; i <= t.size(); ++i) {
        sol::table pt = t[i];
        out.push_back({tbl_double(pt, 1), tbl_double(pt, 2)});
    }
    return out;
}

// MAP19, M313/M314 — same flat row-major shape setBiomeField()'s own w/h +
// table triple already uses; wraps table_to_float_vec() into a Map::FieldGrid
// for the new Hydrology/MountainRanges bindings below.
Map::FieldGrid table_to_field_grid(int w, int h, const sol::table& t) {
    Map::FieldGrid g;
    g.w    = w;
    g.h    = h;
    g.data = table_to_float_vec(t);
    return g;
}

// Inverse of table_to_field_grid() for the in-place-mutation bindings
// (carveRivers/applyMountainRanges): `t` is the SAME sol::table the script's
// local elevation variable refers to (Lua tables are reference types), so
// writing into it here is visible to the script immediately afterward --
// the same "mutate the caller's own table" contract markUrbanCells()'s
// `mask` parameter does NOT have (that one is read-only), but carve()/
// apply() are defined as in-place elevation mutators on the C++ side
// (Hydrology.hpp/MountainRanges.hpp), so this binding must actually mutate,
// not just return a new table the script would have to remember to use.
void write_field_grid_into_table(const Map::FieldGrid& g, sol::table t) {
    for (std::size_t i = 0; i < g.data.size(); ++i) t[i + 1] = g.data[i];
}

} // namespace

// ---------------------------------------------------------------------------
// LuaRuntime::Impl
// ---------------------------------------------------------------------------

struct LuaRuntime::Impl {
    sol::state    lua;
    sol::table    ctx_table;
    // Name of the global the generator's second M.generate() argument is
    // bound to: "scene" for chunk generators, "map" for map generators (M090).
    const char*   binding_name;
    // ctx.random()/ctx.randomInt() stream state (map mode only, M093): a
    // counter mixed with the tile's fixed entropy via Map::noise::hash2i, so
    // the sequence is deterministic given the same script + same tile entropy
    // (map.md §8) without touching Lua's own unseeded math.random.
    std::uint64_t rng_seed{0};
    std::uint64_t rng_call_index{0};
    // scene:callGenerator's own auto id-prefix counter (chunk/object mode
    // only) -- guarantees no id collision even when a script calls the same
    // sub-generator id more than once without supplying its own placement.id.
    std::uint64_t call_counter{0};

    Impl(Mc3SceneBuilder& scene, const ChunkContext& ctx) : binding_name("scene") {
        open_safe_libraries();
        register_scene_api(scene);
        build_ctx_table(ctx);
        remove_unsafe_globals();
        setup_require();
    }

    // Map-generator mode (MAP6, M090/M091/M092/M093).
    Impl(MapBuilder& map, const Map::MapTilePayload* parent) : binding_name("map") {
        open_safe_libraries();
        register_map_api(map);
        register_names_api();
        build_map_ctx_table(map, parent);
        remove_unsafe_globals();
        setup_require();
    }

    void open_safe_libraries() {
        // Open only safe standard libs.
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table
        );
    }

    // ------------------------------------------------------------------
    // Load `source` as a fresh Lua chunk in THIS SAME sol::state (so it
    // inherits the exact same sandbox restrictions the top-level module
    // already has -- io/os/debug/package/require are removed once per Impl,
    // not per chunk) and call its M.generate(ctx_arg, binding). Shared by
    // LuaRuntime::run() (the top-level entry point) and scene:callGenerator
    // (composition, register_scene_api below) so both agree on exactly what
    // "run a generator module" means; a chunk generator calling another
    // chunk generator is not conceptually different from the outermost
    // caller running the first one.
    // ------------------------------------------------------------------
    std::string execute_module(const std::string& source, sol::object ctx_arg) {
        auto load_result = lua.safe_script(source, sol::script_pass_on_error);
        if (!load_result.valid()) {
            sol::error err = load_result;
            return std::string("Lua load error: ") + err.what();
        }

        sol::optional<sol::table> module = load_result;
        if (!module) return "Lua module did not return a table";

        sol::optional<sol::protected_function> gen_fn = (*module)["generate"];
        if (!gen_fn) return "Lua module missing 'generate' function";

        auto call_result = (*gen_fn)(ctx_arg, lua[binding_name]);
        if (!call_result.valid()) {
            sol::error err = call_result;
            return std::string("Lua generate error: ") + err.what();
        }
        return "";
    }

    // ------------------------------------------------------------------
    // scene:addBox / addCylinder / addIcoSphere (M330) / addPlane /
    // addGround / addInstance / addMaterial / setMetadata
    // ------------------------------------------------------------------
    void register_scene_api(Mc3SceneBuilder& scene) {
        // Register Mc3SceneBuilder as a Lua usertype.
        // All methods use table-based calling convention from the Lua side.
        lua.new_usertype<Mc3SceneBuilder>("Mc3SceneBuilder",
            sol::no_constructor,

            // scene:addGround("material") or scene:addGround("material", y)
            "addGround", [](Mc3SceneBuilder& self,
                             const std::string& material,
                             sol::optional<float> y) {
                self.addGround(material, y.value_or(0.0f));
            },

            // scene:addPlane("id", { position={x,y,z}, size={sx,sz},
            //                        material="m", ry=0, rx=0, rz=0 })
            "addPlane", [](Mc3SceneBuilder& self,
                            const std::string& id,
                            sol::table opts) {
                sol::table pos = opts["position"];
                sol::table sz  = opts["size"];
                float x  = tbl_float(pos, 1);
                float y  = tbl_float(pos, 2);
                float z  = tbl_float(pos, 3);
                float sx = tbl_float(sz,  1);
                float sz2= tbl_float(sz,  2);
                std::string mat = tbl_str(opts, "material");
                float ry = tbl_float(opts, "ry");
                float rx = tbl_float(opts, "rx");
                float rz = tbl_float(opts, "rz");
                self.addPlane(id, x, z, sx, sz2, mat, y, ry, rx, rz);
            },

            // scene:addBox("id", { position={x,y,z}, size={sx,sy,sz},
            //                       material="m", ry=0, rx=0, rz=0 })
            // G12 -- rx/rz (tilt/roll, degrees) let a box become a genuinely
            // sloped face (e.g. a real gable roof panel) instead of only
            // ever spinning flat in the horizontal plane.
            "addBox", [](Mc3SceneBuilder& self,
                          const std::string& id,
                          sol::table opts) {
                sol::table pos = opts["position"];
                sol::table sz  = opts["size"];
                float x  = tbl_float(pos, 1);
                float y  = tbl_float(pos, 2);
                float z  = tbl_float(pos, 3);
                float sx = tbl_float(sz,  1);
                float sy = tbl_float(sz,  2);
                float sz2= tbl_float(sz,  3);
                std::string mat = tbl_str(opts, "material");
                float ry = tbl_float(opts, "ry");
                float rx = tbl_float(opts, "rx");
                float rz = tbl_float(opts, "rz");
                self.addBox(id, x, z, sx, sy, sz2, mat, y, ry, rx, rz);
            },

            // scene:addCylinder("id", { position={x,y,z}, radius=r,
            //                           height=h, material="m",
            //                           ry=0, rx=0, rz=0 })
            // G12 -- rx/rz let a cylinder tip onto its side (e.g. a real
            // wheel rolling around a horizontal axle) instead of only ever
            // standing upright.
            "addCylinder", [](Mc3SceneBuilder& self,
                               const std::string& id,
                               sol::table opts) {
                sol::table pos = opts["position"];
                float x = tbl_float(pos, 1);
                float y = tbl_float(pos, 2);
                float z = tbl_float(pos, 3);
                float r = tbl_float(opts, "radius");
                float h = tbl_float(opts, "height");
                std::string mat = tbl_str(opts, "material");
                float ry = tbl_float(opts, "ry");
                float rx = tbl_float(opts, "rx");
                float rz = tbl_float(opts, "rz");
                self.addCylinder(id, x, z, r, h, mat, y, ry, rx, rz);
            },

            // scene:addSphere("id", { position={x,y,z}, radius=r, material="m" })
            // Already existed in Mc3SceneBuilder/Mc3DocumentBuilder (a UV
            // sphere, distinct from addIcoSphere's geodesic one) but was
            // never bound to Lua -- docs/lua-generators.md listed it as
            // available; this closes that gap.
            "addSphere", [](Mc3SceneBuilder& self,
                             const std::string& id,
                             sol::table opts) {
                sol::table pos = opts["position"];
                float x = tbl_float(pos, 1);
                float y = tbl_float(pos, 2);
                float z = tbl_float(pos, 3);
                float r = tbl_float(opts, "radius");
                std::string mat = tbl_str(opts, "material");
                self.addSphere(id, x, z, r, mat, y);
            },

            // scene:addCone("id", { position={x,y,z}, radius=r, height=h,
            //                       material="m" })
            // Same gap as addSphere above -- existed in C++, never bound.
            "addCone", [](Mc3SceneBuilder& self,
                           const std::string& id,
                           sol::table opts) {
                sol::table pos = opts["position"];
                float x = tbl_float(pos, 1);
                float y = tbl_float(pos, 2);
                float z = tbl_float(pos, 3);
                float r = tbl_float(opts, "radius");
                float h = tbl_float(opts, "height");
                std::string mat = tbl_str(opts, "material");
                self.addCone(id, x, z, r, h, mat, y);
            },

            // scene:addIcoSphere("id", { position={x,y,z}, radius=r,
            //                            material="m", scale={sx,sy,sz} })
            // M330 -- a geodesic sphere, distinct from a UV sphere; gives
            // Lua object generators (e.g. tree.lua) the same canopy-shape
            // vocabulary ObjectDefinitionLibrary.cpp's C++ tree definitions
            // already use. scale is optional (default {1,1,1}) -- an
            // ellipsoid deform, e.g. birch_tree()'s tall-narrow canopy.
            "addIcoSphere", [](Mc3SceneBuilder& self,
                                const std::string& id,
                                sol::table opts) {
                sol::table pos = opts["position"];
                float x = tbl_float(pos, 1);
                float y = tbl_float(pos, 2);
                float z = tbl_float(pos, 3);
                float r = tbl_float(opts, "radius");
                std::string mat = tbl_str(opts, "material");
                float sx = 1.0f, sy = 1.0f, sz = 1.0f;
                sol::object scale_obj = opts["scale"];
                if (scale_obj.is<sol::table>()) {
                    sol::table sc = scale_obj;
                    sx = tbl_float(sc, 1, 1.0f);
                    sy = tbl_float(sc, 2, 1.0f);
                    sz = tbl_float(sc, 3, 1.0f);
                }
                self.addIcoSphere(id, x, z, r, mat, y, sx, sy, sz);
            },

            // scene:addInstance("id", { definition="...", position={x,y,z},
            //                           ry=0, rx=0, rz=0, scale=1 })
            "addInstance", [](Mc3SceneBuilder& self,
                               const std::string& id,
                               sol::table opts) {
                sol::table pos = opts["position"];
                float x     = tbl_float(pos, 1);
                float y     = tbl_float(pos, 2);
                float z     = tbl_float(pos, 3);
                std::string def = tbl_str(opts, "definition");
                float ry    = tbl_float(opts, "ry");
                float rx    = tbl_float(opts, "rx");
                float rz    = tbl_float(opts, "rz");
                float scale = tbl_float(opts, "scale", 1.0f);
                self.addInstance(id, def, x, z, ry, y, scale, rx, rz);
            },

            // scene:addMaterial("id", { r=, g=, b=, a=, roughness=, metallic= })
            "addMaterial", [](Mc3SceneBuilder& self,
                               const std::string& id,
                               sol::table opts) {
                float r = tbl_float(opts, "r", 0.8f);
                float g = tbl_float(opts, "g", 0.8f);
                float b = tbl_float(opts, "b", 0.8f);
                float a = tbl_float(opts, "a", 1.0f);
                float roughness = tbl_float(opts, "roughness", 0.8f);
                float metallic  = tbl_float(opts, "metallic",  0.0f);
                self.addMaterial(id, r, g, b, a, roughness, metallic);
            },

            // scene:setMetadata({ generator={...}, ... })
            // Converts the Lua table to JSON and injects it as <metadata>.
            "setMetadata", [](Mc3SceneBuilder& self, sol::table opts) {
                self.setMetadataJson(lua_table_to_json(opts, 0));
            },

            // scene:callGenerator(generator_id, sub_ctx, placement)
            // placement (optional) = { position={x,y,z}, rotation_y=deg,
            //                          scale=1, id="my_prefix" }
            //
            // Looks up generator_id in the GLOBAL LuaGeneratorRegistry
            // singleton (the same one ChunkPipeline.cpp/ContentPackLoader.cpp
            // use for top-level dispatch) so composition works against
            // whatever's actually installed, runs it as a nested module in
            // THIS SAME sol::state via execute_module() above (so it
            // inherits the exact same sandbox automatically), and pushes/
            // pops a transform frame on `scene` (Mc3SceneBuilder::
            // pushTransform/popTransform) so the sub-generator's own local-
            // origin output lands correctly placed with a collision-free id
            // prefix. Tests that use this must populate the singleton
            // themselves first (LuaGeneratorRegistry::instance().
            // load_from_dir("generators/lua")) -- a fixture built around its
            // own LOCAL registry (like LuaGenFixture in
            // tests/LuaGeneratorTests.cpp) won't see anything here.
            //
            // sub_ctx is passed through to the sub-generator almost as-is
            // (it's entirely author-controlled, same as every other
            // ctx field) -- the one default applied here is `parameters`,
            // for the same "always present, even if empty" convention
            // build_ctx_table() already establishes for the top-level ctx.
            //
            // id prefix: placement.id if given, else the generator id with
            // its "lua.<category>." prefix stripped and remaining dots
            // replaced by underscores (e.g. "lua.object.bench.simple" ->
            // "bench_simple", "lua.architecture.window.double_pane" ->
            // "window_double_pane") -- taking only the LAST dotted segment
            // would give a non-descriptive "simple"/"double_pane" for every
            // generator sharing that variant suffix. Always suffixed with an
            // internal auto-incrementing counter -- guarantees no collision
            // even across repeated calls to the same sub-generator id with
            // no caller-supplied prefix.
            //
            // Errors (unknown id, nested Lua error) are raised as a C++
            // exception, which sol2 turns into a Lua error the same way
            // setup_require()'s blocked-require lambda already does --
            // a bad sub-generator call fails the WHOLE top-level generation,
            // same as any other uncaught error inside M.generate().
            "callGenerator", [this](Mc3SceneBuilder& self,
                                     const std::string& generator_id,
                                     sol::table sub_ctx,
                                     sol::optional<sol::table> placement_opt) {
                if (!LuaGeneratorRegistry::instance().has(generator_id)) {
                    throw std::runtime_error(
                        "callGenerator: unknown generator id: " + generator_id);
                }
                const std::string source = LuaGeneratorRegistry::instance().get(generator_id);

                float px = 0.0f, py = 0.0f, pz = 0.0f, pry = 0.0f, pscale = 1.0f;
                std::string prefix;
                if (placement_opt) {
                    sol::table placement = *placement_opt;
                    sol::object pos_obj = placement["position"];
                    if (pos_obj.is<sol::table>()) {
                        sol::table pos = pos_obj;
                        px = tbl_float(pos, 1);
                        py = tbl_float(pos, 2);
                        pz = tbl_float(pos, 3);
                    }
                    pry    = tbl_float(placement, "rotation_y", 0.0f);
                    pscale = tbl_float(placement, "scale", 1.0f);
                    prefix = tbl_str(placement, "id", "");
                }
                if (prefix.empty()) {
                    // Strip "lua.<category>." (the first two dot-separated
                    // segments) if present; fall back to the full id with
                    // dots replaced by underscores otherwise.
                    const std::size_t dot1 = generator_id.find('.');
                    const std::size_t dot2 = (dot1 == std::string::npos)
                        ? std::string::npos : generator_id.find('.', dot1 + 1);
                    prefix = (dot2 == std::string::npos)
                        ? generator_id : generator_id.substr(dot2 + 1);
                    for (char& c : prefix) if (c == '.') c = '_';
                }
                prefix += "_" + std::to_string(call_counter++) + "_";

                sol::optional<sol::table> params = sub_ctx["parameters"];
                if (!params) sub_ctx["parameters"] = lua.create_table();

                self.pushTransform(px, py, pz, pry, pscale, prefix);
                const std::string err = execute_module(source, sub_ctx);
                self.popTransform();

                if (!err.empty()) {
                    throw std::runtime_error(
                        "callGenerator(" + generator_id + "): " + err);
                }
            }
        );

        // Expose the scene instance as a Lua global.
        lua["scene"] = &scene;
    }

    // ------------------------------------------------------------------
    // map:setBiomeField / addRiver / addMountainRange / addCity /
    // addBorder / addRoad / addLake / addStreet / addPark / markUrbanCells
    // / setZoneCandidates / setEdge / setMetadata  (MAP6, M090; addRoad
    // M146; addLake M147; addStreet/addPark/markUrbanCells M153;
    // setZoneCandidates M156) / traceRivers / carveRivers /
    // appendHydrologyFeatures / generateMountainRanges / applyMountainRanges
    // / appendMountainRangeFeatures / applyCoastalBeach /
    // applySwampFlatnessCheck  (MAP19, M313/M314/M315: real C++ bindings
    // over Map::Hydrology/MountainRanges/BiomeRefinement, see NEXT.md §9 for
    // why this overrides the earlier "no speculative binding layer" rule)
    // / applyCanyonCarving / applyCoastalReliefRefinement / applyRiparianForest
    // (M259/M274/M275/M247, 2026-07-11: new functions on the already-exempted
    // BiomeRefinement class, unlocking canyon/tidal_flat/sea_cliff/riparian_forest)
    // / generateVolcanicHotspots / applyVolcanism -- Map::Volcanism is a BRAND
    // NEW C++ class (not a new method on an already-exempted one, unlike the
    // BiomeRefinement additions above), so this genuinely extends the MAP19
    // binding exception to a 5th algorithm family. Asked explicitly via
    // AskUserQuestion (2026-07-11, same pattern M311/M341 used for their own
    // exception conflicts) rather than assumed: answer was yes, extend it --
    // without a Lua binding, planet/continent/country's own scripts (where
    // MountainRanges itself runs) could never produce volcanic terrain, only
    // the rarely-exercised C++ fallback path could. / applyVolcanicBiomes (a
    // BiomeRefinement method, already covered) (M265-268, 2026-07-11: unlocking
    // volcanic/geothermal/ash_plain/volcanic_island)
    // ------------------------------------------------------------------
    void register_map_api(MapBuilder& map) {
        // MAP19, M313/M314 — opaque handles: HydrologyNetwork/
        // MountainRangeNetwork travel from traceRivers()/generateMountainRanges()
        // to carveRivers()/appendHydrologyFeatures()/applyMountainRanges()/
        // appendMountainRangeFeatures() as an sol2 userdata a script passes
        // straight through, same way it already passes a MapBuilder& around
        // (below) -- no fields or methods are exposed, a script has no
        // reason to inspect a network's contents itself (map.md documents
        // the actual rivers/lakes/ranges only after they're re-exported as
        // named MapFeature entries via the append* calls).
        lua.new_usertype<Map::HydrologyNetwork>("HydrologyNetwork", sol::no_constructor);
        lua.new_usertype<Map::MountainRangeNetwork>("MountainRangeNetwork", sol::no_constructor);

        lua.new_usertype<MapBuilder>("MapBuilder",
            sol::no_constructor,

            // map:setBiomeField(w, h, {elev...}, {temp...}, {moist...})
            "setBiomeField", [](MapBuilder& self, int w, int h,
                                  sol::table elevation, sol::table temperature,
                                  sol::table moisture) {
                self.setBiomeField(w, h,
                                   table_to_float_vec(elevation),
                                   table_to_float_vec(temperature),
                                   table_to_float_vec(moisture));
            },

            // map:addContinent("name", x, z)
            "addContinent", [](MapBuilder& self, const std::string& name, double x, double z) {
                self.addContinent(name, x, z);
            },

            // map:addRiver("name", { {x,z}, {x,z}, ... })
            "addRiver", [](MapBuilder& self, const std::string& name, sol::table path) {
                self.addRiver(name, table_to_points(path));
            },

            // map:addMountainRange("name", { {x,z}, {x,z}, ... })
            "addMountainRange", [](MapBuilder& self, const std::string& name, sol::table ridge) {
                self.addMountainRange(name, table_to_points(ridge));
            },

            // map:addCity("name", x, z) or map:addCity("name", x, z, "town")
            "addCity", [](MapBuilder& self, const std::string& name, double x, double z,
                           sol::optional<std::string> size_hint) {
                self.addCity(name, x, z, size_hint.value_or(""));
            },

            // map:addBorder("country", { {x,z}, {x,z}, ... })
            "addBorder", [](MapBuilder& self, const std::string& country, sol::table polygon) {
                self.addBorder(country, table_to_points(polygon));
            },

            // map:addRoad("name", { {x,z}, {x,z}, ... })
            "addRoad", [](MapBuilder& self, const std::string& name, sol::table path) {
                self.addRoad(name, table_to_points(path));
            },

            // map:addLake("name", { {x,z}, {x,z}, ... })
            "addLake", [](MapBuilder& self, const std::string& name, sol::table shoreline) {
                self.addLake(name, table_to_points(shoreline));
            },

            // map:addStreet("name", { {x,z}, {x,z}, ... })
            "addStreet", [](MapBuilder& self, const std::string& name, sol::table path) {
                self.addStreet(name, table_to_points(path));
            },

            // map:addPark("name", x, z)
            "addPark", [](MapBuilder& self, const std::string& name, double x, double z) {
                self.addPark(name, x, z);
            },

            // map:markUrbanCells({0, 1, 1, 0, ...}) -- same w*h row-major
            // shape as setBiomeField()'s own field tables; any non-zero
            // (truthy-as-number) entry marks that cell ZoneType::city.
            "markUrbanCells", [](MapBuilder& self, sol::table mask) {
                const std::vector<float> raw = table_to_float_vec(mask);
                std::vector<std::uint8_t> m(raw.size());
                for (std::size_t i = 0; i < raw.size(); ++i) m[i] = raw[i] != 0.0f ? 1 : 0;
                self.markUrbanCells(m);
            },

            // map:setZoneCandidates({0, 2, 2, 0, ...}) -- same w*h row-major
            // shape as setBiomeField()'s own field tables; each entry is a
            // Map::ZoneCandidate ordinal (M156), unlike markUrbanCells's
            // plain 0/1 mask.
            "setZoneCandidates", [](MapBuilder& self, sol::table mask) {
                const std::vector<float> raw = table_to_float_vec(mask);
                std::vector<std::uint8_t> m(raw.size());
                for (std::size_t i = 0; i < raw.size(); ++i) m[i] = static_cast<std::uint8_t>(raw[i]);
                self.setZoneCandidates(m);
            },

            // map:setEdge("N"/"E"/"S"/"W", {elev...})
            "setEdge", [](MapBuilder& self, const std::string& edge, sol::table elevation) {
                self.setEdge(edge, table_to_float_vec(elevation));
            },

            // map:setMetadata("generator.id", "culture")
            "setMetadata", [](MapBuilder& self, const std::string& generator_id,
                               const std::string& culture) {
                self.setMetadata(generator_id, culture);
            },

            // --- MAP19, M313: Hydrology binding layer ---
            // local network = map:traceRivers(W, H, elevation)
            "traceRivers", [](MapBuilder& self, int w, int h,
                               sol::table elevation) -> Map::HydrologyNetwork {
                return self.traceRivers(table_to_field_grid(w, h, elevation));
            },

            // map:carveRivers(W, H, elevation, network) -- mutates `elevation`
            // in place (see write_field_grid_into_table()'s own comment).
            "carveRivers", [](MapBuilder& self, int w, int h, sol::table elevation,
                               const Map::HydrologyNetwork& network) {
                Map::FieldGrid g = table_to_field_grid(w, h, elevation);
                self.carveRivers(g, network);
                write_field_grid_into_table(g, elevation);
            },

            // map:appendHydrologyFeatures(network, culture, entropy) or
            // map:appendHydrologyFeatures(network, culture, entropy, min_river_points)
            "appendHydrologyFeatures", [](MapBuilder& self, const Map::HydrologyNetwork& network,
                                           const std::string& culture, std::uint64_t entropy,
                                           sol::optional<int> min_river_points) {
                self.appendHydrologyFeatures(network, culture, entropy,
                                              min_river_points.value_or(5));
            },

            // --- MAP19, M314: MountainRanges binding layer ---
            // local network = map:generateMountainRanges(entropy, count,
            //                                              min_peak_m, max_peak_m)
            "generateMountainRanges", [](MapBuilder& self, std::uint64_t entropy, int count,
                                          double min_peak_elevation_m,
                                          double max_peak_elevation_m) -> Map::MountainRangeNetwork {
                return self.generateMountainRanges(entropy, count, min_peak_elevation_m,
                                                    max_peak_elevation_m);
            },

            // map:applyMountainRanges(W, H, elevation, network, falloff_width_m)
            // -- mutates `elevation` in place, same shape as carveRivers().
            "applyMountainRanges", [](MapBuilder& self, int w, int h, sol::table elevation,
                                       const Map::MountainRangeNetwork& network,
                                       double falloff_width_m) {
                Map::FieldGrid g = table_to_field_grid(w, h, elevation);
                self.applyMountainRanges(g, network, falloff_width_m);
                write_field_grid_into_table(g, elevation);
            },

            // map:appendMountainRangeFeatures(W, H, elevation, network, culture, entropy)
            "appendMountainRangeFeatures", [](MapBuilder& self, int w, int h, sol::table elevation,
                                               const Map::MountainRangeNetwork& network,
                                               const std::string& culture, std::uint64_t entropy) {
                self.appendMountainRangeFeatures(network, table_to_field_grid(w, h, elevation),
                                                  culture, entropy);
            },

            // --- M265-268 (MAP16 deferred-biome unlock, 2026-07-11): Volcanism
            // binding layer, same shape as MountainRanges above ---
            // local field = map:generateVolcanicHotspots(entropy, count, min_peak_m, max_peak_m)
            "generateVolcanicHotspots", [](MapBuilder& self, std::uint64_t entropy, int count,
                                            double min_peak_elevation_m,
                                            double max_peak_elevation_m) -> Map::VolcanicField {
                return self.generateVolcanicHotspots(entropy, count, min_peak_elevation_m,
                                                      max_peak_elevation_m);
            },

            // map:applyVolcanism(W, H, elevation, field) -- mutates `elevation`
            // in place, same shape as applyMountainRanges().
            "applyVolcanism", [](MapBuilder& self, int w, int h, sol::table elevation,
                                  const Map::VolcanicField& field) {
                Map::FieldGrid g = table_to_field_grid(w, h, elevation);
                self.applyVolcanism(g, field);
                write_field_grid_into_table(g, elevation);
            },

            // --- MAP19, M315: BiomeRefinement binding layer ---
            // map:applyCoastalBeach() or map:applyCoastalBeach(radius_cells, max_beach_elevation_m)
            // -- refines THIS builder's own already-set biome grid; must be
            // called after setBiomeField() (see MapBuilder::applyCoastalBeach's
            // own doc comment).
            "applyCoastalBeach", [](MapBuilder& self, sol::optional<int> radius_cells,
                                     sol::optional<double> max_beach_elevation_m) {
                self.applyCoastalBeach(radius_cells.value_or(1), max_beach_elevation_m.value_or(50.0));
            },

            // map:applySwampFlatnessCheck() or map:applySwampFlatnessCheck(max_local_relief_m)
            "applySwampFlatnessCheck", [](MapBuilder& self, sol::optional<double> max_local_relief_m) {
                self.applySwampFlatnessCheck(max_local_relief_m.value_or(150.0));
            },

            // M259/M274/M275 (MAP16 deferred-biome unlock, 2026-07-11):
            // canyon/tidal_flat/sea_cliff -- same "already-exempted class,
            // new function on it" precedent as the two entries above (see
            // NEXT.md's binding-exception note).
            // map:applyCanyonCarving() or map:applyCanyonCarving(steep_relief_m)
            "applyCanyonCarving", [](MapBuilder& self, sol::optional<double> steep_relief_m) {
                self.applyCanyonCarving(steep_relief_m.value_or(80.0));
            },

            // map:applyCoastalReliefRefinement() or
            // map:applyCoastalReliefRefinement(radius_cells, max_coastal_elevation_m,
            //                                   flat_relief_m, steep_relief_m)
            "applyCoastalReliefRefinement", [](MapBuilder& self, sol::optional<int> radius_cells,
                                                sol::optional<double> max_coastal_elevation_m,
                                                sol::optional<double> flat_relief_m,
                                                sol::optional<double> steep_relief_m) {
                self.applyCoastalReliefRefinement(radius_cells.value_or(1),
                                                   max_coastal_elevation_m.value_or(50.0),
                                                   flat_relief_m.value_or(15.0),
                                                   steep_relief_m.value_or(80.0));
            },

            // M247 (2026-07-11): riparian_forest -- same exemption as the
            // two entries above; also needs a traced river `network` (see
            // traceRivers()/appendHydrologyFeatures() above for the same
            // opaque-handle pattern).
            // map:applyRiparianForest(network) or map:applyRiparianForest(network, radius_cells)
            "applyRiparianForest", [](MapBuilder& self, const Map::HydrologyNetwork& network,
                                      sol::optional<int> radius_cells) {
                self.applyRiparianForest(network, radius_cells.value_or(1));
            },

            // M265-268 (2026-07-11): volcanic/geothermal/ash_plain/volcanic_island
            // -- same exemption as the entries above; needs a generated hotspot
            // `field` (see generateVolcanicHotspots() above).
            // map:applyVolcanicBiomes(field) or
            // map:applyVolcanicBiomes(field, coastal_radius_cells, inner_fraction)
            "applyVolcanicBiomes", [](MapBuilder& self, const Map::VolcanicField& field,
                                       sol::optional<int> coastal_radius_cells,
                                       sol::optional<double> inner_fraction) {
                self.applyVolcanicBiomes(field, coastal_radius_cells.value_or(1),
                                          inner_fraction.value_or(0.4));
            }
        );

        // Expose the builder instance as a Lua global.
        lua["map"] = &map;
    }

    // ------------------------------------------------------------------
    // names.culture / continent / country / city / river / mountain / lake /
    // street  (MAP6, M091; lake added M147) — thin Lua binding over the stub
    // Naming class.
    // Map-generator mode only for now; reused by chunk generators later if
    // needed (plan.md: "its own small generator family (lua.name.*)").
    // ------------------------------------------------------------------
    void register_names_api() {
        sol::table names = lua.create_table();
        names.set_function("culture", [](std::uint64_t seed) {
            return Naming::culture(seed);
        });
        names.set_function("continent", [](const std::string& culture, std::uint64_t seed) {
            return Naming::continent(culture, seed);
        });
        names.set_function("country", [](const std::string& culture, std::uint64_t seed) {
            return Naming::country(culture, seed);
        });
        names.set_function("city", [](const std::string& culture, std::uint64_t seed) {
            return Naming::city(culture, seed);
        });
        names.set_function("river", [](const std::string& culture, std::uint64_t seed) {
            return Naming::river(culture, seed);
        });
        names.set_function("mountain", [](const std::string& culture, std::uint64_t seed) {
            return Naming::mountain(culture, seed);
        });
        names.set_function("lake", [](const std::string& culture, std::uint64_t seed) {
            return Naming::lake(culture, seed);
        });
        names.set_function("street", [](const std::string& culture, std::uint64_t seed) {
            return Naming::street(culture, seed);
        });
        lua["names"] = names;
    }

    // ------------------------------------------------------------------
    // Convert a Map::FieldGrid to a Lua table {w=, h=, data={...}}.
    // ------------------------------------------------------------------
    sol::table field_grid_to_table(const Map::FieldGrid& g) {
        sol::table t = lua.create_table();
        t["w"] = g.w;
        t["h"] = g.h;
        sol::table data = lua.create_table(static_cast<int>(g.data.size()), 0);
        for (std::size_t i = 0; i < g.data.size(); ++i) data[i + 1] = g.data[i];
        t["data"] = data;
        return t;
    }

    // ------------------------------------------------------------------
    // Convert a Map::BiomeGrid (categorical ZoneType ordinals) to a Lua
    // table {w=, h=, data={...}}. Same shape as field_grid_to_table() above,
    // separate function since the element type differs (uint8_t ordinals,
    // not float field samples) — added alongside make_parent_table()'s new
    // "biome" entry below (2026-07-10 zone-override propagation fix, see
    // plan.md MAP24 M354).
    // ------------------------------------------------------------------
    sol::table biome_grid_to_table(const Map::BiomeGrid& g) {
        sol::table t = lua.create_table();
        t["w"] = g.w;
        t["h"] = g.h;
        sol::table data = lua.create_table(static_cast<int>(g.data.size()), 0);
        for (std::size_t i = 0; i < g.data.size(); ++i) data[i + 1] = g.data[i];
        t["data"] = data;
        return t;
    }

    // ------------------------------------------------------------------
    // ctx.parent — the already-generated parent tile's payload, read-only,
    // for the child generator to interpolate/constrain against (map.md §7).
    // ------------------------------------------------------------------
    sol::table make_parent_table(const Map::MapTilePayload& parent) {
        sol::table t = lua.create_table();
        t["level"]       = parent.tile.level;
        t["tile_x"]      = parent.tile.x;
        t["tile_y"]      = parent.tile.y;
        // entropy is a full 64-bit hash (~50% chance the high bit is set);
        // reinterpret as int64_t (well-defined two's-complement bit
        // preservation in C++20+) before handing to sol2 — pushing a
        // uint64_t > INT64_MAX as-is throws ("integer value will be
        // misrepresented in lua") under SOL_ALL_SAFETIES_ON. Lua scripts get
        // the same 64 bits back (possibly negative); round-trips exactly
        // when passed back into a std::uint64_t-taking binding (e.g. names.*).
        t["variation"]   = static_cast<std::int64_t>(parent.entropy);
        t["culture"]     = parent.culture;
        t["elevation"]   = field_grid_to_table(parent.elevation);
        t["temperature"] = field_grid_to_table(parent.temperature);
        t["moisture"]    = field_grid_to_table(parent.moisture);
        // parent.biome (2026-07-10, plan.md MAP24 M354): the parent's own
        // FINAL classified biome grid, ordinals matching ZoneType. Added so
        // a child script can propagate an override classify() itself can
        // never produce (e.g. ZoneType::city, only ever written by
        // city.lua's markUrbanCells()) — previously only elevation/
        // temperature/moisture were exposed, so any such override silently
        // "evaporated" one level down. Categorical data — a reader must use
        // nearest-neighbor sampling, never bilinear() like the other three.
        t["biome"]       = biome_grid_to_table(parent.biome);
        return t;
    }

    // ------------------------------------------------------------------
    // ctx.edges — the parent's stored boundary descriptors (mirrors
    // Map::MapTilePayload::edges; elevation-only until M107 adds
    // river/road crossings). ctx.edges.N/.E/.S/.W, each {elevation={...}}.
    // ------------------------------------------------------------------
    sol::table make_edges_table(const Map::MapTilePayload& parent) {
        static constexpr const char* NAMES[4] = {"N", "E", "S", "W"};
        sol::table t = lua.create_table();
        for (int i = 0; i < 4; ++i) {
            sol::table e = lua.create_table();
            const auto& samples = parent.edges[static_cast<std::size_t>(i)].elevation;
            sol::table elev = lua.create_table(static_cast<int>(samples.size()), 0);
            for (std::size_t j = 0; j < samples.size(); ++j) elev[j + 1] = samples[j];
            e["elevation"] = elev;
            t[NAMES[i]] = e;
        }
        return t;
    }

    // ------------------------------------------------------------------
    // Build ctx table for map-generator mode (MAP6, M092/M093): tile
    // address/size, entropy, parent/edges (nil at level 0 — the planet root
    // has no parent), and seeded noise/random.
    // ------------------------------------------------------------------
    void build_map_ctx_table(const MapBuilder& map, const Map::MapTilePayload* parent) {
        const Map::MapTilePayload& self = map.payload();
        const Map::TileCoord&      tile = self.tile;

        rng_seed       = self.entropy;
        rng_call_index = 0;

        ctx_table = lua.create_table();
        ctx_table["level"]       = tile.level;
        ctx_table["tile_x"]      = tile.x;
        ctx_table["tile_y"]      = tile.y;
        ctx_table["tile_size_m"] = tile.size_m();
        // See make_parent_table()'s "variation" comment above: reinterpret as
        // int64_t so sol2 doesn't throw for entropies > INT64_MAX.
        ctx_table["variation"]   = static_cast<std::int64_t>(self.entropy);
        ctx_table["parent"]      = (parent != nullptr) ? sol::object(make_parent_table(*parent))
                                                        : sol::object(sol::nil);
        ctx_table["edges"]       = (parent != nullptr) ? sol::object(make_edges_table(*parent))
                                                        : sol::object(sol::nil);

        // ctx.noise(x, y [, octaves, lacunarity, gain]) -> fBm value in [0,1),
        // seeded from this tile's entropy (Map::noise::fbm, same utility the
        // C++ PlanetGenerator/ChildGenerator use).
        const std::uint64_t entropy = self.entropy;
        ctx_table.set_function("noise", [entropy](double x, double y,
                                                    sol::optional<int>    octaves,
                                                    sol::optional<double> lacunarity,
                                                    sol::optional<double> gain) {
            return static_cast<double>(Map::noise::fbm(
                x, y, entropy, octaves.value_or(4), lacunarity.value_or(2.0), gain.value_or(0.5)));
        });

        // ctx.random() -> float in [0,1); ctx.randomInt(lo, hi) -> integer in
        // [lo, hi] inclusive. Both draw from a counter mixed with the tile's
        // entropy, NOT Lua's own math.random (which is unseeded here and
        // would break determinism, map.md §8).
        ctx_table.set_function("random", [this]() {
            ++rng_call_index;
            const auto h = Map::noise::hash2i(
                static_cast<std::int64_t>(rng_call_index), 0, rng_seed);
            return static_cast<double>(Map::noise::to_unit_float(h));
        });
        ctx_table.set_function("randomInt", [this](std::int64_t lo, std::int64_t hi) {
            ++rng_call_index;
            const auto h = Map::noise::hash2i(
                static_cast<std::int64_t>(rng_call_index), 1, rng_seed);
            const auto span = static_cast<std::uint64_t>(hi - lo + 1);
            return lo + static_cast<std::int64_t>(h % span);
        });

        lua["ctx"] = ctx_table;
    }

    // ------------------------------------------------------------------
    // Build ctx table from ChunkContext
    // ------------------------------------------------------------------
    void build_ctx_table(const ChunkContext& ctx) {
        ctx_table = lua.create_table();
        // ctx.seed is a full 64-bit hash (~50% chance the high bit is set);
        // reinterpret as int64_t (well-defined two's-complement bit
        // preservation) before handing to sol2 — pushing a uint64_t >
        // INT64_MAX as-is throws ("integer value will be misrepresented in
        // lua") under SOL_ALL_SAFETIES_ON. Same fix as map-mode's
        // ctx.variation/ctx.parent.variation (see build_map_ctx_table).
        ctx_table["variation"]    = static_cast<std::int64_t>(ctx.seed);
        ctx_table["chunk_x"]     = ctx.coord.x;
        ctx_table["chunk_y"]     = ctx.coord.y;
        ctx_table["chunk_size_m"]= ctx.chunk_size_m;

        // ctx.style: G11 fix (2026-07-11) -- resolve the style id through
        // StyleRegistry into a real {id, name, palette} table when
        // registered, instead of leaving it a bare string. Many existing
        // generators already (uselessly, until now) write `ctx.style and
        // ctx.style.wood_material` as if this were already a table with
        // simple named fields -- worth noting explicitly that this fix does
        // NOT retroactively make those specific call sites start working:
        // the REAL Style palette (src/styles/*.cpp) uses dotted, namespaced
        // keys ("park.lamp", "block.facade.0"), not simple names like
        // "wood_material", so `ctx.style.wood_material` still returns nil
        // (now for a different reason: no such literal key in the palette,
        // rather than ctx.style not being a table at all) -- same
        // "harmless, falls through to the generator's own hardcoded
        // default" behavior as before, not a regression. New generators
        // should use `ctx.style.palette["some.dotted.key"] or "default"`.
        // Falls back to the bare string id when unregistered (unchanged
        // pre-2026-07-11 behavior for that case).
        if (StyleRegistry::instance().has(ctx.style)) {
            const Style& style = StyleRegistry::instance().get(ctx.style);
            sol::table style_tbl = lua.create_table();
            style_tbl["id"]   = style.id;
            style_tbl["name"] = style.name;
            sol::table palette_tbl = lua.create_table();
            for (const auto& kv : style.palette) palette_tbl[kv.first] = kv.second;
            style_tbl["palette"] = palette_tbl;
            ctx_table["style"] = style_tbl;
        } else {
            ctx_table["style"] = ctx.style;
        }

        ctx_table["zone"]        = to_string(ctx.zone);
        // R129 (zone-metadata bug fix, NEXT.md §4) -- the flat WorldMap/
        // WorldConfig-derived zone, unaffected by ChunkPipeline's own M157
        // map-layer override (see ChunkContext::authored_zone's own doc
        // comment). Exposed alongside ctx.zone (left untouched -- some
        // scripts may still legitimately want the overridden, map-aware
        // value for actual generation decisions) specifically so Lua
        // generators building their own ctx.setMetadata({...}) table (see
        // e.g. generators/lua/zone/crossroad.lua) can report the zone this
        // world's own flat config actually authored, instead of an
        // unrelated planet's biome at whatever coordinates a freshly
        // auto-created hand-off world happens to occupy.
        ctx_table["authored_zone"] = to_string(ctx.authored_zone);
        ctx_table["region"]      = to_string(ctx.region);
        ctx_table["parameters"]  = lua.create_table(); // empty; reserved for future use

        // ctx.lod: 0 (coarse) .. 4 (high-detail), see ChunkGenerator.hpp's
        // own ChunkContext::lod comment -- documented for a long time
        // (docs/lua-generators.md) before any C++ field backed it.
        ctx_table["lod"] = ctx.lod;

        // ctx.exits.{north,south,east,west}_{road,path}: previously
        // documented (docs/lua-generators.md) but never actually exposed to
        // object/chunk-mode Lua scripts -- ChunkContext::exits (WorldMap.hpp
        // EdgeExits) always existed, just wasn't copied into the ctx table.
        sol::table exits = lua.create_table();
        exits["north_road"] = ctx.exits.north_road;
        exits["south_road"] = ctx.exits.south_road;
        exits["east_road"]  = ctx.exits.east_road;
        exits["west_road"]  = ctx.exits.west_road;
        exits["north_path"] = ctx.exits.north_path;
        exits["south_path"] = ctx.exits.south_path;
        exits["east_path"]  = ctx.exits.east_path;
        exits["west_path"]  = ctx.exits.west_path;
        ctx_table["exits"] = exits;

        // ctx.random()/ctx.randomInt(lo, hi): previously only existed for
        // map-mode ctx (build_map_ctx_table below) -- object/chunk generators
        // had no seeded-random helper at all and had to derive their own
        // from ctx.variation by hand. Same deterministic counter+hash
        // mechanism, seeded from ctx.seed instead of a map tile's entropy.
        rng_seed       = ctx.seed;
        rng_call_index = 0;
        ctx_table.set_function("random", [this]() {
            ++rng_call_index;
            const auto h = Map::noise::hash2i(
                static_cast<std::int64_t>(rng_call_index), 0, rng_seed);
            return static_cast<double>(Map::noise::to_unit_float(h));
        });
        ctx_table.set_function("randomInt", [this](std::int64_t lo, std::int64_t hi) {
            ++rng_call_index;
            const auto h = Map::noise::hash2i(
                static_cast<std::int64_t>(rng_call_index), 1, rng_seed);
            const auto span = static_cast<std::uint64_t>(hi - lo + 1);
            return lo + static_cast<std::int64_t>(h % span);
        });

        // ctx.containment.childrenOf(parent_id): real, pre-existing gap
        // (found 2026-07-11) -- ContainmentRuleRegistry/TaxonomyRegistry and
        // real data/taxonomy/{taxonomy,containment}.json content have
        // existed since before this session (ContentPackLoader::
        // load_from_disk() already loads both at startup), and
        // docs/taxonomy-and-containment.md has documented this exact
        // `ctx.containment.childrenOf(...)` Lua call for just as long -- but
        // no Lua binding for it ever existed, so no script could ever
        // actually call it. This exposes ContainmentRuleRegistry::
        // children_of() (read-only) to Lua; scripts are still responsible
        // for their own probability roll (ctx.random()) and lod_max gate
        // (<= ctx.lod), matching the doc's own example exactly. Field names
        // here are `parent`/`child` (the real ContainmentRule.hpp struct),
        // not the doc's stale `parent_id`/`child_id` (fixed in the same
        // commit as this binding). Object/chunk-mode only (this function,
        // build_ctx_table) -- not exposed in map mode, matching the doc's
        // own zone/park.lua-style example (an object/chunk-mode script).
        {
            sol::table containment = lua.create_table();
            containment.set_function("childrenOf", [this](const std::string& parent_id) {
                sol::table out = lua.create_table();
                int i = 1;
                for (const auto& rule : ContainmentRuleRegistry::instance().children_of(parent_id)) {
                    sol::table r = lua.create_table();
                    r["parent"]      = rule.parent;
                    r["child"]       = rule.child;
                    r["probability"] = rule.probability;
                    r["min_count"]   = rule.min_count;
                    r["max_count"]   = rule.max_count;
                    r["lod_max"]     = rule.lod_max;
                    out[i++] = r;
                }
                return out;
            });
            ctx_table["containment"] = containment;
        }

        lua["ctx"] = ctx_table;
    }

    // ------------------------------------------------------------------
    // Sandbox: remove unsafe globals
    // ------------------------------------------------------------------
    void remove_unsafe_globals() {
        for (const char* g : {"io", "os", "debug", "package",
                               "dofile", "loadfile", "load", "collectgarbage"}) {
            lua[g] = sol::nil;
        }
    }

    // ------------------------------------------------------------------
    // Sandbox: restrict require() to an empty whitelist
    // (Lua generators should be self-contained; module loading is not allowed)
    // ------------------------------------------------------------------
    void setup_require() {
        lua.set_function("require", [](const std::string& name) -> sol::object {
            throw std::runtime_error("require is not allowed in MeshWorld sandbox: " + name);
            return sol::nil; // unreachable, suppresses compiler warning
        });
    }
};

// ---------------------------------------------------------------------------
// LuaRuntime public interface
// ---------------------------------------------------------------------------

LuaRuntime::LuaRuntime(Mc3SceneBuilder& scene, const ChunkContext& ctx)
    : impl_(std::make_unique<Impl>(scene, ctx))
{}

LuaRuntime::LuaRuntime(MapBuilder& map, const Map::MapTilePayload* parent)
    : impl_(std::make_unique<Impl>(map, parent))
{}

LuaRuntime::~LuaRuntime() = default;

std::string LuaRuntime::run(const std::string& source) {
    // Load the module (executes top-level code, expects M = {generate=...}
    // returned) and call M.generate(ctx, scene|map) -- the Lua side receives
    // ctx as a table and the second arg as a userdata bound under
    // impl_->binding_name. Shared with scene:callGenerator's own nested
    // invocations via Impl::execute_module() (see LuaRuntime.cpp's
    // register_scene_api) -- this is just that same logic for the top-level
    // entry point.
    return impl_->execute_module(source, impl_->ctx_table);
}

} // namespace MeshWorld
