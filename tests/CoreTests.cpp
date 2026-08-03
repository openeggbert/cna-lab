// SPDX-License-Identifier: MS-PL
/**
 * @file CoreTests.cpp
 * @brief Tests for identity, JSON and the reflection metadata layer.
 */

#include "TestHarness.hpp"

#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Core/Json.hpp"
#include "CNA/Editor/Core/PropertyValue.hpp"
#include "CNA/Editor/Core/Uuid.hpp"

using namespace CNA::Editor;

CNA_EDITOR_TEST(UuidGeneratesDistinctValidValues)
{
    const Uuid first = Uuid::generate();
    const Uuid second = Uuid::generate();

    CNA_EDITOR_EXPECT(first.isValid());
    CNA_EDITOR_EXPECT(second.isValid());
    CNA_EDITOR_EXPECT(first != second);
    CNA_EDITOR_EXPECT_EQ(first.toString().size(), std::size_t{36});
}

CNA_EDITOR_TEST(UuidRoundTripsThroughItsTextualForm)
{
    const Uuid original = Uuid::generate();
    CNA_EDITOR_EXPECT(Uuid::parse(original.toString()) == original);
    CNA_EDITOR_EXPECT(Uuid::parse("{" + original.toString() + "}") == original);
}

CNA_EDITOR_TEST(UuidRejectsMalformedText)
{
    CNA_EDITOR_EXPECT(!Uuid::parse("").isValid());
    CNA_EDITOR_EXPECT(!Uuid::parse("not-a-uuid").isValid());
    // One hex digit short: must not silently succeed with a zero-padded value.
    CNA_EDITOR_EXPECT(!Uuid::parse("f392aaaa-bbbb-cccc-dddd-eeeeeeeeeee").isValid());
}

CNA_EDITOR_TEST(UuidDefaultIsNilAndInvalid)
{
    const Uuid nil;
    CNA_EDITOR_EXPECT(!nil.isValid());
    CNA_EDITOR_EXPECT_EQ(nil.toString(), std::string{"00000000-0000-0000-0000-000000000000"});
}

CNA_EDITOR_TEST(JsonParsesAndPreservesMemberOrder)
{
    const JsonParseResult parsed = Json::parse(R"({"z": 1, "a": 2, "m": [true, null, "x"]})");
    CNA_EDITOR_EXPECT(parsed.succeeded);

    const auto& members = parsed.value.getMembers();
    CNA_EDITOR_EXPECT_EQ(members.size(), std::size_t{3});
    CNA_EDITOR_EXPECT_EQ(members[0].first, std::string{"z"});
    CNA_EDITOR_EXPECT_EQ(members[1].first, std::string{"a"});
    CNA_EDITOR_EXPECT_EQ(members[2].first, std::string{"m"});
    CNA_EDITOR_EXPECT_EQ(parsed.value["m"].getElements().size(), std::size_t{3});
}

CNA_EDITOR_TEST(JsonAcceptsCommentsAndTrailingCommas)
{
    const JsonParseResult parsed = Json::parse(R"({
        // a hand-written project file
        "name": "MyGame",
        "modules": ["cna-core", "cna-audio",],
    })");
    CNA_EDITOR_EXPECT(parsed.succeeded);
    CNA_EDITOR_EXPECT_EQ(parsed.value["name"].asString(), std::string{"MyGame"});
    CNA_EDITOR_EXPECT_EQ(parsed.value["modules"].getElements().size(), std::size_t{2});
}

CNA_EDITOR_TEST(JsonReportsFailureRatherThanThrowing)
{
    const JsonParseResult parsed = Json::parse(R"({"unterminated": )");
    CNA_EDITOR_EXPECT(!parsed.succeeded);
    CNA_EDITOR_EXPECT(!parsed.errorMessage.empty());
}

CNA_EDITOR_TEST(JsonAccessorsFallBackInsteadOfThrowing)
{
    const JsonParseResult parsed = Json::parse(R"({"count": 7})");
    CNA_EDITOR_EXPECT(parsed.succeeded);

    // Asking for the wrong alternative must degrade, never throw: a scene with one bad field has
    // to load with that field defaulted.
    CNA_EDITOR_EXPECT_EQ(parsed.value["count"].asString("fallback"), std::string{"fallback"});
    CNA_EDITOR_EXPECT_EQ(parsed.value["missing"].asInt(42), 42);
    CNA_EDITOR_EXPECT(parsed.value["missing"].isNull());
}

CNA_EDITOR_TEST(JsonRoundTripsThroughWriteAndParse)
{
    JsonValue original = JsonValue::makeObject();
    original.set("name", JsonValue{"Level\t01 \"quoted\""});
    original.set("count", JsonValue{3});
    original.set("ratio", JsonValue{0.5});
    original.set("flag", JsonValue{true});

    JsonValue nested = JsonValue::makeArray();
    nested.append(JsonValue{1});
    nested.append(JsonValue{2});
    original.set("values", std::move(nested));

    const JsonParseResult parsed = Json::parse(Json::write(original, true));
    CNA_EDITOR_EXPECT(parsed.succeeded);
    CNA_EDITOR_EXPECT_EQ(parsed.value["name"].asString(), std::string{"Level\t01 \"quoted\""});
    CNA_EDITOR_EXPECT_EQ(parsed.value["count"].asInt(), 3);
    CNA_EDITOR_EXPECT_EQ(parsed.value["ratio"].asNumber(), 0.5);
    CNA_EDITOR_EXPECT(parsed.value["flag"].asBoolean());
    CNA_EDITOR_EXPECT_EQ(parsed.value["values"].getElements().size(), std::size_t{2});
}

