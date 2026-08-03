// SPDX-License-Identifier: MS-PL
/**
 * @file CoreTests.cpp
 * @brief Tests for identity, JSON and the reflection metadata layer.
 */

#include "TestHarness.hpp"

#include <functional>

#include "CNA/Editor/Assets/AssetDatabase.hpp"
#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Core/FormatMigration.hpp"
#include "CNA/Editor/Core/Json.hpp"
#include "CNA/Editor/Core/PropertyValue.hpp"
#include "CNA/Editor/Core/Uuid.hpp"
#include "CNA/Editor/Project/Project.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

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
    // Named rather than positional: a new PropertyDescriptor field would silently shift every
    // value along, and the compiler only catches it when the types happen to disagree.
    PropertyDescriptor health;
    health.name = "health";
    health.displayName = "Health";
    health.type = PropertyType::Integer;
    health.defaultValue = PropertyValue{100};
    descriptor.properties.push_back(std::move(health));

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

// --------------------------------------------------------------------------------------------
// Format migration (plan.md ED-902)
//
// Every real format is at version 1, so these run against synthetic chains. That is the point:
// the mechanism has to be proven before the first real migration is written, not by it.
// --------------------------------------------------------------------------------------------

namespace
{
    /** @brief A document claiming @p version, carrying one renameable field. */
    JsonValue makeVersionedDocument(int version)
    {
        JsonValue document = JsonValue::makeObject();
        document.set("formatVersion", JsonValue{version});
        document.set("oldName", JsonValue{"value"});
        return document;
    }

    /** @brief A step renaming `oldName` to `newName`. */
    std::function<bool(JsonValue&, std::string&)> renameField(std::string from, std::string to)
    {
        return [from = std::move(from), to = std::move(to)](JsonValue& document, std::string& errorMessage) {
            if (!document.contains(from))
            {
                errorMessage = "'" + from + "' is missing";
                return false;
            }
            document.set(to, document[from]);
            document.remove(from);
            return true;
        };
    }
}

CNA_EDITOR_TEST(AMigrationChainUpgradesOneVersionAtATime)
{
    FormatMigrator migrator{"scene", 3};
    CNA_EDITOR_EXPECT(migrator.addMigration(1, "renamed oldName to middleName",
                                            renameField("oldName", "middleName")));
    CNA_EDITOR_EXPECT(migrator.addMigration(2, "renamed middleName to newName",
                                            renameField("middleName", "newName")));
    CNA_EDITOR_EXPECT_EQ(migrator.getMigrationCount(), std::size_t{2});

    JsonValue document = makeVersionedDocument(1);
    const FormatMigrationResult result = migrator.migrate(document);

    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.fromVersion, 1);
    CNA_EDITOR_EXPECT_EQ(result.toVersion, 3);
    CNA_EDITOR_EXPECT_EQ(result.applied.size(), std::size_t{2});
    CNA_EDITOR_EXPECT(result.changedAnything());

    // Both steps ran, in order, and the version was stamped by the migrator rather than by them.
    CNA_EDITOR_EXPECT_EQ(document["newName"].asString(), std::string{"value"});
    CNA_EDITOR_EXPECT(!document.contains("oldName"));
    CNA_EDITOR_EXPECT(!document.contains("middleName"));
    CNA_EDITOR_EXPECT_EQ(document["formatVersion"].asInt(), 3);

    // Running it again is a no-op, not a second rename. Migration has to be idempotent with
    // respect to the version it has already reached, or a re-save would corrupt the file.
    const FormatMigrationResult again = migrator.migrate(document);
    CNA_EDITOR_EXPECT(again.succeeded);
    CNA_EDITOR_EXPECT(!again.changedAnything());
    CNA_EDITOR_EXPECT_EQ(document["newName"].asString(), std::string{"value"});
}

