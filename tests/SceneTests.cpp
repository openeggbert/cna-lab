// SPDX-License-Identifier: MS-PL
/**
 * @file SceneTests.cpp
 * @brief Tests for the scene document, its invariants, its serialisation and its undo behaviour.
 */

#include "TestHarness.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"
#include "CNA/Editor/Scene/PrefabCommands.hpp"
#include "CNA/Editor/Scene/PrefabDocument.hpp"
#include "CNA/Editor/Scene/SceneValidation.hpp"
#include "CNA/Editor/Project/Project.hpp"

using namespace CNA::Editor;

namespace
{
    /** @brief Returns a registry holding the built-in component descriptors. */
    ComponentRegistry makeRegistry()
    {
        ComponentRegistry registry;
        registerBuiltinComponents(registry);
        return registry;
    }

    /** @brief Builds an entity with a Transform positioned at (@p x, @p y, 0). */
    EditorEntity makeEntity(const ComponentRegistry& registry, std::string name, float x, float y)
    {
        EditorEntity entity{Uuid::generate(), std::move(name)};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        transform.applyDefaults(*registry.find(BuiltinComponentIds::kTransform));
        transform.setProperty("position", PropertyValue{EditorVector3{x, y, 0.0f}});
        entity.addComponent(std::move(transform));
        return entity;
    }

    /** @brief Returns a unique scratch directory for a test that touches the filesystem. */
    std::filesystem::path makeScratchDirectory(const std::string& name)
    {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() / ("cna-editor-tests-" + name + "-" + Uuid::generate().toString());
        std::filesystem::create_directories(directory);
        return directory;
    }
}

CNA_EDITOR_TEST(BuiltinComponentsAreRegistered)
{
    const ComponentRegistry registry = makeRegistry();
    CNA_EDITOR_EXPECT(registry.contains(BuiltinComponentIds::kTransform));
    CNA_EDITOR_EXPECT(registry.contains(BuiltinComponentIds::kSpriteRenderer));
    CNA_EDITOR_EXPECT(registry.contains(BuiltinComponentIds::kCamera));
    CNA_EDITOR_EXPECT(registry.contains(BuiltinComponentIds::kAudioSource));

    const ComponentDescriptor* transform = registry.find(BuiltinComponentIds::kTransform);
    CNA_EDITOR_EXPECT(transform->required);
    CNA_EDITOR_EXPECT(transform->unique);

    // Scale must default to 1, not 0: a zero-scaled entity is invisible, and "my sprite does not
    // appear" is the least debuggable possible first experience.
    const PropertyDescriptor* scale = transform->findProperty("scale");
    CNA_EDITOR_EXPECT(scale != nullptr);
    CNA_EDITOR_EXPECT_EQ(scale->defaultValue.get<EditorVector3>().x, 1.0f);
}

CNA_EDITOR_TEST(SceneAddsAndFindsEntities)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid id = scene.addEntity(makeEntity(registry, "Player", 10.0f, 20.0f));
    CNA_EDITOR_EXPECT(id.isValid());
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{1});

    const EditorEntity* entity = scene.findEntity(id);
    CNA_EDITOR_EXPECT(entity != nullptr);
    CNA_EDITOR_EXPECT_EQ(entity->getName(), std::string{"Player"});
    CNA_EDITOR_EXPECT(scene.findEntity(Uuid::generate()) == nullptr);
}

CNA_EDITOR_TEST(SceneRejectsDuplicateEntityIds)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Player", 0.0f, 0.0f);
    const Uuid id = scene.addEntity(entity);
    CNA_EDITOR_EXPECT(id.isValid());
    CNA_EDITOR_EXPECT(!scene.addEntity(entity).isValid());
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{1});
}

CNA_EDITOR_TEST(SceneMaintainsHierarchy)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = scene.addEntity(makeEntity(registry, "Parent", 0.0f, 0.0f));
    const Uuid child = scene.addEntity(makeEntity(registry, "Child", 0.0f, 0.0f));

    CNA_EDITOR_EXPECT_EQ(scene.getRootEntities().size(), std::size_t{2});
    CNA_EDITOR_EXPECT(scene.reparentEntity(child, parent));
    CNA_EDITOR_EXPECT_EQ(scene.getRootEntities().size(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(scene.getChildren(parent).size(), std::size_t{1});
    CNA_EDITOR_EXPECT(scene.isAncestorOf(parent, child));
    CNA_EDITOR_EXPECT(!scene.isAncestorOf(child, parent));
}

CNA_EDITOR_TEST(SceneRejectsParentCycles)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid a = scene.addEntity(makeEntity(registry, "A", 0.0f, 0.0f));
    const Uuid b = scene.addEntity(makeEntity(registry, "B", 0.0f, 0.0f));
    const Uuid c = scene.addEntity(makeEntity(registry, "C", 0.0f, 0.0f));

    CNA_EDITOR_EXPECT(scene.reparentEntity(b, a));
    CNA_EDITOR_EXPECT(scene.reparentEntity(c, b));

    // A graph with a cycle has no roots and makes every hierarchy walk infinite, so these must
    // be refused outright rather than detected later.
    CNA_EDITOR_EXPECT(!scene.reparentEntity(a, c));
    CNA_EDITOR_EXPECT(!scene.reparentEntity(a, a));
    CNA_EDITOR_EXPECT_EQ(scene.getRootEntities().size(), std::size_t{1});
}