CNA_EDITOR_TEST(JsonWritesIntegersWithoutDecimalPoint)
{
    JsonValue json = JsonValue::makeObject();
    json.set("count", JsonValue{120});
    const std::string text = Json::write(json, false);
    CNA_EDITOR_EXPECT(text.find("120") != std::string::npos);
    CNA_EDITOR_EXPECT(text.find("120.0") == std::string::npos);
}

CNA_EDITOR_TEST(PropertyValueRoundTripsEveryType)
{
    const std::vector<PropertyValue> values{
        PropertyValue{true},
        PropertyValue{static_cast<std::int64_t>(-17)},
        PropertyValue{2.5f},
        PropertyValue{std::string{"hello"}},
        PropertyValue{PropertyValue::EnumValue{"FlipHorizontally"}},
        PropertyValue{EditorColor{1, 2, 3, 4}},
        PropertyValue{EditorVector2{1.0f, 2.0f}},
        PropertyValue{EditorVector3{1.0f, 2.0f, 3.0f}},
        PropertyValue{EditorVector4{1.0f, 2.0f, 3.0f, 4.0f}},
        PropertyValue{EditorQuaternion{0.0f, 0.0f, 0.7071f, 0.7071f}},
        PropertyValue{EditorRectangle{1, 2, 3, 4}},
        PropertyValue{PropertyValue::AssetReference{Uuid::generate()}},
        PropertyValue{PropertyValue::EntityReference{Uuid::generate()}},
    };

    for (const PropertyValue& value : values)
    {
        const PropertyValue restored = PropertyValue::fromJson(value.toJson(), value.getType());
        CNA_EDITOR_EXPECT_EQ(restored.toDisplayString(), value.toDisplayString());
        CNA_EDITOR_EXPECT(restored == value);
    }
}

CNA_EDITOR_TEST(PropertyValueTypeNamesRoundTrip)
{
    for (int index = 0; index <= static_cast<int>(PropertyType::EntityReference); ++index)
    {
        const auto type = static_cast<PropertyType>(index);
        CNA_EDITOR_EXPECT(parsePropertyType(toString(type)) == type);
    }
}

CNA_EDITOR_TEST(PropertyValueAbsentQuaternionDefaultsToIdentity)
{
    // An all-zero quaternion is not a rotation; a missing field must give identity instead.
    const PropertyValue restored = PropertyValue::fromJson(JsonValue{}, PropertyType::Quaternion);
    const EditorQuaternion rotation = restored.get<EditorQuaternion>();
    CNA_EDITOR_EXPECT_EQ(rotation.w, 1.0f);
    CNA_EDITOR_EXPECT_EQ(rotation.x, 0.0f);
}

CNA_EDITOR_TEST(PropertyValueGetReturnsFallbackOnTypeMismatch)
{
    const PropertyValue value{std::string{"text"}};
    CNA_EDITOR_EXPECT_EQ(value.get<float>(9.0f), 9.0f);
    CNA_EDITOR_EXPECT_EQ(value.get<std::string>(), std::string{"text"});
}

CNA_EDITOR_TEST(ComponentRegistryRegistersFindsAndReplaces)
{
    ComponentRegistry registry;

    ComponentDescriptor descriptor;
    descriptor.typeId = "Game.PlayerSpawn";
    descriptor.displayName = "Player Spawn";
    descriptor.properties.push_back(
        PropertyDescriptor{"health", "Health", PropertyType::Integer, PropertyValue{100}, {}, {}, 0.0, 0.0, false});

    CNA_EDITOR_EXPECT(registry.registerComponent(descriptor));
    CNA_EDITOR_EXPECT(registry.contains("Game.PlayerSpawn"));
    CNA_EDITOR_EXPECT_EQ(registry.getCount(), std::size_t{1});

    const ComponentDescriptor* found = registry.find("Game.PlayerSpawn");
    CNA_EDITOR_EXPECT(found != nullptr);
    CNA_EDITOR_EXPECT(found->findProperty("health") != nullptr);
    CNA_EDITOR_EXPECT(found->findProperty("mana") == nullptr);

    // Re-registering replaces rather than duplicating; this is what makes plugin reload possible.
    descriptor.displayName = "Player Start";
    CNA_EDITOR_EXPECT(registry.registerComponent(descriptor));
    CNA_EDITOR_EXPECT_EQ(registry.getCount(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(registry.find("Game.PlayerSpawn")->displayName, std::string{"Player Start"});

    CNA_EDITOR_EXPECT(registry.unregisterComponent("Game.PlayerSpawn"));
    CNA_EDITOR_EXPECT(!registry.contains("Game.PlayerSpawn"));
}

CNA_EDITOR_TEST(ComponentRegistryRejectsEmptyTypeId)
{
    ComponentRegistry registry;
    CNA_EDITOR_EXPECT(!registry.registerComponent(ComponentDescriptor{}));
    CNA_EDITOR_EXPECT_EQ(registry.getCount(), std::size_t{0});
}