CNA_EDITOR_TEST(AMigratorRefusesWhatItCannotUpgrade)
{
    FormatMigrator migrator{"scene", 3};

    // No step at all: better to refuse than to read a version-1 file with a version-3 reader,
    // which would silently substitute defaults for fields that moved.
    JsonValue old = makeVersionedDocument(1);
    const FormatMigrationResult noStep = migrator.migrate(old);
    CNA_EDITOR_EXPECT(!noStep.succeeded);
    CNA_EDITOR_EXPECT(noStep.errorMessage.find("no migration") != std::string::npos);

    // A gap in the chain stops at the gap rather than skipping it.
    CNA_EDITOR_EXPECT(migrator.addMigration(1, "step one", renameField("oldName", "middleName")));
    JsonValue gapped = makeVersionedDocument(1);
    const FormatMigrationResult gap = migrator.migrate(gapped);
    CNA_EDITOR_EXPECT(!gap.succeeded);
    CNA_EDITOR_EXPECT_EQ(gapped["formatVersion"].asInt(), 2);

    // A step that refuses reports its own reason.
    FormatMigrator refusing{"scene", 2};
    CNA_EDITOR_EXPECT(refusing.addMigration(1, "needs a field that is not there",
                                            renameField("absent", "present")));
    JsonValue missing = makeVersionedDocument(1);
    const FormatMigrationResult refused = refusing.migrate(missing);
    CNA_EDITOR_EXPECT(!refused.succeeded);
    CNA_EDITOR_EXPECT(refused.errorMessage.find("'absent' is missing") != std::string::npos);

    // A file from the future is refused by the same code that upgrades one from the past: both
    // answer "what version is this?", and splitting them is how a loader refuses what it could read.
    JsonValue future = makeVersionedDocument(9);
    const FormatMigrationResult ahead = migrator.migrate(future);
    CNA_EDITOR_EXPECT(!ahead.succeeded);
    CNA_EDITOR_EXPECT(ahead.errorMessage.find("newer than this build supports") != std::string::npos);

    // As is one with no version at all.
    JsonValue unversioned = JsonValue::makeObject();
    CNA_EDITOR_EXPECT(!migrator.migrate(unversioned).succeeded);
    JsonValue notAnObject{"not an object"};
    CNA_EDITOR_EXPECT(!migrator.migrate(notAnObject).succeeded);
}

CNA_EDITOR_TEST(AMigratorRefusesAnUnusableStep)
{
    FormatMigrator migrator{"scene", 3};

    CNA_EDITOR_EXPECT(migrator.addMigration(1, "first", renameField("a", "b")));

    // Two steps reading the same version would make the outcome depend on registration order.
    CNA_EDITOR_EXPECT(!migrator.addMigration(1, "duplicate", renameField("a", "c")));

    // A step that upgrades to or past the current version has nowhere to go.
    CNA_EDITOR_EXPECT(!migrator.addMigration(3, "at the top", renameField("a", "b")));
    CNA_EDITOR_EXPECT(!migrator.addMigration(0, "below the first version", renameField("a", "b")));
    CNA_EDITOR_EXPECT(!migrator.addMigration(2, "no function", {}));

    CNA_EDITOR_EXPECT_EQ(migrator.getMigrationCount(), std::size_t{1});
}

CNA_EDITOR_TEST(TheRealFormatsRunTheirChainsOnEveryLoad)
{
    // Empty today, and that is the intended state: the mechanism exists so that the first real
    // migration is a small tested addition rather than an emergency.
    CNA_EDITOR_EXPECT_EQ(getSceneFormatMigrator().getMigrationCount(), std::size_t{0});
    CNA_EDITOR_EXPECT_EQ(getProjectFormatMigrator().getMigrationCount(), std::size_t{0});
    CNA_EDITOR_EXPECT_EQ(getAssetFormatMigrator().getMigrationCount(), std::size_t{0});

    CNA_EDITOR_EXPECT_EQ(getSceneFormatMigrator().getCurrentVersion(), SceneDocument::kFormatVersion);
    CNA_EDITOR_EXPECT_EQ(getProjectFormatMigrator().getCurrentVersion(), Project::kFormatVersion);
    CNA_EDITOR_EXPECT_EQ(getAssetFormatMigrator().getCurrentVersion(), AssetDatabase::kFormatVersion);
}

