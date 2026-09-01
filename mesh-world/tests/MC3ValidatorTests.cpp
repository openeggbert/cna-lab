// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "MC3Validator.hpp"
#include "BuiltinMaterials.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "generators/ParkGenerator.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

MeshWorld::ChunkContext make_ctx(float size = 64.0f) {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = 1;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.chunk_size_m = size;
    return ctx;
}

// Minimal valid MC3 XML -- matches MeshCraft's REAL Mc3XmlWriter output
// shape (mesh-craft/mc3/src/Mc3XmlWriter.cpp): geometry nests inside an
// <objects> wrapper, and position is one combined "x y z" attribute, not
// separate x=/y=/z= attributes (2026-07-11: the OLD fixtures here used a
// flat, non-<objects>-wrapped, separate-x/z-attribute shape that no real
// generator has ever actually produced -- see MC3Validator.cpp's own
// validate() comment for the full story on how that let two real
// validator bugs go unnoticed for a long time).
const char* VALID_MC3 = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen","version":"0.1"}}</metadata>
<objects>
<box id="box1" position="10 0 10" size="2 2 2" material="grass_park"/>
</objects>
</mc3>)xml";

// R131 uses only this deliberately small, standard Draft 2020-12 subset:
// $ref, type/const/enum, required/properties/additionalProperties, array
// items, string lengths, and basic numeric/array bounds. Keeping the test evaluator local
// avoids introducing a JSON-Schema runtime dependency into MeshWorld just to
// test a declarative interchange contract; production consumers can use any
// full Draft 2020-12 implementation.
using Json = nlohmann::json;

const Json& resolve_schema_ref(const Json& schema, const Json& root) {
    if (!schema.contains("$ref")) return schema;
    const std::string ref = schema.at("$ref").get<std::string>();
    if (ref.rfind("#/$defs/", 0) != 0)
        throw std::runtime_error("unsupported JSON Schema reference: " + ref);
    return root.at("$defs").at(ref.substr(std::string("#/$defs/").size()));
}

bool has_json_type(const Json& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "boolean") return value.is_boolean();
    if (type == "number") return value.is_number();
    if (type == "integer") return value.is_number_integer() || value.is_number_unsigned();
    return false;
}

void validate_schema_subset(const Json& schema_input, const Json& value,
                            const Json& root, const std::string& path,
                            std::vector<std::string>& errors) {
    const Json& schema = resolve_schema_ref(schema_input, root);
    if (schema.contains("const") && value != schema.at("const"))
        errors.push_back(path + " does not equal its required const value");
    if (schema.contains("enum")) {
        const auto& values = schema.at("enum");
        if (std::find(values.begin(), values.end(), value) == values.end())
            errors.push_back(path + " is not an allowed enum value");
    }
    if (schema.contains("type")) {
        const std::string type = schema.at("type").get<std::string>();
        if (!has_json_type(value, type)) {
            errors.push_back(path + " has the wrong JSON type (expected " + type + ")");
            return;
        }
    }
    if (value.is_number()) {
        const double number = value.get<double>();
        if (schema.contains("minimum") && number < schema.at("minimum").get<double>())
            errors.push_back(path + " is below its minimum");
        if (schema.contains("maximum") && number > schema.at("maximum").get<double>())
            errors.push_back(path + " is above its maximum");
        if (schema.contains("exclusiveMinimum") &&
            number <= schema.at("exclusiveMinimum").get<double>())
            errors.push_back(path + " is not above its exclusive minimum");
    }
    if (value.is_string() && schema.contains("minLength") &&
        value.get_ref<const std::string&>().size() < schema.at("minLength").get<std::size_t>())
        errors.push_back(path + " is shorter than its minimum length");
    if (value.is_array()) {
        if (schema.contains("minItems") && value.size() < schema.at("minItems").get<std::size_t>())
            errors.push_back(path + " has too few array items");
        if (schema.contains("maxItems") && value.size() > schema.at("maxItems").get<std::size_t>())
            errors.push_back(path + " has too many array items");
        if (schema.contains("items")) {
            for (std::size_t i = 0; i < value.size(); ++i)
                validate_schema_subset(schema.at("items"), value.at(i), root,
                                       path + "[" + std::to_string(i) + "]", errors);
        }
    }
    if (!value.is_object()) return;

    if (schema.contains("required")) {
        for (const auto& name : schema.at("required")) {
            const std::string key = name.get<std::string>();
            if (!value.contains(key)) errors.push_back(path + " is missing required property '" + key + "'");
        }
    }
    const Json* properties = schema.contains("properties") ? &schema.at("properties") : nullptr;
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (properties && properties->contains(it.key())) {
            validate_schema_subset(properties->at(it.key()), it.value(), root,
                                   path + "." + it.key(), errors);
        } else if (schema.contains("additionalProperties")) {
            const auto& additional = schema.at("additionalProperties");
            if (additional.is_boolean() && !additional.get<bool>()) {
                errors.push_back(path + " has unknown property '" + it.key() + "'");
            } else if (additional.is_object()) {
                validate_schema_subset(additional, it.value(), root,
                                       path + "." + it.key(), errors);
            }
        }
    }
}

