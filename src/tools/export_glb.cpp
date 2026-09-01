// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldGLB — batch-converts a directory of already-exported mc3.xml
// chunk files (see MeshWorldExport) to .glb, using MeshCraft's own
// mc3togltf_lib (GltfExporter) directly rather than shelling out to the
// mc3togltf executable per file. Mirrors MeshWorldExport's own --mcb flag,
// but as a standalone binary since mc3togltf's dependency chain (Manifold,
// tinygltf, tinyobjloader) is heavier than Mcb's.
//
// Usage:
//   MeshWorldGLB [--stats] <input_dir> [output_dir]
//
// Every "<x>_<y>.mc3.xml" file directly inside input_dir is converted to
// "<x>_<y>.glb" in output_dir (default: same as input_dir). Non-chunk
// files (anything not matching "*.mc3.xml") are skipped.
//
// MeshWorld generators reference materials by bare string id (e.g.
// material="rock_sea"), resolved at MeshWorld's own render time via the
// runtime-only MaterialRegistry -- individual exported chunk XML files
// never embed a real <materials> definition for them (first found via a
// real conversion run: every material showed up as "unknown", 0 glTF
// materials written despite 14k+ objects processed). Since mc3togltf is a
// generic MeshCraft tool with no notion of MeshWorld's own registry, this
// binary collects every material id actually referenced somewhere in each
// loaded document and injects a matching real Mc3Material (base color +
// roughness + metallic, no texture -- a chunk's own file lives in a
// different directory than assets/textures/, so a texture URI wouldn't
// resolve there anyway, same class of path-resolution bug already fixed
// once this session for the live renderer) sourced from MeshWorld's own
// MaterialRegistry, so the resulting .glb actually carries real MeshWorld
// colors instead of glTF's flat default grey. Only materials the document
// actually uses are injected, not MeshWorld's whole ~165-entry catalogue --
// that was tried first and roughly doubled per-file .glb size for no
// benefit (measured on a real demo chunk).

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include <filesystem>

#include "GltfExporter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "BuiltinMaterials.hpp"
#include "ComposerAssets.hpp"
#include "MaterialRegistry.hpp"
#include "ObjectDefinitionLibrary.hpp"

namespace fs = std::filesystem;

namespace {

// Recursively collects every non-empty `material`/`materialOverride` string
// actually referenced by `obj` or any of its children. Injecting the FULL
// MeshWorld material catalogue (~165 entries) into every one of hundreds of
// per-chunk documents regardless of use was tried first and rejected: it
// roughly doubled per-file .glb size (measured: 20KB -> 49KB on a real demo
// chunk) for materials that chunk never even references. Collecting real
// usage first keeps injection to only what's actually needed.
void collect_material_ids(const std::shared_ptr<MeshCraft::Mc3::Mc3Object>& obj,
                           std::set<std::string>& out) {
    if (!obj) return;
    if (!obj->material.empty())         out.insert(obj->material);
    if (!obj->materialOverride.empty()) out.insert(obj->materialOverride);
    for (const auto& child : obj->children) collect_material_ids(child, out);
}

// Adds a real Mc3Material (base color + roughness + metallic, sourced from
// MeshWorld's own MaterialRegistry) to doc.materials for every material id
// actually referenced somewhere in the document that it doesn't already
// define itself -- see the top-of-file comment for why this is needed at
// all: MeshWorld's generators reference materials by bare id, resolved via
// a runtime-only registry never serialized into the exported chunk XML.
void inject_referenced_materials(MeshCraft::Mc3::Mc3Document& doc) {
    std::set<std::string> referenced;
    for (const auto& obj : doc.objects)                  collect_material_ids(obj, referenced);
    for (const auto& [id, def] : doc.definitions)         collect_material_ids(def, referenced);

    auto& registry = MeshWorld::MaterialRegistry::instance();
    for (const auto& id : referenced) {
        if (doc.materials.count(id)) continue;   // already defined in the file itself
        if (!registry.has(id))       continue;    // genuinely unknown to MeshWorld too
        const auto& m = registry.get(id);
        doc.materials[id] = MeshCraft::Mc3::Mc3Material::opaque(
            id, {m.r, m.g, m.b, 1.0f}, m.roughness, m.metallic);
    }
}

} // namespace

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--stats] <input_dir> [output_dir]\n"
              << "  --stats  Print aggregate export statistics after conversion\n"
              << "\n"
              << "Converts every *.mc3.xml file directly inside input_dir to a .glb\n"
              << "file of the same basename in output_dir (default: input_dir).\n";
}

