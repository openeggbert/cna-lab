// SPDX-License-Identifier: MS-PL
/**
 * @file ApplicationTests.cpp
 * @brief End-to-end tests over the whole editor, driven through the null UI and viewport.
 *
 * These run the *real* EditorApplication -- there is no separate test mode that could drift away
 * from what a user gets. That is the payoff of injecting the UI and the viewport rather than
 * having the application create them (ANALYSIS.md decision D-02).
 */

#include "TestHarness.hpp"

#include <filesystem>
#include <fstream>

#include "CNA/Editor/EditorApplication.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

using namespace CNA::Editor;

namespace
{
    std::filesystem::path makeScratchDirectory(const std::string& name)
    {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() / ("cna-editor-tests-" + name + "-" + Uuid::generate().toString());
        std::filesystem::create_directories(directory);
        return directory;
    }

    void writeFile(const std::filesystem::path& path, std::string_view contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        stream << contents;
    }

    /** @brief Builds an application over the null UI and viewport. */
    EditorApplication makeApplication()
    {
        return EditorApplication{std::make_unique<NullEditorUi>(), std::make_unique<NullEditorViewport>()};
    }
}

CNA_EDITOR_TEST(OptionsParseTheDocumentedFlags)
{
    const char* argv[] = {"cna-editor", "--project=/tmp/MyGame.cnaproject", "--headless", "--frames=3"};
    const EditorOptions options = EditorOptions::parse(4, argv);

    CNA_EDITOR_EXPECT(!options.hasError);
    CNA_EDITOR_EXPECT_EQ(options.projectPath, std::string{"/tmp/MyGame.cnaproject"});
    CNA_EDITOR_EXPECT(options.headless);
    CNA_EDITOR_EXPECT_EQ(options.frameLimit, 3);
}

CNA_EDITOR_TEST(OptionsAcceptABareProjectPath)
{
    const char* argv[] = {"cna-editor", "/tmp/MyGame.cnaproject"};
    const EditorOptions options = EditorOptions::parse(2, argv);
    CNA_EDITOR_EXPECT(!options.hasError);
    CNA_EDITOR_EXPECT_EQ(options.projectPath, std::string{"/tmp/MyGame.cnaproject"});
}

CNA_EDITOR_TEST(OptionsRejectGraphicsBackendSelection)
{
    // CNA fixes its backend at compile time, so accepting --graphics here would teach users a
    // mental model the framework does not support. Rejecting it loudly is the honest behaviour.
    const char* argv[] = {"cna-editor", "--graphics=vulkan"};
    const EditorOptions options = EditorOptions::parse(2, argv);

    CNA_EDITOR_EXPECT(options.hasError);
    CNA_EDITOR_EXPECT(options.errorMessage.find("compile time") != std::string::npos);
    CNA_EDITOR_EXPECT(options.errorMessage.find("cna-player") != std::string::npos);
}

CNA_EDITOR_TEST(OptionsRejectUnknownFlags)
{
    const char* argv[] = {"cna-editor", "--nonsense"};
    const EditorOptions options = EditorOptions::parse(2, argv);
    CNA_EDITOR_EXPECT(options.hasError);
}

CNA_EDITOR_TEST(UsageTextExplainsTheBackendConstraint)
{
    const std::string usage = EditorOptions::getUsage();
    CNA_EDITOR_EXPECT(usage.find("compile") != std::string::npos);
    CNA_EDITOR_EXPECT(usage.find("cna-player") != std::string::npos);
}

CNA_EDITOR_TEST(ApplicationStartsWithAUsableEmptyScene)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;

    CNA_EDITOR_EXPECT(application.initialize(options));

    // A scene with no camera renders nothing, which reads as "the editor is broken".
    const SceneDocument& scene = application.getContext().getScene();
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{1});
    CNA_EDITOR_EXPECT(scene.getEntities().front().findComponent(BuiltinComponentIds::kCamera) != nullptr);
    CNA_EDITOR_EXPECT(scene.getEntities().front().findComponent(BuiltinComponentIds::kTransform) != nullptr);
}

CNA_EDITOR_TEST(ApplicationDrawsEveryPanelEachFrame)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    auto& ui = static_cast<NullEditorUi&>(application.getUi());
    CNA_EDITOR_EXPECT(ui.beginFrame());
    application.renderFrame();
    ui.endFrame();

    const std::vector<std::string>& panels = ui.getLastFramePanels();
    CNA_EDITOR_EXPECT_EQ(panels.size(), std::size_t{5});

    const auto contains = [&](const std::string& title) {
        return std::find(panels.begin(), panels.end(), title) != panels.end();
    };
    CNA_EDITOR_EXPECT(contains("Scene Hierarchy"));
    CNA_EDITOR_EXPECT(contains("Viewport"));
    CNA_EDITOR_EXPECT(contains("Inspector"));
    CNA_EDITOR_EXPECT(contains("Assets"));
    CNA_EDITOR_EXPECT(contains("Console"));

    // The viewport must actually have rendered, or --headless would be a no-op rather than a
    // smoke test. NullEditorViewport walks the same transform and bounds code a real one does.
    const auto& viewport = static_cast<NullEditorViewport&>(application.getViewport());
    CNA_EDITOR_EXPECT_EQ(viewport.getRenderCount(), std::uint64_t{1});
    CNA_EDITOR_EXPECT(viewport.getWidth() > 0);
}