std::string join_errors(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (const auto& error : errors) out << "\n  " << error;
    return out.str();
}

std::filesystem::path find_project_root_for_mc3_schema() {
    const std::filesystem::path current = std::filesystem::current_path();
    for (const auto& candidate : {current, current.parent_path()}) {
        if (std::filesystem::is_regular_file(candidate / "schemas/mc3.schema.json") &&
            std::filesystem::is_directory(candidate / "data/mc3lib"))
            return candidate;
    }
    throw std::runtime_error("could not locate MeshWorld's schemas/ and data/mc3lib/ directories");
}

} // namespace

// T135 — valid MC3 from ParkGenerator passes validation
TEST(MC3ValidatorTests, ParkGeneratorPassesValidation) {
    MeshWorld::register_builtin_materials();

    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    std::string xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    ASSERT_NE(xml.find("<objects>"), std::string::npos)
        << "sanity check: real generator output must actually nest geometry "
           "inside <objects> the way this whole test file assumes";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, ctx.chunk_size_m);

    for (const auto& e : result.errors)
        ADD_FAILURE() << "Validation error: " << e;

    EXPECT_TRUE(result.ok) << "ParkGenerator output failed validation";
}

// 2026-07-11 -- the real proof the validator now actually reaches inside
// <objects>: corrupt one position in REAL generator output and confirm the
// validator catches it. Before the recursion/position-parsing fix, this
// would have silently passed (nothing inside <objects> was ever checked).
TEST(MC3ValidatorTests, RealGeneratorOutputWithCorruptedPositionFailsValidation) {
    MeshWorld::register_builtin_materials();

    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    std::string xml = gen.generate(ctx);
    ASSERT_FALSE(xml.empty());

    // Find the first real position="..." attribute in <objects> and replace
    // it with a wildly out-of-bounds value.
    auto objects_start = xml.find("<objects>");
    ASSERT_NE(objects_start, std::string::npos);
    auto pos_attr = xml.find("position=\"", objects_start);
    ASSERT_NE(pos_attr, std::string::npos)
        << "expected at least one positioned object in real ParkGenerator output";
    auto value_start = pos_attr + std::strlen("position=\"");
    auto value_end   = xml.find('"', value_start);
    ASSERT_NE(value_end, std::string::npos);
    xml.replace(value_start, value_end - value_start, "99999 0 99999");

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, ctx.chunk_size_m);

    EXPECT_FALSE(result.ok) << "expected the corrupted out-of-bounds position to be caught";
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("99999") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "expected an error mentioning the corrupted coordinate";
}

// T136 — MC3 with missing <metadata> fails validation
TEST(MC3ValidatorTests, MissingMetadataFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<objects>
<box id="b1" position="5 0 5" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("metadata") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected error mentioning 'metadata'";
}

// T137 — MC3 with object outside chunk bounds fails validation
TEST(MC3ValidatorTests, OutOfBoundsObjectFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="out_of_bounds" position="999 0 999" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("out_of_bounds") != std::string::npos ||
            e.find("x=") != std::string::npos ||
            e.find("z=") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected out-of-bounds error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

// An object at the ORIGIN legitimately omits its "position" attribute
// entirely (Mc3XmlWriter's own nonzero3() gate) -- must not be misread as
// out-of-bounds, or as malformed.
TEST(MC3ValidatorTests, ObjectAtOriginWithNoPositionAttributePasses) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="origin_box" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, MalformedXmlFails) {
    MeshWorld::MC3Validator validator;
    auto result = validator.validate("<mc3><unclosed>", 64.0f);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errors.empty());
}