int main(int argc, char* argv[]) {
    bool        want_stats = false;
    std::string input_dir;
    std::string output_dir;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--stats") {
            want_stats = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (input_dir.empty()) {
            input_dir = arg;
        } else {
            output_dir = arg;
        }
    }

    if (input_dir.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    if (output_dir.empty()) output_dir = input_dir;

    MeshWorld::register_builtin_materials();
    // R114 (city showcase) -- every real content instance (houses,
    // vehicles, trees, street furniture -- everything BuildingComposer or
    // any other generator places via w.instance()) is a bare <instance
    // ref="..."/> in the exported chunk XML with no accompanying
    // <definitions> entry (that resolution only ever happened at
    // WorldRenderer's own live render time). Populate the same global
    // definition sources the live renderer uses so resolve_instance_
    // definitions() below can actually inline real geometry per file,
    // instead of every instance silently exporting as nothing.
    MeshWorld::ObjectDefinitionLibrary::instance().load_all();
    MeshWorld::register_composer_assets();

    if (!fs::is_directory(input_dir)) {
        std::cerr << "Error: not a directory: " << input_dir << "\n";
        return 1;
    }
    fs::create_directories(output_dir);

    std::vector<fs::path> inputs;
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& p = entry.path();
        // "*.mc3.xml" -- filename() ends in both ".mc3" and ".xml" stacked,
        // matching ChunkCoord::to_string() + ".mc3.xml" (ChunkCache.cpp).
        if (p.extension() != ".xml") continue;
        if (p.stem().extension() != ".mc3") continue;
        inputs.push_back(p);
    }
    std::sort(inputs.begin(), inputs.end());

    if (inputs.empty()) {
        std::cerr << "Error: no *.mc3.xml files found in " << input_dir << "\n";
        return 1;
    }

    std::cout << "MeshWorldGLB — batch mc3.xml -> glb converter\n";
    std::cout << "Input  : " << input_dir  << "\n";
    std::cout << "Output : " << output_dir << "\n";
    std::cout << "Files  : " << inputs.size() << "\n\n";

    int written = 0;
    int errors  = 0;
    mc3togltf::ExportStats totals;

    for (const auto& xml_path : inputs) {
        // "foo.mc3.xml" -> "foo.glb"
        fs::path glb_name = xml_path.filename();
        glb_name.replace_extension();       // drop ".xml"  -> "foo.mc3"
        glb_name.replace_extension(".glb"); // drop ".mc3"  -> "foo.glb"
        fs::path glb_path = fs::path(output_dir) / glb_name;

        try {
            auto doc = MeshCraft::Mc3::Mc3Document::loadFromFile(xml_path);
            // Instance resolution first: it can inject whole new
            // definition subtrees (houses/vehicles/trees/furniture) that
            // reference their OWN materials -- scanning for referenced
            // materials before that would only ever see the original
            // document's own pre-existing objects/definitions, missing
            // everything inside a newly-injected subtree entirely.
            MeshWorld::resolve_instance_definitions(doc);
            inject_referenced_materials(doc);

            mc3togltf::GltfExporter exporter;
            exporter.exportDocument(doc, glb_path, mc3togltf::OutputFormat::GLB);
            ++written;

            totals.objectsProcessed   += exporter.stats.objectsProcessed;
            totals.gltfNodes          += exporter.stats.gltfNodes;
            totals.uniqueMeshes       += exporter.stats.uniqueMeshes;
            totals.materialCount      += exporter.stats.materialCount;
            totals.reusedMeshRefs     += exporter.stats.reusedMeshRefs;
            totals.totalVertices      += exporter.stats.totalVertices;
            totals.totalTriangles     += exporter.stats.totalTriangles;
            totals.objMeshesLoaded    += exporter.stats.objMeshesLoaded;
            totals.csgMeshesEvaluated += exporter.stats.csgMeshesEvaluated;
            // Confirmed expected/ignorable (NEXT.md's known-warnings note):
            // every chunk contributes exactly one "ambient light has no
            // glTF equivalent" warning here, since glTF's own lighting
            // model has no ambient-fill concept to map
            // Mc3DocumentBuilder's fixed directional sun + ambient fill
            // onto (see R114's own `MeshWorldGLB --stats` verification
            // run: 9 warnings for 9 chunks, 0 "unknown definition"/
            // "unknown material" warnings of any other kind).
            totals.warnings           += exporter.stats.warnings;
        } catch (const std::exception& e) {
            std::cerr << "  ERROR " << xml_path.filename().string() << ": " << e.what() << "\n";
            ++errors;
        }

        if ((written + errors) % 50 == 0 || (written + errors) == static_cast<int>(inputs.size())) {
            std::cout << "\r  " << std::setw(3) << (written + errors) << " / " << inputs.size()
                      << std::flush;
        }
    }
    std::cout << "\n\n";

    std::cout << "Written: " << written << ", errors: " << errors << "\n";

    if (want_stats) {
        std::cout << "\nAggregate export statistics:\n"
                  << "  Objects processed: " << totals.objectsProcessed   << "\n"
                  << "  glTF nodes:        " << totals.gltfNodes          << "\n"
                  << "  Unique meshes:     " << totals.uniqueMeshes       << "\n"
                  << "  Materials:         " << totals.materialCount      << "\n"
                  << "  Reused mesh refs:  " << totals.reusedMeshRefs     << "\n"
                  << "  Total vertices:    " << totals.totalVertices      << "\n"
                  << "  Total triangles:   " << totals.totalTriangles     << "\n"
                  << "  OBJ files loaded:  " << totals.objMeshesLoaded    << "\n"
                  << "  CSG evaluations:   " << totals.csgMeshesEvaluated << "\n"
                  << "  Warnings:          " << totals.warnings           << "\n";
    }

    return errors > 0 ? 1 : 0;
}
