// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "MC3Validator.hpp"
#include "MaterialRegistry.hpp"
#include "Mc3MeshBuilder.hpp"
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <cstring>
#include <set>
#include <sstream>
#include <string>

namespace MeshWorld {

namespace {

// M330 -- "icosphere" added: tree.lua now emits it for canopy geometry (the
// first real generator content to use this element), so it needs the same
// id/bounds/material checks every other geometry element already gets, not
// a silent skip. "cone" added 2026-07-11 alongside the new scene:addCone
// Lua binding -- same reasoning.
static const std::set<std::string> OBJECT_ELEMENTS = {
    "box", "plane", "cylinder", "sphere", "cone", "ground", "instance", "icosphere"
};

// Parses a "x y z" attribute (MeshCraft's own vec3Str() format -- see
// mesh-craft/mc3/src/Mc3XmlWriter.cpp) into 3 floats. Returns false if the
// attribute is present but doesn't parse as exactly 3 numbers (a genuinely
// malformed value); an ABSENT attribute is not malformed -- MeshCraft's
// writer omits `position` entirely whenever it's exactly (0,0,0), so
// callers must treat "absent" as "(0,0,0)", not as an error.
bool parse_vec3(const char* raw, float& x, float& y, float& z) {
    std::istringstream iss(raw);
    if (!(iss >> x >> y >> z)) return false;
    // Reject trailing garbage after 3 valid numbers (e.g. "1 2 3 4" or "1 2 3x").
    std::string extra;
    if (iss >> extra) return false;
    return true;
}

// T128, T129, T132 (2026-07-11: rewritten -- see the "Real, pre-existing gap"
// note on MC3Validator::validate() below for why this used to silently
// check nothing on real generator output).
//
// `in_objects_section`: true only for elements reachable through <objects>
// (the actual placed-in-the-world namespace) -- duplicate-id and bounds
// checks are scoped to that namespace only. Elements inside <definitions>
// are reusable LOCAL-space templates: their own internal ids are a
// separate, definition-local namespace (two different definitions
// legitimately reusing the same internal shape id, e.g. "root", is not a
// bug), and their coordinates aren't chunk-bounded the way placed objects
// are. Every element still gets the universal checks (non-empty id,
// malformed-numeric position, unregistered-material warning) regardless.
void check_object(const tinyxml2::XMLElement* el,
                  float chunk_size_m,
                  bool in_objects_section,
                  std::set<std::string>& seen_object_ids,
                  std::set<std::string>& materials_used,
                  ValidationResult& result) {
    const char* id = el->Attribute("id");
    if (!id || id[0] == '\0') {
        result.add_error(std::string("Element <") + el->Name() +
                         "> missing or empty 'id' attribute");
        id = nullptr;
    }

    // Duplicate ids within the placed-objects namespace silently make one
    // instance unreachable/unrenderable (or ambiguous) downstream -- a real
    // structural bug the old validator never had a chance to catch (it
    // never reached anything inside <objects> at all, see validate() below).
    if (id && in_objects_section) {
        if (!seen_object_ids.insert(id).second)
            result.add_error(std::string("duplicate object id '") + id +
                             "' within <objects>");
    }

    // Position: MeshCraft's writer stores a SINGLE combined "position"
    // attribute ("x y z", space-separated -- vec3Str()), omitted entirely
    // when the position is exactly (0,0,0). The old code looked for
    // separate "x"/"z" attributes that this format never actually emits,
    // so this check always silently passed regardless of real position.
    const char* pos_attr = el->Attribute("position");
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (pos_attr) {
        if (!parse_vec3(pos_attr, x, y, z)) {
            result.add_error(std::string(id ? id : "?") +
                             ": malformed 'position' attribute \"" + pos_attr + "\"");
            pos_attr = nullptr; // don't also run a bogus bounds check below
        }
    }

    if (in_objects_section && chunk_size_m > 0.0f && pos_attr) {
        if (x < 0.0f || x > chunk_size_m)
            result.add_error(std::string(id ? id : "?") +
                             ": x=" + std::to_string(x) +
                             " outside chunk bounds [0," +
                             std::to_string(chunk_size_m) + "]");
        if (z < 0.0f || z > chunk_size_m)
            result.add_error(std::string(id ? id : "?") +
                             ": z=" + std::to_string(z) +
                             " outside chunk bounds [0," +
                             std::to_string(chunk_size_m) + "]");

        // R130b (§21.3 subset, single-chunk-computable only -- see this
        // iteration's plan Key Decision 1) -- terrain penetration: an
        // object placed meaningfully below y=0 has sunk beneath the flat
        // chunk-local ground plane every generator authors relative to
        // (MC3Writer's own ground() call). A small negative-epsilon
        // tolerance absorbs harmless float noise from generator math;
        // anything further below is a real placement bug this validator
        // CAN catch from one chunk's own XML alone -- unlike
        // building-road intersection, cross-chunk road continuity, and
        // socket/cave-connection alignment, which all need data this
        // single-chunk validator doesn't have (deferred, see this
        // iteration's plan).
        //
        // EXCEPT "water" material: RiverBankGenerator.cpp/BridgeGenerator.cpp
        // both deliberately recess their water plane 0.3-0.4m below the
        // ground plane (grep-confirmed: RiverBankGenerator's own
        // `w.plane("water", 0, -0.3f, ...)`, BridgeGenerator's own
        // `w.plane("river", 0, -0.4f, ...)`) -- an established, intentional
        // real-content convention for a recessed riverbed, not a placement
        // bug. Read below, before the position check, purely to exclude
        // this one legitimate case; the general material-used/registered
        // check further down is unaffected and still runs on every element.
        const char* mat_for_terrain_check = el->Attribute("material");
        const bool is_water = mat_for_terrain_check &&
                              std::strcmp(mat_for_terrain_check, "water") == 0;
        constexpr float kTerrainPenetrationEpsilon = 0.05f;
        if (!is_water && y < -kTerrainPenetrationEpsilon)
            result.add_error(std::string(id ? id : "?") +
                             ": y=" + std::to_string(y) +
                             " penetrates terrain (below the ground plane)");
    }

    // R122 (§21.2 "finite transform values") -- rotation/scale previously
    // had NO validation at all (not even a malformed-syntax check the way
    // position always got). A NaN/Inf float (e.g. from a stray 0/0 or an
    // unguarded log/sqrt of a negative value in a generator's own math)
    // serializes as literal "nan"/"inf" text (Mc3XmlWriter's own
    // snprintf("%.6g", ...) -- see mesh-craft's vec3Str()); parse_vec3()'s
    // istringstream-based parsing already REJECTS that text as a stream
    // extraction failure (empirically verified: libstdc++'s num_get does
    // not parse "nan"/"inf"/an out-of-float-range literal into a
    // successfully-extracted value here), so it is caught as "malformed"
    // below -- the same bucket a genuinely garbled attribute falls into,
    // not a separate "non-finite" message. (A separate std::isfinite()
    // check after a successful parse would be unreachable dead code given
    // this parsing method; not added -- see this project's own "don't
    // validate scenarios that can't happen" rule.)
    for (const char* attr_name : {"rotation", "scale"}) {
        const char* attr = el->Attribute(attr_name);
        if (!attr) continue;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        if (!parse_vec3(attr, rx, ry, rz))
            result.add_error(std::string(id ? id : "?") + ": malformed '" + attr_name +
                             "' attribute \"" + attr + "\"");
    }

    // instance elements must reference a definition id -- whether that id
    // actually RESOLVES to a real <definition> is intentionally not checked
    // here: definitions are frequently injected from a separate source
    // (WorldRenderer::inject_definitions(), ObjectDefinitionLibrary) after
    // a chunk's own XML is generated, so a standalone chunk document
    // legitimately references definitions it doesn't itself carry. This
    // only catches the structurally-broken case of an empty reference.
    if (std::strcmp(el->Name(), "instance") == 0) {
        const char* def = el->Attribute("definition");
        if (!def || def[0] == '\0')
            result.add_error(std::string(id ? id : "?") +
                             ": <instance> missing or empty 'definition' attribute");
    }

    const char* mat = el->Attribute("material");
    if (mat && mat[0] != '\0') {
        materials_used.insert(mat);
        if (!MaterialRegistry::instance().has(mat))
            result.add_warning(std::string("material '") + mat +
                               "' used by '" + (id ? id : "?") +
                               "' is not registered in MaterialRegistry");
    }
}

// R108 -- the four light element tags MeshCraft's Mc3XmlWriter emits under
// <lights> (see Mc3XmlWriter.cpp's light-writing block). No real generator
// in this project emits any of these yet (grep-confirmed); counting them
// here is forward-looking diagnostics infra, not a currently-exercised path.
bool is_light_tag(const char* tag) {
    return std::strcmp(tag, "ambient") == 0 || std::strcmp(tag, "directional") == 0 ||
           std::strcmp(tag, "spot") == 0 || std::strcmp(tag, "point") == 0;
}

// R130a (mesh_world_revival.md §21.2 remainder, R111's own "asset metadata"
// format) -- validates one <definition>'s <assetMetadata> block, when
// present. "materialSlots": each declared slot name must be non-empty and
// unique within the same definition (no slot-override authoring mechanism
// exists yet to cross-check bindings against, so this is a well-formedness
// check, not a binding-validity check -- documented honestly as such, not
// oversold). "lods": each entry needs a non-empty tier name and a non-empty
// target definition id; a SELF-referencing target (a definition listing
// itself as its own LOD proxy) is always a real, unambiguous bug and is
// flagged regardless of context. A target that doesn't match this same
// document's own <definition id="..."> is deliberately NOT flagged --
// exactly like <instance definition="...">'s own existing precedent (see
// check_object()'s comment above): LOD proxy definitions are frequently
// injected from a separate source (WorldRenderer::inject_definitions(),
// ObjectDefinitionLibrary) and a standalone chunk document legitimately
// references proxies it doesn't itself carry.
void check_asset_metadata(const tinyxml2::XMLElement* asset_metadata,
                          const std::string& owning_definition_id,
                          ValidationResult& result) {
    const std::string owner = owning_definition_id.empty() ? "?" : owning_definition_id;

    if (const tinyxml2::XMLElement* slots_el = asset_metadata->FirstChildElement("materialSlots")) {
        std::set<std::string> seen_slots;
        for (const tinyxml2::XMLElement* tag_el = slots_el->FirstChildElement("tag");
             tag_el; tag_el = tag_el->NextSiblingElement("tag")) {
            const char* value = tag_el->Attribute("value");
            if (!value || value[0] == '\0') {
                result.add_error("<definition id=\"" + owner + "\">: empty materialSlots entry");
                continue;
            }
            if (!seen_slots.insert(value).second)
                result.add_error("<definition id=\"" + owner + "\">: duplicate materialSlots entry '" +
                                 value + "'");
        }
    }

    if (const tinyxml2::XMLElement* lods_el = asset_metadata->FirstChildElement("lods")) {
        for (const tinyxml2::XMLElement* lod_el = lods_el->FirstChildElement("lod");
             lod_el; lod_el = lod_el->NextSiblingElement("lod")) {
            const char* tier = lod_el->Attribute("tier");
            const char* def  = lod_el->Attribute("definition");
            if (!tier || tier[0] == '\0')
                result.add_error("<definition id=\"" + owner + "\">: lods entry missing or empty 'tier' attribute");
            if (!def || def[0] == '\0') {
                result.add_error("<definition id=\"" + owner + "\">: lods entry missing or empty 'definition' attribute");
                continue;
            }
            if (owning_definition_id == def)
                result.add_error("<definition id=\"" + owner +
                                 "\">: dangling lods entry -- references itself ('" + std::string(def) +
                                 "') as its own LOD");
        }
    }
}

// R130a (§21.2 remainder) -- validates the document's own top-level
// <textures>/<materials> sections (MeshCraft's Mc3XmlWriter always writes
// these, when non-empty, as direct children of the <mc3> root -- never
// nested inside <objects>/<definitions>): every non-empty texture-reference
// child element on a <material> must match a declared <texture id="...">
// in this SAME document's own <textures> section. Mirrors
// Mc3DocumentBuilder::add_material()'s own existing self-consistency
// (every generated MaterialEntry with a texture_uri registers BOTH the
// baseColorTexture reference AND the matching doc_->textures[...] entry in
// the same call) as a regression guard against hand-authored/imported
// content -- or a future bug in that exact symmetry -- that isn't
// self-consistent the same way.
void check_materials_and_textures(const tinyxml2::XMLElement* root, ValidationResult& result) {
    std::set<std::string> declared_texture_ids;
    if (const tinyxml2::XMLElement* textures_el = root->FirstChildElement("textures")) {
        for (const tinyxml2::XMLElement* tex_el = textures_el->FirstChildElement("texture");
             tex_el; tex_el = tex_el->NextSiblingElement("texture")) {
            const char* tex_id = tex_el->Attribute("id");
            if (tex_id && tex_id[0] != '\0') declared_texture_ids.insert(tex_id);
        }
    }

    const tinyxml2::XMLElement* materials_el = root->FirstChildElement("materials");
    if (!materials_el) return;

    static const char* kTextureRefTags[] = {
        "base_color_texture", "normal_texture", "metallic_roughness_texture",
        "occlusion_texture", "emissive_texture"
    };
    for (const tinyxml2::XMLElement* mat_el = materials_el->FirstChildElement("material");
         mat_el; mat_el = mat_el->NextSiblingElement("material")) {
        const char* mat_id = mat_el->Attribute("id");
        for (const char* ref_tag : kTextureRefTags) {
            const tinyxml2::XMLElement* ref_el = mat_el->FirstChildElement(ref_tag);
            if (!ref_el) continue;
            const char* tex_ref = ref_el->GetText();
            if (!tex_ref || tex_ref[0] == '\0') continue;
            if (!declared_texture_ids.count(tex_ref))
                result.add_error(std::string("material '") + (mat_id ? mat_id : "?") +
                                 "' references " + ref_tag + " '" + tex_ref +
                                 "' which is not declared in <textures>");
        }
    }
}

// Recursively walks every element in the document (not just <mc3>'s direct
// children). Real generator output nests all placed geometry inside a
// single <objects> wrapper (and reusable templates inside <definitions>/
// <definition>) -- see the note on MC3Validator::validate() for why a
// direct-children-only scan used to silently check nothing.
void walk(const tinyxml2::XMLElement* el,
          float chunk_size_m,
          bool in_objects_section,
          bool in_lights_section,
          bool* has_metadata,
          std::string* generator_id,
          std::set<std::string>& seen_object_ids,
          std::set<std::string>& seen_definition_ids,
          std::set<std::string>& materials_used,
          ValidationResult& result) {
    for (const tinyxml2::XMLElement* child = el->FirstChildElement();
         child; child = child->NextSiblingElement()) {
        const char* tag = child->Name();

        if (in_lights_section && is_light_tag(tag))
            ++result.light_count;

        if (std::strcmp(tag, "metadata") == 0) {
            *has_metadata = true;
            const char* text = child->GetText();
            if (!text || text[0] == '\0') {
                result.add_error("<metadata> element is empty");
            } else {
                try {
                    auto j = nlohmann::json::parse(text);
                    if (j.contains("generator") && j["generator"].is_object()) {
                        auto& gen = j["generator"];
                        if (gen.contains("id"))
                            *generator_id = gen["id"].get<std::string>();
                    } else if (j.contains("generator_id")) {
                        *generator_id = j["generator_id"].get<std::string>();
                    }
                    if (generator_id->empty())
                        result.add_error("metadata JSON: generator id is empty or missing");
                } catch (const nlohmann::json::exception& e) {
                    result.add_error(std::string("metadata JSON parse error: ") + e.what());
                }
            }
            continue;
        }

        if (std::strcmp(tag, "definition") == 0) {
            const char* def_id = child->Attribute("id");
            if (!def_id || def_id[0] == '\0')
                result.add_error("<definition> missing or empty 'id' attribute");
            else if (!seen_definition_ids.insert(def_id).second)
                result.add_error(std::string("duplicate definition id '") + def_id + "'");
            // R130a -- validate this definition's own <assetMetadata> (R111), if present.
            if (const tinyxml2::XMLElement* ame = child->FirstChildElement("assetMetadata"))
                check_asset_metadata(ame, def_id ? def_id : "", result);
            walk(child, chunk_size_m, /*in_objects_section=*/false, in_lights_section,
                 has_metadata, generator_id, seen_object_ids, seen_definition_ids,
                 materials_used, result);
            continue;
        }

        const bool entering_objects = std::strcmp(tag, "objects") == 0;
        const bool entering_lights  = std::strcmp(tag, "lights") == 0;
        if (OBJECT_ELEMENTS.count(tag)) {
            check_object(child, chunk_size_m, in_objects_section, seen_object_ids,
                        materials_used, result);
            if (in_objects_section) {
                ++result.object_count;
                // R130b (§21.4 "performance validation") -- <instance>-only
                // count, split out of the more generic object_count above.
                if (std::strcmp(tag, "instance") == 0)
                    ++result.instance_count;
            }
        }

        // Recurse regardless of tag -- group/objects/definitions/union/etc.
        // all nest further geometry that still needs the same checks.
        walk(child, chunk_size_m, in_objects_section || entering_objects,
             in_lights_section || entering_lights,
             has_metadata, generator_id, seen_object_ids, seen_definition_ids,
             materials_used, result);
    }
}

} // namespace

ValidationResult MC3Validator::validate(const std::string& xml, float chunk_size_m) const {
    ValidationResult result;

    // T127 — well-formed XML
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
        result.add_error(std::string("XML parse error: ") + doc.ErrorStr());
        return result;
    }