TEST(MC3ValidatorTests, WrongRootElementFails) {
    MeshWorld::MC3Validator validator;
    auto result = validator.validate("<scene><box id=\"b1\"/></scene>", 64.0f);
    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("mc3") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(MC3ValidatorTests, ObjectMissingIdFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box position="10 0 10" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("id") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected error about missing id";
}

TEST(MC3ValidatorTests, ValidXmlPassesNoBoundsCheck) {
    MeshWorld::MC3Validator validator;
    auto result = validator.validate(VALID_MC3, 0.0f);
    // No bounds check when chunk_size_m==0
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, UnregisteredMaterialWarnNotError) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="5 0 5" size="1 1 1" material="definitely_unknown_xyz"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    // Unregistered material is a WARNING, not an error — ok should be true
    EXPECT_TRUE(result.ok) << "Unregistered material should be a warning, not an error";
    EXPECT_FALSE(result.warnings.empty()) << "Expected a warning for unknown material";
}

// ── 2026-07-11 additions: duplicate ids, malformed position, instance refs ──

TEST(MC3ValidatorTests, DuplicateObjectIdWithinObjectsFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="dup" position="1 0 1" size="1 1 1" material="grass_park"/>
<box id="dup" position="2 0 2" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("duplicate") != std::string::npos && e.find("dup") != std::string::npos)
            { found = true; break; }
    EXPECT_TRUE(found) << "Expected a duplicate-id error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, MalformedPositionAttributeFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="not a number" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("malformed") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected a malformed-position error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

// R122 (§21.2 "finite transform values") -- a NaN/Inf position (e.g. from
// an unguarded 0/0 in a generator's own math) serializes as literal
// "nan"/"inf" text (Mc3XmlWriter's own vec3Str(), snprintf("%.6g", ...)).
// Empirically, libstdc++'s istringstream-based float parsing rejects that
// text outright (extraction failure, not a successfully-parsed NaN/Inf
// value) -- so this is caught by the SAME "malformed" check a garbled
// attribute triggers, not a separate non-finite-specific message. This
// test proves the scenario is still caught, whichever bucket the message
// falls into.
TEST(MC3ValidatorTests, NonFinitePositionAttributeFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="nan 0 0" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("malformed") != std::string::npos && e.find("position") != std::string::npos) {
            found = true;
            break;
        }
    EXPECT_TRUE(found) << "Expected a malformed-position error for a NaN literal. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

// R122 -- rotation previously had NO validation at all (not even the
// malformed-syntax check position always got); this proves it now does.
TEST(MC3ValidatorTests, NonFiniteRotationAttributeFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="1 0 1" rotation="0 inf 0" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("malformed") != std::string::npos && e.find("rotation") != std::string::npos) {
            found = true;
            break;
        }
    EXPECT_TRUE(found) << "Expected a malformed-rotation error for an Inf literal. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, MalformedScaleAttributeFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="1 0 1" scale="not a number" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("malformed") != std::string::npos && e.find("scale") != std::string::npos) {
            found = true;
            break;
        }
    EXPECT_TRUE(found) << "Expected a malformed-scale error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, FiniteRotationAndScaleStillPass) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="1 0 1" rotation="0 45 0" scale="2 2 2" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected validation error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, InstanceMissingDefinitionAttributeFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<instance id="i1" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("definition") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected a missing-definition error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, InstanceReferencingAnUnresolvedDefinitionIsNotAnErrorHere) {
    // Definitions are frequently injected from a separate source
    // (WorldRenderer::inject_definitions()) after a chunk's own XML is
    // generated -- a standalone chunk document referencing a definition id
    // it doesn't itself carry is normal, not a structural bug this
    // per-document validator can or should catch.
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<instance id="i1" definition="tree_pine" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, DefinitionInternalIdsCanRepeatAcrossDefinitionsWithoutError) {
    // Two independent <definition> templates both naming their own root
    // shape "root" is normal (each definition is a self-contained little
    // object tree, not part of the placed-objects namespace) -- must not
    // be flagged as a duplicate id the way it would be inside <objects>.
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="tree_pine"><box id="root" size="1 1 1" material="foliage_pine"/></definition>
<definition id="tree_oak"><box id="root" size="1 1 1" material="foliage_oak"/></definition>
</definitions>
<objects>
<instance id="i1" definition="tree_pine" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, DuplicateDefinitionIdFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="tree_pine"><box id="root" size="1 1 1" material="foliage_pine"/></definition>
<definition id="tree_pine"><box id="root" size="1 1 1" material="foliage_pine"/></definition>
</definitions>
<objects>
<instance id="i1" definition="tree_pine" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("duplicate definition id") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected a duplicate-definition-id error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

// ── R130a: material/texture reference validity (§21.2 remainder) ──

TEST(MC3ValidatorTests, DanglingTextureReferenceFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<materials>
<material id="wall_brick"><base_color_texture>tex_missing</base_color_texture></material>
</materials>
<objects>
<box id="b1" position="1 0 1" size="1 1 1" material="wall_brick"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("wall_brick") != std::string::npos && e.find("tex_missing") != std::string::npos)
            { found = true; break; }
    EXPECT_TRUE(found) << "Expected a dangling-texture-reference error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

// Mirrors Mc3DocumentBuilder::add_material()'s own real output shape: every
// material with a texture reference also registers a matching <texture id>
// entry in the same document -- must pass with zero errors.
TEST(MC3ValidatorTests, SelfConsistentMaterialAndTexturePasses) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<textures>
<texture id="tex_brick" uri="tex_brick"/>
</textures>
<materials>
<material id="wall_brick"><base_color_texture>tex_brick</base_color_texture></material>
</materials>
<objects>
<box id="b1" position="1 0 1" size="1 1 1" material="wall_brick"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, MultipleDanglingTextureRefTagsAllReported) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<materials>
<material id="wall_brick">
<base_color_texture>tex_missing_1</base_color_texture>
<normal_texture>tex_missing_2</normal_texture>
</material>
</materials>
<objects>
<box id="b1" position="1 0 1" size="1 1 1" material="wall_brick"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    int found = 0;
    for (const auto& e : result.errors)
        if (e.find("tex_missing_1") != std::string::npos || e.find("tex_missing_2") != std::string::npos)
            ++found;
    EXPECT_EQ(found, 2) << "Expected both dangling texture references to be reported independently";
}