CNA_EDITOR_TEST(SceneDeletesSubtreesRecursively)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid root = scene.addEntity(makeEntity(registry, "Root", 0.0f, 0.0f));
    const Uuid middle = scene.addEntity(makeEntity(registry, "Middle", 0.0f, 0.0f));
    const Uuid leaf = scene.addEntity(makeEntity(registry, "Leaf", 0.0f, 0.0f));
    scene.reparentEntity(middle, root);
    scene.reparentEntity(leaf, middle);

    const std::vector<EditorEntity> removed = scene.removeEntityRecursive(root);
    CNA_EDITOR_EXPECT_EQ(removed.size(), std::size_t{3});
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{0});

    // Parents first, so an undo can re-add without ever pointing at a missing parent.
    CNA_EDITOR_EXPECT(removed.front().getId() == root);
}

CNA_EDITOR_TEST(SceneRoundTripsThroughJson)
{
    const ComponentRegistry registry = makeRegistry();

    SceneDocument original;
    original.setName("Level01");
    const Uuid parent = original.addEntity(makeEntity(registry, "Parent", 100.0f, 220.0f));

    EditorEntity sprite = makeEntity(registry, "Player", 5.0f, 6.0f);
    EditorComponent renderer{BuiltinComponentIds::kSpriteRenderer};
    renderer.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
    const Uuid textureId = Uuid::generate();
    renderer.setProperty("texture", PropertyValue{PropertyValue::AssetReference{textureId}});
    renderer.setProperty("tint", PropertyValue{EditorColor{255, 0, 0, 128}});
    renderer.setProperty("layerDepth", PropertyValue{0.25f});
    sprite.addComponent(std::move(renderer));
    sprite.setEditorState("expanded", PropertyValue{true});
    const Uuid spriteId = original.addEntity(std::move(sprite));
    original.reparentEntity(spriteId, parent);

    SceneDocument restored;
    const SceneLoadResult result = restored.loadFromJson(original.toJson(), registry);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{0});
    CNA_EDITOR_EXPECT_EQ(restored.getName(), std::string{"Level01"});
    CNA_EDITOR_EXPECT(restored.getSceneId() == original.getSceneId());
    CNA_EDITOR_EXPECT_EQ(restored.getEntityCount(), std::size_t{2});

    const EditorEntity* restoredSprite = restored.findEntity(spriteId);
    CNA_EDITOR_EXPECT(restoredSprite != nullptr);
    CNA_EDITOR_EXPECT(restoredSprite->getParentId() == parent);

    const EditorComponent* restoredRenderer = restoredSprite->findComponent(BuiltinComponentIds::kSpriteRenderer);
    CNA_EDITOR_EXPECT(restoredRenderer != nullptr);
    CNA_EDITOR_EXPECT(restoredRenderer->getProperty("texture").get<PropertyValue::AssetReference>().id == textureId);
    CNA_EDITOR_EXPECT(restoredRenderer->getProperty("tint").get<EditorColor>() == (EditorColor{255, 0, 0, 128}));
    CNA_EDITOR_EXPECT_EQ(restoredRenderer->getProperty("layerDepth").get<float>(), 0.25f);
    CNA_EDITOR_EXPECT(restoredSprite->getEditorState().count("expanded") > 0);
}

