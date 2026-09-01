// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3DependencyPruner.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3ImportResolver.hpp>
#ifdef MESH_WORLD_HAS_MCB
#include <MeshCraft/Mcb/McbWriter.hpp>
#endif

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using MeshCraft::Mc3::Mc3Document;
using MeshCraft::Mc3::Mc3ImportResolver;

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " [--mcb] <input.mc3lib.{json,xml}> <output.{json,xml,mcb}>"
              << " <root-definition> [root-definition ...]\n"
              << "\nExtracts only the selected definitions and their transitive MC3 "
                 "dependencies from a reusable library. Imports are resolved from the "
                 "input library's directory; the output is standalone (not a .mc3lib).\n";
}

bool is_json_library(const std::filesystem::path& path) {
    return path.extension() == ".json";
}

} // namespace

int main(int argc, char** argv) {
    bool write_mcb = false;
    int first_argument = 1;
    if (argc > 1 && std::string(argv[1]) == "--mcb") {
        write_mcb = true;
        ++first_argument;
    }
    if (argc - first_argument < 3) {
        print_usage(argv[0]);
        return 2;
    }

    const std::filesystem::path input = argv[first_argument];
    const std::filesystem::path output = argv[first_argument + 1];
    std::vector<std::string> roots;
    for (int i = first_argument + 2; i < argc; ++i) roots.emplace_back(argv[i]);

    try {
        Mc3Document library = is_json_library(input)
            ? Mc3Document::loadFromLibraryJsonFile(input)
            : Mc3Document::loadFromLibraryFile(input);
        const std::filesystem::path search_dir = input.parent_path().empty()
            ? std::filesystem::path{"."} : input.parent_path();
        Mc3ImportResolver({search_dir}).resolveAndMergeInto(library);
        Mc3Document standalone = MeshWorld::prune_mc3_dependencies(library, roots);

        if (write_mcb) {
#ifdef MESH_WORLD_HAS_MCB
            MeshCraft::Mcb::saveToFile(standalone, output);
#else
            std::cerr << "MeshWorldPruneMc3Lib was built without MeshCraft Mcb support; "
                         "reconfigure with the sibling mesh-craft/mcb directory available.\n";
            return 2;
#endif
        } else if (output.extension() == ".json") {
            standalone.saveToJsonFile(output);
        } else {
            standalone.saveToFile(output);
        }
        std::cout << "wrote standalone " << output << " (" << standalone.definitions.size()
                  << " definitions, " << standalone.materials.size() << " materials, "
                  << standalone.textures.size() + standalone.svgTextures.size() << " textures)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MeshWorldPruneMc3Lib: " << error.what() << '\n';
        return 1;
    }
}