// ── R130a: assetMetadata materialSlots/lods validity (§21.2 remainder) ──

TEST(MC3ValidatorTests, EmptyMaterialSlotEntryFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><materialSlots><tag value=""/></materialSlots></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("materialSlots") != std::string::npos && e.find("empty") != std::string::npos)
            { found = true; break; }
    EXPECT_TRUE(found) << "Expected an empty-materialSlots error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, DuplicateMaterialSlotEntryFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><materialSlots><tag value="wall"/><tag value="wall"/></materialSlots></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("duplicate materialSlots") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected a duplicate-materialSlots error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, ValidMaterialSlotsPass) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><materialSlots><tag value="wall"/><tag value="roof"/></materialSlots></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, SelfReferencingLodEntryFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><lods><lod tier="low" definition="house_01"/></lods></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("dangling lods") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "Expected a dangling-self-referencing-lods error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, LodEntryMissingTierOrDefinitionFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><lods><lod tier="" definition=""/></lods></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found_tier = false, found_def = false;
    for (const auto& e : result.errors) {
        if (e.find("'tier'") != std::string::npos) found_tier = true;
        if (e.find("'definition'") != std::string::npos) found_def = true;
    }
    EXPECT_TRUE(found_tier) << "Expected a missing-tier error";
    EXPECT_TRUE(found_def)  << "Expected a missing-definition error";
}

// A lods entry pointing to ANOTHER real definition present in the SAME
// document resolves fine (the trivial "found" case) -- and a target that
// isn't present in this document at all (the "may be external" case) must
// NOT be flagged, mirroring <instance definition="..."> precedent.
TEST(MC3ValidatorTests, LodEntryResolvingWithinSameDocumentPasses) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><lods><lod tier="low" definition="house_01_lod1"/></lods></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
<definition id="house_01_lod1">
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

TEST(MC3ValidatorTests, LodEntryReferencingExternalDefinitionIsNotAnErrorHere) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<definitions>
<definition id="house_01">
<assetMetadata><lods><lod tier="low" definition="house_01_lod1_externally_injected"/></lods></assetMetadata>
<box id="root" size="1 1 1" material="wall_plaster"/>
</definition>
</definitions>
<objects>
<instance id="i1" definition="house_01" position="1 0 1"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

// ── R130b: terrain-penetration check (§21.3 subset, single-chunk-computable only) ──

TEST(MC3ValidatorTests, ObjectPenetratingTerrainFails) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="sunk" position="5 -3 5" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);

    EXPECT_FALSE(result.ok);
    bool found = false;
    for (const auto& e : result.errors)
        if (e.find("sunk") != std::string::npos && e.find("terrain") != std::string::npos)
            { found = true; break; }
    EXPECT_TRUE(found) << "Expected a terrain-penetration error. Errors: "
                       << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(MC3ValidatorTests, ObjectSlightlyBelowZeroWithinEpsilonPasses) {
    // Harmless float noise from generator math (e.g. -0.001) must not be
    // misread as terrain penetration.
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="5 -0.001 5" size="1 1 1" material="grass_park"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

// RiverBankGenerator.cpp/BridgeGenerator.cpp both deliberately recess their
// "water" plane 0.3-0.4m below the ground plane -- an established,
// intentional real-content convention (a recessed riverbed), not a
// placement bug. Found via DemoWorldTests.AllChunksPassMC3Validator
// failing against real generated content; must not regress.
TEST(MC3ValidatorTests, WaterMaterialBelowGroundIsNotTerrainPenetration) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<plane id="water" position="5 -0.3 5" size="10 10" material="water"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(result.ok);
}