CNA_EDITOR_TEST(SceneRoundTripsThroughAFile)
{
    const ComponentRegistry registry = makeRegistry();
    const std::filesystem::path directory = makeScratchDirectory("scene");
    const std::string path = (directory / "Level01.cnascene").generic_string();

    SceneDocument original;
    original.setName("Level01");
    const Uuid id = original.addEntity(makeEntity(registry, "Player", 1.0f, 2.0f));

    std::string errorMessage;
    CNA_EDITOR_EXPECT(original.saveToFile(path, &errorMessage));
    CNA_EDITOR_EXPECT_EQ(errorMessage, std::string{});

    SceneDocument restored;
    const SceneLoadResult result = restored.loadFromFile(path, registry);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT(restored.findEntity(id) != nullptr);

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(SceneRejectsAFutureFormatVersion)
{
    const ComponentRegistry registry = makeRegistry();

    JsonValue json = JsonValue::makeObject();
    json.set("formatVersion", JsonValue{SceneDocument::kFormatVersion + 1});
    json.set("name", JsonValue{"FromTheFuture"});

    SceneDocument scene;
    const SceneLoadResult result = scene.loadFromJson(json, registry);
    CNA_EDITOR_EXPECT(!result.succeeded);
    CNA_EDITOR_EXPECT(result.errorMessage.find("newer") != std::string::npos);
}

CNA_EDITOR_TEST(ScenePreservesUnknownComponentTypes)
{
    // A scene using a component from a plugin that failed to load must still round-trip. Dropping
    // the data would turn "the plugin is missing" into "the plugin's data is gone".
    ComponentRegistry registry = makeRegistry();

    JsonValue componentJson = JsonValue::makeObject();
    componentJson.set("spawnCount", JsonValue{7});
    componentJson.set("label", JsonValue{"wave-1"});

    JsonValue componentsJson = JsonValue::makeObject();
    componentsJson.set("Mc3.SpawnPoint", std::move(componentJson));

    JsonValue entityJson = JsonValue::makeObject();
    entityJson.set("id", JsonValue{Uuid::generate().toString()});
    entityJson.set("name", JsonValue{"Spawner"});
    entityJson.set("components", std::move(componentsJson));

    JsonValue entitiesJson = JsonValue::makeArray();
    entitiesJson.append(std::move(entityJson));

    JsonValue sceneJson = JsonValue::makeObject();
    sceneJson.set("formatVersion", JsonValue{SceneDocument::kFormatVersion});
    sceneJson.set("sceneId", JsonValue{Uuid::generate().toString()});
    sceneJson.set("name", JsonValue{"WithPlugin"});
    sceneJson.set("entities", std::move(entitiesJson));

    SceneDocument scene;
    const SceneLoadResult result = scene.loadFromJson(sceneJson, registry);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});

    const EditorEntity& entity = scene.getEntities().front();
    const EditorComponent* component = entity.findComponent("Mc3.SpawnPoint");
    CNA_EDITOR_EXPECT(component != nullptr);
    CNA_EDITOR_EXPECT_EQ(component->getProperty("label").get<std::string>(), std::string{"wave-1"});

    // And it must survive a save/load cycle unchanged.
    SceneDocument reloaded;
    CNA_EDITOR_EXPECT(reloaded.loadFromJson(scene.toJson(), registry).succeeded);
    CNA_EDITOR_EXPECT(reloaded.getEntities().front().findComponent("Mc3.SpawnPoint") != nullptr);
}

CNA_EDITOR_TEST(SceneRepairsDanglingParentReferences)
{
    ComponentRegistry registry = makeRegistry();

    JsonValue entityJson = JsonValue::makeObject();
    entityJson.set("id", JsonValue{Uuid::generate().toString()});
    entityJson.set("name", JsonValue{"Orphan"});
    entityJson.set("parent", JsonValue{Uuid::generate().toString()});
    entityJson.set("components", JsonValue::makeObject());

    JsonValue entitiesJson = JsonValue::makeArray();
    entitiesJson.append(std::move(entityJson));

    JsonValue sceneJson = JsonValue::makeObject();
    sceneJson.set("formatVersion", JsonValue{SceneDocument::kFormatVersion});
    sceneJson.set("sceneId", JsonValue{Uuid::generate().toString()});
    sceneJson.set("entities", std::move(entitiesJson));

    SceneDocument scene;
    const SceneLoadResult result = scene.loadFromJson(sceneJson, registry);

    // A scene broken by a bad merge is exactly the scene a user needs the editor to open.
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(scene.getRootEntities().size(), std::size_t{1});
}

// --------------------------------------------------------------------------------------------
// Scene validation (plan.md ED-310)
//
// Every rule describes a state the editor allows the user to reach, so each test asserts both
// halves: that the offending scene is reported, and that the nearest legitimate scene is not.
// A validator that cries wolf is turned off, and then it catches nothing at all.
// --------------------------------------------------------------------------------------------

namespace
{
    /** @brief Adds a component of @p typeId with its declared defaults filled in. */
    EditorComponent& addComponentWithDefaults(EditorEntity& entity,
                                              const ComponentRegistry& registry,
                                              const std::string& typeId)
    {
        EditorComponent component{typeId};
        if (const ComponentDescriptor* descriptor = registry.find(typeId))
        {
            component.applyDefaults(*descriptor);
        }
        return entity.addComponent(std::move(component));
    }

    /** @brief Returns how many issues carry @p ruleId. */
    std::size_t countRule(const std::vector<SceneIssue>& issues, const std::string& ruleId)
    {
        return static_cast<std::size_t>(
            std::count_if(issues.begin(), issues.end(),
                          [&](const SceneIssue& issue) { return issue.ruleId == ruleId; }));
    }
}

CNA_EDITOR_TEST(AnEmptySceneReportsNothing)
{
    const ComponentRegistry registry = makeRegistry();
    const SceneDocument scene;

    CNA_EDITOR_EXPECT(validateScene(scene, registry).empty());
}

