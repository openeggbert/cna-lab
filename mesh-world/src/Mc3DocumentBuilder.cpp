// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Mc3DocumentBuilder.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Environment.hpp>
#include <MeshCraft/Mc3/Mc3Light.hpp>
#include <MeshCraft/Mc3/Mc3Material.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <MeshCraft/Mc3/Mc3Texture.hpp>
#include <MeshCraft/Mc3/Mc3Transform.hpp>
#include "MaterialRegistry.hpp"

#include <atomic>
#include <fstream>
#include <stdexcept>

using namespace MeshCraft::Mc3;

namespace MeshWorld {

namespace {

std::shared_ptr<Mc3Object> make_obj(ObjectType type,
                                    const std::string& id,
                                    const std::string& material,
                                    const Mc3Transform& t) {
    auto obj      = std::make_shared<Mc3Object>();
    obj->type      = type;
    obj->id        = id;
    obj->material  = material;
    obj->transform = t;
    return obj;
}

} // namespace

Mc3DocumentBuilder::Mc3DocumentBuilder(const std::string& model,
                                       float chunk_size_m,
                                       int coord_x, int coord_y)
    : doc_(std::make_unique<Mc3Document>())
    , chunk_size_m_(chunk_size_m)
    , coord_x_(coord_x)
    , coord_y_(coord_y)
{
    doc_->version = "0.1";
    doc_->model   = model;

    // Lights: directional sun + cool ambient fill.
    doc_->addLight(Mc3Light::directional("sun",
        {-0.4f, -1.0f, -0.6f},
        {1.0f,  0.95f, 0.85f},
        1.0f));
    doc_->addLight(Mc3Light::ambient("ambient",
        {0.30f, 0.35f, 0.40f},
        0.6f));

    // Environment: sky blue background + linear fog that fades geometry
    // into the sky colour so there is no hard clip edge in the distance.
    Mc3Environment env;
    env.backgroundColor = {0.55f, 0.75f, 0.95f};
    Mc3Fog fog;
    fog.mode  = FogMode::Linear;
    fog.color = {0.70f, 0.82f, 0.93f};
    fog.start = 80.0f;
    fog.end   = 220.0f;
    env.fog   = fog;
    doc_->setEnvironment(env);
}

Mc3DocumentBuilder::~Mc3DocumentBuilder() = default;

void Mc3DocumentBuilder::ground(const std::string& material, float y) {
    plane("ground", 0.0f, 0.0f, chunk_size_m_, chunk_size_m_, material, y);
}

void Mc3DocumentBuilder::plane(const std::string& id,
                                float x, float z, float sx, float sz,
                                const std::string& material, float y, float ry,
                                float rx, float rz) {
    Mc3Transform t;
    // x,z is min corner → center = (x + sx/2, y, z + sz/2)
    t.position = {x + sx / 2.0f, y, z + sz / 2.0f};
    if (rx != 0.0f || ry != 0.0f || rz != 0.0f) t.rotation = {rx, ry, rz};

    auto obj = make_obj(ObjectType::Plane, id, material, t);

    Mc3Primitive prim;
    prim.size = {sx, 0.0f, sz};
    obj->primitive = prim;

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::box(const std::string& id,
                              float x, float z, float sx, float sy, float sz,
                              const std::string& material, float y, float ry,
                              float rx, float rz) {
    Mc3Transform t;
    // x,z is center; y is base → center height = y + sy/2
    t.position = {x, y + sy / 2.0f, z};
    if (rx != 0.0f || ry != 0.0f || rz != 0.0f) t.rotation = {rx, ry, rz};

    auto obj = make_obj(ObjectType::Box, id, material, t);

    Mc3Primitive prim;
    prim.size = {sx, sy, sz};
    obj->primitive = prim;

    // M-fix (player collision): Mc3Object::collision already exists in the
    // MC3 schema for exactly this (mc3.xsd's own default="none"; the
    // MeshCraft editor's PropertiesPanel offers "none"/"box"/"sphere"/
    // "mesh"/"convex"/"capsule") but no mesh-world generator had ever set
    // it, so nothing any generator built was ever actually marked solid.
    // Every inline box (walls, houses, cliffs, roofs -- the vast majority
    // of what generators build via w.box()/scene:addBox(), as opposed to
    // addGround()'s separate Plane type or addInstance()'s decorative
    // props) is structural, so this defaults every one of them to the
    // format's own "box" collision shape instead of touching every
    // individual generator call site. WorldRenderer's collision query
    // (apps/mesh-world-app) still applies its own short/thin-object height
    // filter on top of this at query time, so a low curb or road marking
    // built via box() doesn't block the player just because it's
    // collision="box" at the data level.
    obj->collision = "box";

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::cylinder(const std::string& id,
                                   float x, float z,
                                   float radius, float height,
                                   const std::string& material, float y,
                                   float ry, float rx, float rz) {
    Mc3Transform t;
    // x,z is center; y is base → center height = y + height/2
    t.position = {x, y + height / 2.0f, z};
    // G12 -- a cylinder's own axis defaults to Y (up); rx/rz let a caller
    // tip it onto its side (e.g. a real cylindrical wheel rolling around a
    // horizontal axle) instead of only ever standing upright.
    if (rx != 0.0f || ry != 0.0f || rz != 0.0f) t.rotation = {rx, ry, rz};

    auto obj = make_obj(ObjectType::Cylinder, id, material, t);

    Mc3Primitive prim;
    prim.radius = radius;
    prim.height = height;
    obj->primitive = prim;

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::sphere(const std::string& id,
                                 float x, float z, float radius,
                                 const std::string& material, float y) {
    Mc3Transform t;
    // x,z is center; y is base → center height = y + radius
    t.position = {x, y + radius, z};

    auto obj = make_obj(ObjectType::Sphere, id, material, t);

    Mc3Primitive prim;
    prim.radius = radius;
    obj->primitive = prim;

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::cone(const std::string& id,
                               float x, float z,
                               float radius, float height,
                               const std::string& material, float y) {
    Mc3Transform t;
    // x,z is center; y is base → center height = y + height/2 (mirrors cylinder)
    t.position = {x, y + height / 2.0f, z};

    auto obj = make_obj(ObjectType::Cone, id, material, t);

    Mc3Primitive prim;
    prim.radius = radius;
    prim.height = height;
    obj->primitive = prim;

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::icoSphere(const std::string& id,
                                    float x, float z, float radius,
                                    const std::string& material, float y,
                                    float sx, float sy, float sz) {
    Mc3Transform t;
    // x,z is center; y is base → center height = y + radius (mirrors sphere()).
    // Non-uniform scale (default 1,1,1) lets a caller deform the icosphere
    // into an ellipsoid -- ObjectDefinitionLibrary's own C++ tree shapes
    // (e.g. birch_tree()'s tall-narrow canopy) already rely on exactly this.
    t.position = {x, y + radius, z};
    if (sx != 1.0f || sy != 1.0f || sz != 1.0f) t.scale = {sx, sy, sz};

    auto obj = make_obj(ObjectType::IcoSphere, id, material, t);

    Mc3Primitive prim;
    prim.primitiveType = PrimitiveType::IcoSphere;
    prim.radius = radius;
    // Mc3Primitive::segments doubles as "subdivision level" for IcoSphere
    // (unlike its facet-count meaning for Sphere/Cone) -- 2 matches both
    // Mc3Object::makeIcoSphere()'s own default and every icosphere
    // ObjectDefinitionLibrary.cpp's shape helpers already create.
    prim.segments = 2;
    obj->primitive = prim;

    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::instance(const std::string& id,
                                   const std::string& definition,
                                   float x, float z, float ry, float y, float scale,
                                   float rx, float rz) {
    Mc3Transform t;
    t.position = {x, y, z};
    if (rx != 0.0f || ry != 0.0f || rz != 0.0f) t.rotation = {rx, ry, rz};
    if (scale != 1.0f) t.scale    = {scale, scale, scale};

    auto obj = make_obj(ObjectType::Instance, id, "", t);
    obj->definition = definition;
    doc_->objects.push_back(obj);
}

void Mc3DocumentBuilder::add_material(const std::string& id,
                                       float r, float g, float b, float a,
                                       float roughness, float metallic) {
    Mc3Material mat;
    mat.name      = id;
    mat.baseColor = {r, g, b, a};
    mat.roughness = roughness;
    mat.metallic  = metallic;
    if (MaterialRegistry::instance().has(id)) {
        const auto& entry = MaterialRegistry::instance().get(id);
        if (!entry.texture_uri.empty()) {
            mat.baseColorTexture = entry.texture_uri;
            if (!doc_->textures.count(entry.texture_uri))
                doc_->textures[entry.texture_uri] =
                    Mc3Texture(entry.texture_uri, entry.texture_uri);
        }
    }
    doc_->materials[id] = mat;
}

void Mc3DocumentBuilder::set_metadata_json(const std::string& json) {
    metadata_json_ = json;
}

void Mc3DocumentBuilder::build_to_file(const std::filesystem::path& path) const {
    if (metadata_json_.empty()) {
        doc_->saveToFile(path);
        return;
    }
    // Write with injected <metadata> tag.
    std::string xml = build_to_string();
    std::ofstream ofs(path);
    if (!ofs.is_open())
        throw std::runtime_error("Mc3DocumentBuilder: cannot write " + path.string());
    ofs << xml;
}

std::string Mc3DocumentBuilder::build_to_string() const {
    static std::atomic<uint64_t> counter{0};
    auto n = counter.fetch_add(1, std::memory_order_relaxed);
    auto tmp = std::filesystem::temp_directory_path() /
               ("mw_tmp_" + std::to_string(n) + ".mc3.xml");

    doc_->saveToFile(tmp);

    std::ifstream ifs(tmp);
    if (!ifs.is_open())
        throw std::runtime_error("Mc3DocumentBuilder: failed to read " + tmp.string());

    std::string result((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    std::filesystem::remove(tmp);

    // Inject <metadata> tag as first child of <mc3 ...>.
    // The XML output starts with <?xml ...?><mc3 ...> — we must skip the
    // processing instruction and find the closing '>' of the <mc3> root tag.
    if (!metadata_json_.empty()) {
        auto mc3_start = result.find("<mc3");
        if (mc3_start != std::string::npos) {
            auto pos = result.find('>', mc3_start);
            if (pos != std::string::npos) {
                std::string meta =
                    "\n    <metadata format=\"json\" type=\"generation\"><![CDATA[\n"
                    + metadata_json_
                    + "\n    ]]></metadata>";
                result.insert(pos + 1, meta);
            }
        }
    }

    return result;
}

} // namespace MeshWorld