CNA_EDITOR_TEST(TheSceneLoaderReadsTheUpgradedDocumentNotTheOriginal)
{
    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    // A synthetic version 1 whose entity list moved: proof that the loader runs the chain and then
    // reads what came out of it, rather than running it and reading the original anyway.
    FormatMigrator migrator{"scene", 2};
    CNA_EDITOR_EXPECT(migrator.addMigration(1, "moved 'objects' to 'entities'",
                                            renameField("objects", "entities")));

    JsonValue entity = JsonValue::makeObject();
    entity.set("id", JsonValue{Uuid::generate().toString()});
    entity.set("name", JsonValue{"Player"});
    entity.set("components", JsonValue::makeObject());

    JsonValue objects = JsonValue::makeArray();
    objects.append(std::move(entity));

    JsonValue document = JsonValue::makeObject();
    document.set("formatVersion", JsonValue{1});
    document.set("sceneId", JsonValue{Uuid::generate().toString()});
    document.set("name", JsonValue{"Level01"});
    document.set("objects", std::move(objects));

    SceneDocument scene;
    const SceneLoadResult result = scene.loadFromJson(document, registry, &migrator);

    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(scene.getEntities().front().getName(), std::string{"Player"});

    // The upgrade is reported rather than performed silently, and the caller's document is left
    // exactly as it was -- a loader that edited its input would surprise anyone reusing it.
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(result.warnings.front().find("moved 'objects' to 'entities'") != std::string::npos);
    CNA_EDITOR_EXPECT(document.contains("objects"));
    CNA_EDITOR_EXPECT_EQ(document["formatVersion"].asInt(), 1);
}

CNA_EDITOR_TEST(TheProjectLoaderRunsItsChainToo)
{
    FormatMigrator migrator{"project", 2};
    CNA_EDITOR_EXPECT(migrator.addMigration(1, "renamed 'title' to 'name'", renameField("title", "name")));

    JsonValue document = JsonValue::makeObject();
    document.set("formatVersion", JsonValue{1});
    document.set("title", JsonValue{"Upgraded"});
    document.set("kind", JsonValue{"CnaNative"});

    Project project;
    const ProjectLoadResult result = project.loadFromJson(document, &migrator);

    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(project.getName(), std::string{"Upgraded"});
    CNA_EDITOR_EXPECT(!result.warnings.empty());
}

// --------------------------------------------------------------------------------------------
// List properties (plan.md ED-311)
// --------------------------------------------------------------------------------------------