CNA_EDITOR_TEST(TwoPrimaryCamerasAreAnError)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity first = makeEntity(registry, "Main Camera", 0.0f, 0.0f);
    addComponentWithDefaults(first, registry, BuiltinComponentIds::kCamera);
    scene.addEntity(std::move(first));

    EditorEntity second = makeEntity(registry, "Cutscene Camera", 0.0f, 0.0f);
    addComponentWithDefaults(second, registry, BuiltinComponentIds::kCamera);
    scene.addEntity(std::move(second));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);

    // One issue per offending camera, so that either row selects a real entity.
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "duplicate-primary-camera"), std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(countIssues(issues, SceneIssue::Severity::Error), std::size_t{2});

    // And the ordinary case -- one camera, marked primary -- says nothing at all.
    SceneDocument single;
    EditorEntity only = makeEntity(registry, "Main Camera", 0.0f, 0.0f);
    addComponentWithDefaults(only, registry, BuiltinComponentIds::kCamera);
    single.addEntity(std::move(only));

    CNA_EDITOR_EXPECT(validateScene(single, registry).empty());
}

CNA_EDITOR_TEST(SwitchingACameraOffResolvesThePrimaryConflict)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity first = makeEntity(registry, "Main Camera", 0.0f, 0.0f);
    addComponentWithDefaults(first, registry, BuiltinComponentIds::kCamera);
    scene.addEntity(std::move(first));

    EditorEntity second = makeEntity(registry, "Cutscene Camera", 0.0f, 0.0f);
    addComponentWithDefaults(second, registry, BuiltinComponentIds::kCamera);
    // Switching the entity off is how a person swaps cameras, so it has to count as a resolution.
    second.setEnabled(false);
    scene.addEntity(std::move(second));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "duplicate-primary-camera"),
                         std::size_t{0});
}

CNA_EDITOR_TEST(ACameraUnderADisabledParentDoesNotCompete)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity primary = makeEntity(registry, "Main Camera", 0.0f, 0.0f);
    addComponentWithDefaults(primary, registry, BuiltinComponentIds::kCamera);
    scene.addEntity(std::move(primary));

    EditorEntity group = makeEntity(registry, "Cutscene", 0.0f, 0.0f);
    group.setEnabled(false);
    const Uuid groupId = scene.addEntity(std::move(group));

    EditorEntity child = makeEntity(registry, "Cutscene Camera", 0.0f, 0.0f);
    addComponentWithDefaults(child, registry, BuiltinComponentIds::kCamera);
    const Uuid childId = scene.addEntity(std::move(child));
    CNA_EDITOR_EXPECT(scene.reparentEntity(childId, groupId));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "duplicate-primary-camera"),
                         std::size_t{0});
}

CNA_EDITOR_TEST(ASceneWhoseCamerasAreAllSecondaryIsReported)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Camera", 0.0f, 0.0f);
    EditorComponent& camera = addComponentWithDefaults(entity, registry, BuiltinComponentIds::kCamera);
    camera.setProperty("isPrimary", PropertyValue{false});
    scene.addEntity(std::move(entity));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "no-primary-camera"), std::size_t{1});

    // Scene-wide: there is no offending entity, and blaming one would send the user to the
    // wrong row.
    for (const SceneIssue& issue : issues)
    {
        if (issue.ruleId == "no-primary-camera") { CNA_EDITOR_EXPECT(!issue.entityId.isValid()); }
    }

    // A scene with no cameras at all is not reported: it is a fragment, not a broken level.
    SceneDocument empty;
    empty.addEntity(makeEntity(registry, "Group", 0.0f, 0.0f));
    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(empty, registry), "no-primary-camera"), std::size_t{0});
}

CNA_EDITOR_TEST(InvertedCameraPlanesAreAnError)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Camera", 0.0f, 0.0f);
    EditorComponent& camera = addComponentWithDefaults(entity, registry, BuiltinComponentIds::kCamera);
    camera.setProperty("nearPlane", PropertyValue{1000.0f});
    camera.setProperty("farPlane", PropertyValue{0.1f});
    scene.addEntity(std::move(entity));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "camera-planes-inverted"),
                         std::size_t{1});
}

CNA_EDITOR_TEST(AZeroScaleIsReportedOnAnyAxis)
{
    const ComponentRegistry registry = makeRegistry();

    for (int axis = 0; axis < 3; ++axis)
    {
        SceneDocument scene;
        EditorEntity entity = makeEntity(registry, "Player", 0.0f, 0.0f);
        addComponentWithDefaults(entity, registry, BuiltinComponentIds::kSpriteRenderer);

        EditorVector3 scale{1.0f, 1.0f, 1.0f};
        (axis == 0 ? scale.x : axis == 1 ? scale.y : scale.z) = 0.0f;
        entity.findComponent(BuiltinComponentIds::kTransform)->setProperty("scale", PropertyValue{scale});

        scene.addEntity(std::move(entity));

        CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "zero-scale"), std::size_t{1});
    }

    // A negative scale is a mirror, not a mistake, and must not be reported.
    SceneDocument mirrored;
    EditorEntity entity = makeEntity(registry, "Player", 0.0f, 0.0f);
    addComponentWithDefaults(entity, registry, BuiltinComponentIds::kSpriteRenderer);
    entity.findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("scale", PropertyValue{EditorVector3{-1.0f, 1.0f, 1.0f}});
    mirrored.addEntity(std::move(entity));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(mirrored, registry), "zero-scale"), std::size_t{0});
}