CNA_EDITOR_TEST(ApplicationHonoursItsFrameLimit)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    options.frameLimit = 4;
    CNA_EDITOR_EXPECT(application.initialize(options));

    CNA_EDITOR_EXPECT_EQ(application.run(), 0);
    CNA_EDITOR_EXPECT_EQ(static_cast<NullEditorUi&>(application.getUi()).getFrameCount(), std::uint64_t{4});
}

CNA_EDITOR_TEST(ApplicationWalksADeepHierarchyWithoutCrashing)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();
    SceneDocument& scene = context.getScene();

    Uuid parent;
    for (int depth = 0; depth < 64; ++depth)
    {
        EditorEntity entity{Uuid::generate(), "Level" + std::to_string(depth)};
        entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
        const Uuid id = scene.addEntity(std::move(entity));
        if (parent.isValid()) { scene.reparentEntity(id, parent); }
        parent = id;
    }

    context.select(parent);
    application.renderFrame();

    // NullEditorUi expands every node, so this really did walk all 64 levels.
    CNA_EDITOR_EXPECT_EQ(scene.getEntityCount(), std::size_t{65});
    CNA_EDITOR_EXPECT(context.getPrimarySelection() == parent);
}

CNA_EDITOR_TEST(SelectionIsPrunedWhenItsEntityIsDeleted)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();
    EditorEntity entity{Uuid::generate(), "Doomed"};
    entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
    const Uuid id = context.getScene().addEntity(std::move(entity));

    context.select(id);
    CNA_EDITOR_EXPECT(context.isSelected(id));

    context.execute(std::make_unique<DeleteEntityCommand>(context.getScene(), id));

    // Without pruning, the inspector would keep showing a deleted entity and the next command
    // would target a missing id.
    CNA_EDITOR_EXPECT(!context.isSelected(id));
    CNA_EDITOR_EXPECT(!context.getPrimarySelection().isValid());

    // ...and undo must not silently resurrect the selection either; the entity returns, the
    // selection does not, which is what every editor does.
    context.getHistory().undo();
    CNA_EDITOR_EXPECT(context.getScene().findEntity(id) != nullptr);
    CNA_EDITOR_EXPECT(!context.isSelected(id));
}

CNA_EDITOR_TEST(FullProjectRoundTripThroughTheApplication)
{
    // The Phase 1 milestone in miniature: open a project, load a scene, edit a property, undo,
    // redo, save, and reopen to confirm the edit persisted.
    const std::filesystem::path directory = makeScratchDirectory("app");

    Project project = Project::createDefault("MyGame", directory.generic_string());
    project.setStartupScene("Scenes/Level01.cnascene");
    std::string errorMessage;
    CNA_EDITOR_EXPECT(project.saveToFile({}, &errorMessage));

    writeFile(directory / "Assets" / "player.png", "pixels");

    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    SceneDocument scene;
    scene.setName("Level01");
    EditorEntity player{Uuid::generate(), "Player"};
    EditorComponent transform{BuiltinComponentIds::kTransform};
    transform.applyDefaults(*registry.find(BuiltinComponentIds::kTransform));
    player.addComponent(std::move(transform));
    const Uuid playerId = scene.addEntity(std::move(player));
    CNA_EDITOR_EXPECT(scene.saveToFile((directory / "Scenes" / "Level01.cnascene").generic_string(), &errorMessage));

    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    options.projectPath = project.getFilePath();
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();
    CNA_EDITOR_EXPECT(context.hasProject());
    CNA_EDITOR_EXPECT_EQ(context.getScene().getName(), std::string{"Level01"});
    CNA_EDITOR_EXPECT(context.getScene().findEntity(playerId) != nullptr);
    CNA_EDITOR_EXPECT_EQ(context.getAssets().getCount(), std::size_t{1});

    context.select(playerId);
    context.execute(std::make_unique<SetPropertyCommand>(
        context.getScene(), playerId, BuiltinComponentIds::kTransform, "position",
        PropertyValue{EditorVector3{100.0f, 220.0f, 0.0f}}));

    CNA_EDITOR_EXPECT(context.getHistory().isDirty());
    CNA_EDITOR_EXPECT(context.getHistory().undo());
    CNA_EDITOR_EXPECT(context.getHistory().redo());
    CNA_EDITOR_EXPECT(context.saveScene());
    CNA_EDITOR_EXPECT(!context.getHistory().isDirty());

    application.renderFrame();

    SceneDocument reopened;
    const SceneLoadResult result =
        reopened.loadFromFile((directory / "Scenes" / "Level01.cnascene").generic_string(), registry);
    CNA_EDITOR_EXPECT(result.succeeded);

    const EditorComponent* savedTransform =
        reopened.findEntity(playerId)->findComponent(BuiltinComponentIds::kTransform);
    CNA_EDITOR_EXPECT_EQ(savedTransform->getProperty("position").get<EditorVector3>().x, 100.0f);
    CNA_EDITOR_EXPECT_EQ(savedTransform->getProperty("position").get<EditorVector3>().y, 220.0f);

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(ApplicationReportsAMissingProjectRatherThanCrashing)
{
    EditorApplication application = makeApplication();
    EditorOptions options;
    options.headless = true;
    options.projectPath = "/definitely/not/here/Missing.cnaproject";

    CNA_EDITOR_EXPECT(!application.initialize(options));

    const auto& ui = static_cast<NullEditorUi&>(application.getUi());
    bool sawError = false;
    for (const auto& entry : ui.getLog())
    {
        if (entry.severity == LogSeverity::Error) { sawError = true; }
    }
    CNA_EDITOR_EXPECT(sawError);
}