// ── R130b: performance metrics (§21.4) ──

TEST(MC3ValidatorTests, InstanceCountCountsOnlyInstanceElements) {
    const char* xml = R"xml(<?xml version="1.0"?>
<mc3>
<metadata format="json" type="generation">{"generator":{"id":"test.gen"}}</metadata>
<objects>
<box id="b1" position="1 0 1" size="1 1 1" material="grass_park"/>
<instance id="i1" definition="tree_pine" position="2 0 2"/>
<instance id="i2" definition="tree_oak" position="3 0 3"/>
</objects>
</mc3>)xml";

    MeshWorld::MC3Validator validator;
    auto result = validator.validate(xml, 64.0f);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.object_count, 3);
    EXPECT_EQ(result.instance_count, 2);
}

TEST(MC3ValidatorTests, TriangleCountAndDrawCallEstimateArePopulated) {
    MeshWorld::MC3Validator validator;
    auto result = validator.validate(VALID_MC3, 64.0f);
    EXPECT_TRUE(result.ok);
    // VALID_MC3 has exactly one <box> -- Mc3MeshBuilder tessellates a box
    // into 12 triangles (6 faces * 2 triangles), same computation
    // ChunkPipeline::get() already relies on for ChunkDiagnostics.
    EXPECT_EQ(result.triangle_count, 12);
    EXPECT_EQ(result.draw_call_estimate, result.object_count);
}

// R131 — the semantic mc3.json interchange contract deliberately lives next
// to MeshWorld's real mc3lib data. Validate every library against the same
// closed-field Draft 2020-12 schema, plus representative rejection cases.
TEST(MC3JsonSchemaTests, SchemaAcceptsAllTrackedMc3LibrariesAndRejectsMalformedDocuments) {
    const std::filesystem::path project_root = find_project_root_for_mc3_schema();
    const std::filesystem::path schema_path = project_root / "schemas/mc3.schema.json";
    std::ifstream schema_file(schema_path);
    ASSERT_TRUE(schema_file.is_open()) << schema_path;
    const Json schema = Json::parse(schema_file);

    EXPECT_EQ(schema.value("$schema", ""), "https://json-schema.org/draft/2020-12/schema");
    EXPECT_EQ(schema.value("$id", ""),
              "https://openeggbert.github.io/mesh-world/schemas/mc3.schema.json");
    EXPECT_EQ(schema.at("properties").at("format").at("const"), "mc3");
    EXPECT_FALSE(schema.at("additionalProperties").get<bool>());

    const std::filesystem::path library_dir = project_root / "data/mc3lib";
    std::vector<std::filesystem::path> libraries;
    for (const auto& entry : std::filesystem::directory_iterator(library_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            libraries.push_back(entry.path());
    }
    std::sort(libraries.begin(), libraries.end());
    ASSERT_FALSE(libraries.empty());

    Json representative;
    for (const auto& path : libraries) {
        std::ifstream document_file(path);
        ASSERT_TRUE(document_file.is_open()) << path;
        const Json document = Json::parse(document_file);
        std::vector<std::string> errors;
        validate_schema_subset(schema, document, schema, "$", errors);
        EXPECT_TRUE(errors.empty()) << path << join_errors(errors);
        representative = document;
    }

    Json unknown_field = representative;
    unknown_field["notPartOfMc3"] = true;
    std::vector<std::string> errors;
    validate_schema_subset(schema, unknown_field, schema, "$", errors);
    EXPECT_FALSE(errors.empty()) << "closed top-level MC3 contract must reject unknown fields";

    Json bad_format = representative;
    bad_format["format"] = "xml";
    errors.clear();
    validate_schema_subset(schema, bad_format, schema, "$", errors);
    EXPECT_FALSE(errors.empty()) << "mc3.json format discriminator must be exact";

    ASSERT_FALSE(representative.at("definitions").empty());
    Json bad_vector = representative;
    bad_vector["definitions"][0]["object"]["transform"]["position"] = Json::array({0.0, 1.0});
    errors.clear();
    validate_schema_subset(schema, bad_vector, schema, "$", errors);
    EXPECT_FALSE(errors.empty()) << "semantic vectors must retain their fixed arity";
}