CNA_EDITOR_TEST(AnEntityThatDoesNothingIsReportedButAGroupIsNot)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    // A transform and nothing else, with no children: it occupies a row in the hierarchy and
    // does not exist as far as the game is concerned.
    scene.addEntity(makeEntity(registry, "Leftover", 0.0f, 0.0f));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "empty-entity"), std::size_t{1});

    // The same entity with a child is a group, which is the ordinary way to move a set of things
    // together, and must stay silent.
    SceneDocument grouped;
    const Uuid parent = grouped.addEntity(makeEntity(registry, "Enemies", 0.0f, 0.0f));

    EditorEntity child = makeEntity(registry, "Enemy", 0.0f, 0.0f);
    addComponentWithDefaults(child, registry, BuiltinComponentIds::kSpriteRenderer);
    const Uuid childId = grouped.addEntity(std::move(child));
    CNA_EDITOR_EXPECT(grouped.reparentEntity(childId, parent));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(grouped, registry), "empty-entity"), std::size_t{0});
}

CNA_EDITOR_TEST(ASpriteWithNoTextureIsReported)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Player", 0.0f, 0.0f);
    addComponentWithDefaults(entity, registry, BuiltinComponentIds::kSpriteRenderer);
    scene.addEntity(std::move(entity));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "sprite-without-texture"), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(countIssues(issues, SceneIssue::Severity::Error), std::size_t{0});

    // Once it points at something, the rule stops firing -- whether that id resolves is the
    // missing-reference report's question, not this one's.
    SceneDocument textured;
    EditorEntity sprite = makeEntity(registry, "Player", 0.0f, 0.0f);
    EditorComponent& renderer =
        addComponentWithDefaults(sprite, registry, BuiltinComponentIds::kSpriteRenderer);
    renderer.setProperty("texture", PropertyValue{PropertyValue::AssetReference{Uuid::generate()}});
    textured.addEntity(std::move(sprite));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(textured, registry), "sprite-without-texture"),
                         std::size_t{0});
}

CNA_EDITOR_TEST(AnUnregisteredComponentTypeIsReportedRatherThanIgnored)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Spawner", 0.0f, 0.0f);
    entity.addComponent(EditorComponent{"Mc3.SpawnPoint"});
    scene.addEntity(std::move(entity));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "unknown-component-type"), std::size_t{1});

    // And it counts as doing something: the editor cannot see what it does, which is not the
    // same as knowing that it does nothing.
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "empty-entity"), std::size_t{0});
}

CNA_EDITOR_TEST(AnEntityWithoutATransformIsAnError)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    // Reachable from a hand-edited file, or from a build older than the Transform requirement.
    scene.addEntity(EditorEntity{Uuid::generate(), "Ghost"});

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "missing-required-component"), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(countIssues(issues, SceneIssue::Severity::Error), std::size_t{1});
}

CNA_EDITOR_TEST(ASecondUniqueComponentIsAnError)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    EditorEntity entity = makeEntity(registry, "Player", 0.0f, 0.0f);
    addComponentWithDefaults(entity, registry, BuiltinComponentIds::kTransform);
    scene.addEntity(std::move(entity));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);

    // Reported once for the pair, not once per instance: the user has one problem to fix.
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "duplicate-component"), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(issues.front().componentTypeId, std::string{BuiltinComponentIds::kTransform});
}

CNA_EDITOR_TEST(IssuesComeBackInDocumentOrder)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    scene.addEntity(makeEntity(registry, "First", 0.0f, 0.0f));
    scene.addEntity(makeEntity(registry, "Second", 0.0f, 0.0f));
    scene.addEntity(makeEntity(registry, "Third", 0.0f, 0.0f));

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(issues.size(), std::size_t{3});
    CNA_EDITOR_EXPECT_EQ(issues[0].entityName, std::string{"First"});
    CNA_EDITOR_EXPECT_EQ(issues[1].entityName, std::string{"Second"});
    CNA_EDITOR_EXPECT_EQ(issues[2].entityName, std::string{"Third"});

    // Pure: the same document must produce the same report, or it cannot be diffed or asserted on.
    CNA_EDITOR_EXPECT(validateScene(scene, registry).size() == issues.size());
}

