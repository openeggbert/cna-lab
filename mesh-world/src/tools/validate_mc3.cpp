// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldValidate — standalone MC3Validator runner. Reads one or more
// mc3.xml files from disk (independent of any ChunkPipeline/world config —
// useful for validating hand-edited or externally-produced MC3 files, not
// just MeshWorld's own generated chunks) and prints errors/warnings.
//
// Usage:
//   MeshWorldValidate [--chunk-size N] <file.mc3.xml> [file2.mc3.xml ...]
//
// --chunk-size N   Bounds-check objects against [0, N] meters (default: 64,
//                  MeshWorld's own WorldConfig::chunk_size_m default).
//                  Pass 0 to skip bounds checking entirely.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "BuiltinMaterials.hpp"
#include "MC3Validator.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--chunk-size N] <file.mc3.xml> [file2.mc3.xml ...]\n"
              << "  --chunk-size N   Bounds-check objects against [0, N] meters (default: 64)\n"
              << "                   Pass 0 to skip bounds checking.\n";
}

bool read_file(const std::string& path, std::string* out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    float                    chunk_size_m = 64.0f;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--chunk-size") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --chunk-size requires a value\n";
                return 1;
            }
            chunk_size_m = std::stof(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // Without this, every material in every file would report as
    // "not registered" (MaterialRegistry starts empty) -- the same real
    // catalogue export_chunks.cpp/main.cpp register before generating.
    MeshWorld::register_builtin_materials();

    MeshWorld::MC3Validator validator;
    int files_with_errors   = 0;
    int files_with_warnings = 0;
    int files_missing       = 0;

    for (const auto& path : files) {
        std::string xml;
        if (!read_file(path, &xml)) {
            std::cerr << "ERROR " << path << ": cannot open file\n";
            ++files_missing;
            continue;
        }

        const auto vr = validator.validate(xml, chunk_size_m);
        if (!vr.errors.empty()) {
            ++files_with_errors;
            std::cout << "ERROR " << path
                       << " (" << (vr.generator_id.empty() ? "?" : vr.generator_id) << "):\n";
            for (const auto& e : vr.errors) std::cout << "  " << e << "\n";
        }
        if (!vr.warnings.empty()) {
            ++files_with_warnings;
            std::cout << "WARNING " << path
                       << " (" << (vr.generator_id.empty() ? "?" : vr.generator_id) << "):\n";
            for (const auto& w : vr.warnings) std::cout << "  " << w << "\n";
        }
        if (vr.errors.empty() && vr.warnings.empty()) {
            std::cout << "OK " << path
                       << " (" << (vr.generator_id.empty() ? "?" : vr.generator_id)
                       << ", " << vr.object_count << " objects, "
                       << vr.instance_count << " instances, "
                       << vr.material_count << " materials, "
                       << vr.light_count << " lights, "
                       // R130b (§21.4 "performance validation") -- printed
                       // alongside the existing counts above, same style.
                       << vr.triangle_count << " triangles, "
                       << "~" << vr.draw_call_estimate << " draw calls)\n";
        }
    }

    std::cout << "\n" << files.size() << " file(s): "
               << files_with_errors   << " with errors, "
               << files_with_warnings << " with warnings, "
               << files_missing       << " missing/unreadable\n";

    return (files_with_errors > 0 || files_missing > 0) ? 1 : 0;
}
