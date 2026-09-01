// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3DependencyPruner.hpp"

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace MeshWorld {

namespace {

using MeshCraft::Mc3::Mc3Document;
using MeshCraft::Mc3::Mc3Material;
using MeshCraft::Mc3::Mc3Object;

struct Reachability {
    const Mc3Document& source;
    std::set<std::string> definitions;
    std::set<std::string> materials;

    explicit Reachability(const Mc3Document& document) : source(document) {}

    void add_material(const std::string& id) {
        if (!id.empty()) materials.insert(id);
    }

    void visit_object(const std::shared_ptr<Mc3Object>& object) {
        if (!object) return;
        add_material(object->material);
        add_material(object->materialOverride);
        for (const auto& [name, state] : object->states) {
            static_cast<void>(name);
            if (state.material) add_material(*state.material);
        }
        if (!object->scriptId.empty())
            throw std::runtime_error("MC3 dependency pruning: definition contains runtime script '" +
                                     object->scriptId + "'; compile/expand it through the separately "
                                     "scoped R104 path before pruning");
        if (!object->definition.empty()) add_definition(object->definition);
        for (const auto& definition : object->variantDefinitions) add_definition(definition);
        if (object->assetMetadata) {
            for (const auto& [tier, definition] : object->assetMetadata->lods) {
                static_cast<void>(tier);
                add_definition(definition);
            }
        }
        for (const auto& child : object->children) visit_object(child);
    }

    void add_definition(const std::string& id) {
        if (id.empty() || !definitions.insert(id).second) return;
        const auto found = source.definitions.find(id);
        if (found == source.definitions.end() || !found->second)
            throw std::runtime_error("MC3 dependency pruning: unresolved definition '" + id + "'");
        visit_object(found->second);
    }
};

std::shared_ptr<Mc3Object> clone_object(
    const std::shared_ptr<Mc3Object>& source,
    std::map<const Mc3Object*, std::shared_ptr<Mc3Object>>& copies) {
    if (!source) return {};
    if (const auto found = copies.find(source.get()); found != copies.end()) return found->second;

    auto copy = std::make_shared<Mc3Object>(*source);
    copies.emplace(source.get(), copy);
    copy->children.clear();
    copy->children.reserve(source->children.size());
    for (const auto& child : source->children) copy->children.push_back(clone_object(child, copies));
    return copy;
}

} // namespace

MeshCraft::Mc3::Mc3Document prune_mc3_dependencies(
    const MeshCraft::Mc3::Mc3Document& resolved_library,
    const std::vector<std::string>& root_definition_ids) {
    if (root_definition_ids.empty())
        throw std::invalid_argument("MC3 dependency pruning requires at least one root definition");

    Reachability reachable(resolved_library);
    for (const auto& id : root_definition_ids) reachable.add_definition(id);

    Mc3Document output;
    output.version = resolved_library.version;
    output.model = resolved_library.model;
    output.unit = resolved_library.unit;
    output.coordinateSystem = resolved_library.coordinateSystem;
    output.rotationUnits = resolved_library.rotationUnits;
    output.eulerOrder = resolved_library.eulerOrder;
    output.metadata = resolved_library.metadata;
    output.meta = resolved_library.meta;
    output.sourcePath = resolved_library.sourcePath;
    output.environment = resolved_library.environment;
    output.lights = resolved_library.lights;
    output.cameras = resolved_library.cameras;
    output.defaultCamera = resolved_library.defaultCamera;
    // `library`, `imports`, and include skip sets deliberately stay empty:
    // this is a standalone compiled scene, not a library export.

    std::map<const Mc3Object*, std::shared_ptr<Mc3Object>> object_copies;
    for (const auto& id : reachable.definitions)
        output.definitions[id] = clone_object(resolved_library.definitions.at(id), object_copies);

    std::set<std::string> texture_ids;
    for (const auto& id : reachable.materials) {
        const auto found = resolved_library.materials.find(id);
        if (found == resolved_library.materials.end()) continue;
        const Mc3Material& material = found->second;
        output.materials[id] = material;
        for (const auto* texture : {&material.baseColorTexture, &material.normalTexture,
                                    &material.emissiveTexture, &material.metallicRoughnessTexture,
                                    &material.occlusionTexture}) {
            if (!texture->empty()) texture_ids.insert(*texture);
        }
    }
    for (const auto& id : texture_ids) {
        if (const auto bitmap = resolved_library.textures.find(id);
            bitmap != resolved_library.textures.end())
            output.textures[id] = bitmap->second;
        if (const auto svg = resolved_library.svgTextures.find(id);
            svg != resolved_library.svgTextures.end())
            output.svgTextures[id] = svg->second;
    }
    return output;
}

} // namespace MeshWorld