CNA_EDITOR_TEST(AnEntityLeftOnARenamedLayerIsReportedNotRewritten)
{
    ComponentRegistry registry = makeRegistry();
    applyProjectLayers(registry, {"Background", "Default"});

    SceneDocument scene;
    EditorEntity entity = makeEntity(registry, "Backdrop", 0.0f, 0.0f);
    addComponentWithDefaults(entity, registry, BuiltinComponentIds::kSpriteRenderer);
    EditorComponent& layer = addComponentWithDefaults(entity, registry, BuiltinComponentIds::kLayer);
    layer.setProperty("layer", PropertyValue{PropertyValue::EnumValue{"Background"}});
    const Uuid entityId = scene.addEntity(std::move(entity));

    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(scene, registry), "unknown-enum-value"), std::size_t{0});

    // Rename the layer out from under it, which is exactly what editing the project's layer list
    // does. The stored value is kept: which of the remaining layers the user meant is their
    // decision, not the editor's.
    applyProjectLayers(registry, {"Backdrop", "Default"});

    const std::vector<SceneIssue> issues = validateScene(scene, registry);
    CNA_EDITOR_EXPECT_EQ(countRule(issues, "unknown-enum-value"), std::size_t{1});

    const EditorComponent* stored = scene.findEntity(entityId)->findComponent(BuiltinComponentIds::kLayer);
    CNA_EDITOR_EXPECT_EQ(stored->getProperty("layer").get<PropertyValue::EnumValue>().name,
                         std::string{"Background"});

    // And it is a warning, not an error: the scene still runs and nothing was lost.
    CNA_EDITOR_EXPECT_EQ(countIssues(issues, SceneIssue::Severity::Error), std::size_t{0});
}

CNA_EDITOR_TEST(TagsAreAListOnTheirOwnComponent)
{
    const ComponentRegistry registry = makeRegistry();

    // Its own component rather than a field on EditorEntity: a tag is a game concept and the
    // entity type is deliberately not one (D-04).
    const ComponentDescriptor* descriptor = registry.find(BuiltinComponentIds::kTags);
    CNA_EDITOR_EXPECT(descriptor != nullptr);

    const PropertyDescriptor* tags = descriptor->findProperty("tags");
    CNA_EDITOR_EXPECT(tags != nullptr);
    CNA_EDITOR_EXPECT(tags->type == PropertyType::List);
    CNA_EDITOR_EXPECT(tags->elementType == PropertyType::String);

    SceneDocument scene;
    EditorEntity entity = makeEntity(registry, "Enemy", 0.0f, 0.0f);
    EditorComponent& component = addComponentWithDefaults(entity, registry, BuiltinComponentIds::kTags);

    PropertyValue::ListValue list;
    list.items.emplace_back(std::string{"enemy"});
    list.items.emplace_back(std::string{"spawns-loot"});
    component.setProperty("tags", PropertyValue{list});
    scene.addEntity(std::move(entity));

    SceneDocument reloaded;
    CNA_EDITOR_EXPECT(reloaded.loadFromJson(scene.toJson(), registry).succeeded);
    CNA_EDITOR_EXPECT(reloaded.getEntities().front().findComponent(BuiltinComponentIds::kTags)
                          ->getProperty("tags") == PropertyValue{list});

    // An entity carrying only a transform and a tag list still does nothing, and says so.
    CNA_EDITOR_EXPECT_EQ(countRule(validateScene(reloaded, registry), "empty-entity"), std::size_t{0});
}

// --------------------------------------------------------------------------------------------
// Prefabs (plan.md ED-300)
// --------------------------------------------------------------------------------------------

namespace
{
    /** @brief Builds a two-entity scene: "Enemy" with a sprite, and a child "Weapon". */
    struct PrefabScene
    {
        ComponentRegistry registry = makeRegistry();
        SceneDocument scene;
        Uuid rootId;
        Uuid childId;

        PrefabScene()
        {
            EditorEntity root = makeEntity(registry, "Enemy", 10.0f, 20.0f);
            addComponentWithDefaults(root, registry, BuiltinComponentIds::kSpriteRenderer);
            rootId = scene.addEntity(std::move(root));

            EditorEntity child = makeEntity(registry, "Weapon", 5.0f, 0.0f);
            addComponentWithDefaults(child, registry, BuiltinComponentIds::kSpriteRenderer);
            childId = scene.addEntity(std::move(child));
            scene.reparentEntity(childId, rootId);
        }

        [[nodiscard]] PrefabDocument capture(const std::string& name = "Enemy") const
        {
            PrefabDocument prefab;
            prefab.captureFromScene(scene, rootId, name);
            return prefab;
        }
    };
}

CNA_EDITOR_TEST(APrefabCapturesASubtreeAndRoundTripsThroughAFile)
{
    const PrefabScene fixture;
    const PrefabDocument prefab = fixture.capture();

    CNA_EDITOR_EXPECT_EQ(prefab.getEntities().size(), std::size_t{2});
    CNA_EDITOR_EXPECT(prefab.getRootId() == fixture.rootId);
    CNA_EDITOR_EXPECT(prefab.getPrefabId().isValid());

    // The root has no parent inside the prefab: keeping the scene's would make the file describe a
    // hierarchy that only exists where it was captured from.
    CNA_EDITOR_EXPECT(!prefab.getEntities().front().getParentId().isValid());
    CNA_EDITOR_EXPECT(prefab.getEntities().back().getParentId() == fixture.rootId);

    PrefabDocument reloaded;
    const PrefabLoadResult result = reloaded.loadFromJson(prefab.toJson(), fixture.registry);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT(result.warnings.empty());
    CNA_EDITOR_EXPECT_EQ(Json::write(reloaded.toJson()), Json::write(prefab.toJson()));

    // Capturing something that is not in the scene fails rather than producing an empty prefab.
    PrefabDocument missing;
    CNA_EDITOR_EXPECT(!missing.captureFromScene(fixture.scene, Uuid::generate(), "Nothing"));
}