CNA_EDITOR_TEST(AListRoundTripsThroughJsonWithItsDeclaredElementType)
{
    PropertyValue::ListValue tags;
    tags.items.emplace_back(std::string{"ground"});
    tags.items.emplace_back(std::string{"solid"});

    const PropertyValue value{tags};
    CNA_EDITOR_EXPECT(value.getType() == PropertyType::List);
    CNA_EDITOR_EXPECT_EQ(value.toDisplayString(), std::string{"2 items"});

    // A plain array with no per-element type tag: the descriptor is the one source of truth for
    // what the elements are.
    const JsonValue json = value.toJson();
    CNA_EDITOR_EXPECT(json.isArray());
    CNA_EDITOR_EXPECT_EQ(json.getElements().size(), std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(json.getElements().front().asString(), std::string{"ground"});

    const PropertyValue read = PropertyValue::fromJson(json, PropertyType::List, PropertyType::String);
    CNA_EDITOR_EXPECT(read == value);

    // A list of vectors is a list of arrays, and nests exactly one level -- which is all it has to.
    PropertyValue::ListValue points;
    points.items.emplace_back(EditorVector2{1.0f, 2.0f});
    points.items.emplace_back(EditorVector2{3.0f, 4.0f});
    const PropertyValue vectors{points};
    CNA_EDITOR_EXPECT(PropertyValue::fromJson(vectors.toJson(), PropertyType::List,
                                              PropertyType::Vector2) == vectors);

    // One item reads as "1 item", because "1 items" is the kind of detail that makes a UI look
    // machine-written.
    PropertyValue::ListValue single;
    single.items.emplace_back(std::int64_t{7});
    CNA_EDITOR_EXPECT_EQ(PropertyValue{single}.toDisplayString(), std::string{"1 item"});
}

CNA_EDITOR_TEST(AListWithNoDeclaredElementTypeReadsBackEmpty)
{
    JsonValue json = JsonValue::makeArray();
    json.append(JsonValue{"ground"});
    json.append(JsonValue{"solid"});

    // Guessing would produce a list the inspector cannot edit and the next save would write out in
    // a shape nothing declared. Empty is the honest answer.
    const PropertyValue read = PropertyValue::fromJson(json, PropertyType::List);
    CNA_EDITOR_EXPECT(read.getType() == PropertyType::List);
    CNA_EDITOR_EXPECT(read.get<PropertyValue::ListValue>().items.empty());

    // Nor do lists nest: a list of lists is a table and deserves its own type.
    CNA_EDITOR_EXPECT(PropertyValue::fromJson(json, PropertyType::List, PropertyType::List)
                          .get<PropertyValue::ListValue>()
                          .items.empty());
}

CNA_EDITOR_TEST(TheListTypeNameIsAppendedRatherThanInserted)
{
    // toString(PropertyType) is on the editor-to-player wire, so every existing name has to stay
    // exactly where it was. This asserts the whole table, which is the only way to notice an
    // insertion in the middle.
    CNA_EDITOR_EXPECT_EQ(std::string{toString(PropertyType::List)}, std::string{"list"});
    CNA_EDITOR_EXPECT(parsePropertyType("list") == PropertyType::List);

    CNA_EDITOR_EXPECT_EQ(std::string{toString(PropertyType::Boolean)}, std::string{"bool"});
    CNA_EDITOR_EXPECT_EQ(std::string{toString(PropertyType::Vector3)}, std::string{"vector3"});
    CNA_EDITOR_EXPECT_EQ(std::string{toString(PropertyType::AssetReference)}, std::string{"asset"});
    CNA_EDITOR_EXPECT_EQ(std::string{toString(PropertyType::EntityReference)}, std::string{"entity"});
    CNA_EDITOR_EXPECT(parsePropertyType("vector3") == PropertyType::Vector3);
    CNA_EDITOR_EXPECT(parsePropertyType("nonsense") == PropertyType::None);
}

CNA_EDITOR_TEST(ASceneRoundTripsAListAndDoesNotLoseAnUnregisteredOne)
{
    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    ComponentDescriptor tagged;
    tagged.typeId = "Game.Tagged";
    tagged.displayName = "Tagged";
    PropertyDescriptor tags;
    tags.name = "tags";
    tags.type = PropertyType::List;
    tags.elementType = PropertyType::String;
    tags.defaultValue = PropertyValue{PropertyValue::ListValue{}};
    tagged.properties.push_back(std::move(tags));
    CNA_EDITOR_EXPECT(registry.registerComponent(tagged));

    PropertyValue::ListValue list;
    list.items.emplace_back(std::string{"ground"});
    list.items.emplace_back(std::string{"solid"});

    SceneDocument scene;
    EditorEntity entity{Uuid::generate(), "Tile"};
    EditorComponent component{"Game.Tagged"};
    component.setProperty("tags", PropertyValue{list});
    entity.addComponent(std::move(component));
    scene.addEntity(std::move(entity));

    SceneDocument reloaded;
    CNA_EDITOR_EXPECT(reloaded.loadFromJson(scene.toJson(), registry).succeeded);

    const EditorComponent* readBack = reloaded.getEntities().front().findComponent("Game.Tagged");
    CNA_EDITOR_EXPECT(readBack != nullptr);
    CNA_EDITOR_EXPECT(readBack->getProperty("tags") == PropertyValue{list});

    // The same scene opened by a build whose plugin is missing must save back byte for byte.
    // Before lists existed this array became the empty string and the field was silently lost.
    ComponentRegistry withoutPlugin;
    registerBuiltinComponents(withoutPlugin);

    SceneDocument blind;
    const SceneLoadResult blindLoad = blind.loadFromJson(scene.toJson(), withoutPlugin);
    CNA_EDITOR_EXPECT(blindLoad.succeeded);
    CNA_EDITOR_EXPECT(!blindLoad.warnings.empty());
    CNA_EDITOR_EXPECT_EQ(Json::write(blind.toJson()), Json::write(scene.toJson()));
}
