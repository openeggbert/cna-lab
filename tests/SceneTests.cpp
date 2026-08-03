// SPDX-License-Identifier: MS-PL
/**
 * @file SceneTests.cpp
 * @brief Tests for the scene document, its invariants, its serialisation and its undo behaviour.
 */

#include "TestHarness.hpp"

#include <algorithm>
#include <filesystem>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"
#include "CNA/Editor/Scene/SceneValidation.hpp"

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