CNA_EDITOR_TEST(APrefabRefusesAnEmptyFileAndRepairsABrokenOne)
{
    const ComponentRegistry registry = makeRegistry();

    JsonValue json = JsonValue::makeObject();
    json.set("formatVersion", JsonValue{PrefabDocument::kFormatVersion});
    json.set("prefabId", JsonValue{Uuid::generate().toString()});
    json.set("entities", JsonValue::makeArray());

    // A prefab with no entities instantiates to nothing, and the user would have no way to tell
    // that from an instantiation that silently failed.
    PrefabDocument empty;
    CNA_EDITOR_EXPECT(!empty.loadFromJson(json, registry).succeeded);

    // A child whose parent is not in the file is attached to the root rather than dropped: a file
    // broken by a bad merge is exactly the one somebody needs the editor to open.
    const Uuid rootId = Uuid::generate();

    JsonValue root = JsonValue::makeObject();
    root.set("id", JsonValue{rootId.toString()});
    root.set("name", JsonValue{"Root"});
    root.set("components", JsonValue::makeObject());

    JsonValue orphan = JsonValue::makeObject();
    orphan.set("id", JsonValue{Uuid::generate().toString()});
    orphan.set("name", JsonValue{"Orphan"});
    orphan.set("parent", JsonValue{Uuid::generate().toString()});
    orphan.set("components", JsonValue::makeObject());

    JsonValue entities = JsonValue::makeArray();
    entities.append(std::move(root));
    entities.append(std::move(orphan));
    json.set("entities", std::move(entities));

    PrefabDocument repaired;
    const PrefabLoadResult result = repaired.loadFromJson(json, registry);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(repaired.getEntities().back().getParentId() == rootId);

    // A file from a newer build is refused by the same gate every other format uses.
    json.set("formatVersion", JsonValue{PrefabDocument::kFormatVersion + 1});
    PrefabDocument future;
    CNA_EDITOR_EXPECT(!future.loadFromJson(json, registry).succeeded);
    CNA_EDITOR_EXPECT_EQ(getPrefabFormatMigrator().getMigrationCount(), std::size_t{0});
}