    // T127 — <mc3> root element
    const tinyxml2::XMLElement* root = doc.FirstChildElement("mc3");
    if (!root) {
        result.add_error("Root element <mc3> not found");
        return result;
    }

    // Real, pre-existing gap (found + fixed 2026-07-11, procedural-model-
    // generator-roadmap infra pass): this used to iterate only root's
    // DIRECT children looking for tags in OBJECT_ELEMENTS. MeshCraft's own
    // writer (mesh-craft/mc3/src/Mc3XmlWriter.cpp, used by EVERY real
    // generator in this project -- MC3Writer.hpp's own doc comment: "Builds
    // ... Delegates to Mc3SceneBuilder -> Mc3DocumentBuilder -> MeshCraft
    // Mc3XmlWriter") always nests actual geometry one level deeper inside
    // an <objects> wrapper (and reusable templates inside <definitions>/
    // <definition>), and "objects"/"definitions" were never themselves in
    // OBJECT_ELEMENTS -- so check_object() was NEVER actually invoked on
    // any real generator's output, only on hand-written test XML that
    // (understandably, but incorrectly) put <box> directly under <mc3>.
    // MC3ValidatorTests.ParkGeneratorPassesValidation "passed" because
    // there was nothing left to check, not because the output was actually
    // validated. Compounding this: even a direct <box> child was checked
    // against separate "x"/"z" attributes that MeshCraft's writer has never
    // emitted -- real position lives in one combined "position" attribute,
    // and is OMITTED ENTIRELY when the position is exactly the origin
    // (Mc3XmlWriter's own nonzero3() gate) -- so even a reached bounds check
    // always silently passed. Both fixed below: a full recursive tree walk
    // (walk(), above) instead of one flat loop, and parse_vec3() reading
    // the real "position" attribute format.
    bool has_metadata = false;
    std::string generator_id;
    std::set<std::string> seen_object_ids;
    std::set<std::string> seen_definition_ids;
    std::set<std::string> materials_used;
    walk(root, chunk_size_m, /*in_objects_section=*/false, /*in_lights_section=*/false,
         &has_metadata, &generator_id, seen_object_ids, seen_definition_ids,
         materials_used, result);

    // R130a (§21.2 remainder) -- document-internal material/texture
    // cross-reference check; see check_materials_and_textures()'s own
    // doc comment.
    check_materials_and_textures(root, result);

    if (!has_metadata)
        result.add_error("<metadata> element not found inside <mc3>");

    result.generator_id = generator_id;
    result.materials_used.assign(materials_used.begin(), materials_used.end());
    result.material_count = static_cast<int>(result.materials_used.size());

    // R130b (§21.4 "performance validation") -- instance_count is filled in
    // directly by walk() above (see its own comment). triangle_count reuses
    // the EXACT SAME Mc3MeshBuilder computation ChunkPipeline::get() already
    // makes at src/ChunkPipeline.cpp:306 for ChunkDiagnostics -- Mc3MeshBuilder
    // returns an empty MeshList on parse error, so this is safe to call
    // unconditionally, even for XML that failed earlier checks above.
    // draw_call_estimate is a conservative placeholder (see its own doc
    // comment in ValidationResult.hpp), not a real batching-aware estimate.
    result.triangle_count     = Mc3MeshBuilder{}.build(xml).total_triangles();
    result.draw_call_estimate = result.object_count;

    return result;
}

} // namespace MeshWorld