CNA_EDITOR_TEST(InstantiatingAPrefabGivesFreshIdsAndKeepsTheLink)
{
    PrefabScene fixture;
    const PrefabDocument prefab = fixture.capture();
    const Uuid assetId = Uuid::generate();

    SceneDocument target;
    auto command = std::make_unique<InstantiatePrefabCommand>(target, prefab, assetId, Uuid{});
    CNA_EDITOR_EXPECT(command->isValid());

    const Uuid instanceRoot = command->getRootId();
    command->execute();

    CNA_EDITOR_EXPECT_EQ(target.getEntityCount(), std::size_t{2});
    CNA_EDITOR_EXPECT(getPrefabAssetOf(target, instanceRoot) == assetId);

    // Fresh ids: two instances of one prefab are two different entities, and reusing the prefab's
    // would make the second instantiation collide with the first.
    CNA_EDITOR_EXPECT(instanceRoot != fixture.rootId);

    auto second = std::make_unique<InstantiatePrefabCommand>(target, prefab, assetId, Uuid{});
    second->execute();
    CNA_EDITOR_EXPECT_EQ(target.getEntityCount(), std::size_t{4});
    CNA_EDITOR_EXPECT(second->getRootId() != instanceRoot);

    // Every entity carries its link, so selecting a child still answers questions about the
    // instance it belongs to.
    const std::vector<Uuid> children = target.getChildren(instanceRoot);
    CNA_EDITOR_EXPECT_EQ(children.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(findInstanceRoot(target, children.front()) == instanceRoot);

    // A freshly made instance has changed nothing.
    CNA_EDITOR_EXPECT(findPrefabOverrides(target, instanceRoot, prefab).empty());

    second->undo();
    command->undo();
    CNA_EDITOR_EXPECT_EQ(target.getEntityCount(), std::size_t{0});

    // An empty prefab, or an unknown parent, is refused rather than half-applied.
    const PrefabDocument nothing;
    CNA_EDITOR_EXPECT(!InstantiatePrefabCommand(target, nothing, assetId, Uuid{}).isValid());
    CNA_EDITOR_EXPECT(!InstantiatePrefabCommand(target, prefab, assetId, Uuid::generate()).isValid());
}

CNA_EDITOR_TEST(OverridesAreFoundByComparingRatherThanByRecording)
{
    PrefabScene fixture;
    const PrefabDocument prefab = fixture.capture();

    SceneDocument target;
    InstantiatePrefabCommand instantiate{target, prefab, Uuid::generate(), Uuid{}};
    instantiate.execute();
    const Uuid instanceRoot = instantiate.getRootId();

    // Change a property, rename an entity, add one and delete one -- four different ways an
    // instance can diverge, all of them ordinary things a user does.
    target.findEntity(instanceRoot)
        ->findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("position", PropertyValue{EditorVector3{999.0f, 0.0f, 0.0f}});
    target.findEntity(instanceRoot)->setName("Boss");

    EditorEntity extra = makeEntity(fixture.registry, "Shield", 0.0f, 0.0f);
    const Uuid extraId = target.addEntity(std::move(extra));
    target.reparentEntity(extraId, instanceRoot);

    const std::vector<PrefabOverride> overrides = findPrefabOverrides(target, instanceRoot, prefab);

    const auto countKind = [&](PrefabOverride::Kind kind) {
        return static_cast<std::size_t>(
            std::count_if(overrides.begin(), overrides.end(),
                          [kind](const PrefabOverride& entry) { return entry.kind == kind; }));
    };

    CNA_EDITOR_EXPECT_EQ(countKind(PrefabOverride::Kind::AddedEntity), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(countKind(PrefabOverride::Kind::RemovedEntity), std::size_t{0});
    CNA_EDITOR_EXPECT(countKind(PrefabOverride::Kind::Property) >= std::size_t{2});

    bool sawRename = false;
    bool sawPosition = false;
    for (const PrefabOverride& entry : overrides)
    {
        if (entry.propertyName == "name") { sawRename = true; }
        if (entry.propertyName == "position") { sawPosition = true; }
    }
    CNA_EDITOR_EXPECT(sawRename);
    CNA_EDITOR_EXPECT(sawPosition);

    // Deleting one of the prefab's *own* entities is a removal, not a silent match. Found by its
    // link rather than by position: the children are ordered by name, and "Shield" sorts before
    // "Weapon", so an index here would delete the entity the test just added.
    Uuid weaponId;
    for (const Uuid& childId : target.getChildren(instanceRoot))
    {
        if (target.findEntity(childId)->getName() == "Weapon") { weaponId = childId; }
    }
    CNA_EDITOR_EXPECT(weaponId.isValid());
    target.removeEntityRecursive(weaponId);
    const std::vector<PrefabOverride> afterDelete = findPrefabOverrides(target, instanceRoot, prefab);
    CNA_EDITOR_EXPECT(std::any_of(afterDelete.begin(), afterDelete.end(),
                                  [](const PrefabOverride& entry)
                                  { return entry.kind == PrefabOverride::Kind::RemovedEntity; }));
}

CNA_EDITOR_TEST(RevertingAnInstancePutsItBackExactlyAndUndoesInOnePress)
{
    PrefabScene fixture;
    const PrefabDocument prefab = fixture.capture();

    SceneDocument target;
    InstantiatePrefabCommand instantiate{target, prefab, Uuid::generate(), Uuid{}};
    instantiate.execute();
    const Uuid instanceRoot = instantiate.getRootId();

    target.findEntity(instanceRoot)->setName("Boss");
    target.findEntity(instanceRoot)
        ->findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("position", PropertyValue{EditorVector3{999.0f, 0.0f, 0.0f}});

    EditorEntity extra = makeEntity(fixture.registry, "Shield", 0.0f, 0.0f);
    const Uuid extraId = target.addEntity(std::move(extra));
    target.reparentEntity(extraId, instanceRoot);

    auto revert = std::make_unique<RevertPrefabInstanceCommand>(target, instanceRoot, prefab);
    CNA_EDITOR_EXPECT(revert->isValid());
    revert->execute();

    CNA_EDITOR_EXPECT(findPrefabOverrides(target, instanceRoot, prefab).empty());
    CNA_EDITOR_EXPECT_EQ(target.getEntityCount(), std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(target.findEntity(instanceRoot)->getName(), std::string{"Enemy"});

    // The added entity is gone. That is deliberate: a revert that left some of the user's changes
    // in place would not be a revert, and the user who wanted a partial one has undo.
    CNA_EDITOR_EXPECT(target.findEntity(extraId) == nullptr);

    // The root keeps its id, because the selection and every reference into the instance point at
    // it -- reverting must not break them.
    CNA_EDITOR_EXPECT(target.findEntity(instanceRoot) != nullptr);
    CNA_EDITOR_EXPECT(getPrefabAssetOf(target, instanceRoot).isValid());

    revert->undo();
    CNA_EDITOR_EXPECT_EQ(target.getEntityCount(), std::size_t{3});
    CNA_EDITOR_EXPECT_EQ(target.findEntity(instanceRoot)->getName(), std::string{"Boss"});
    CNA_EDITOR_EXPECT(target.findEntity(extraId) != nullptr);

    // Reverting an instance that has not diverged is refused: an undo entry that undoes to the
    // state it is already in reads to the user as a broken Ctrl+Z.
    revert->execute();
    CNA_EDITOR_EXPECT(!RevertPrefabInstanceCommand(target, instanceRoot, prefab).isValid());

    // As is reverting something that is not an instance at all.
    const Uuid plain = target.addEntity(makeEntity(fixture.registry, "Plain", 0.0f, 0.0f));
    CNA_EDITOR_EXPECT(!RevertPrefabInstanceCommand(target, plain, prefab).isValid());
}
