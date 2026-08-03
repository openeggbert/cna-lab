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

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "CNA/Editor/EditorApplication.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneTransform.hpp"
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

/**
 * @brief A null UI that reports whatever viewport interaction a test scripts.
 *
 * The gizmo's arithmetic is covered in ViewportTests; what these tests cover is the *wiring* --
 * that a press on a handle starts a drag rather than a reselection, that the drag survives the
 * pointer leaving the panel, and that one drag is one undo entry. All of that lives in
 * EditorApplication and none of it is reachable without a UI that can say "the left button just
 * went down here".
 */
namespace
{
    class ScriptedUi final : public NullEditorUi
    {
    public:
        UiImageInteraction interaction;

        /** @brief Buttons to report as clicked, by label. Each entry fires once. */
        std::vector<std::string> pendingClicks;

        /** @brief Enum choices to apply, as (field label, option) pairs. Each fires once. */
        std::vector<std::pair<std::string, std::string>> pendingChoices;

        /** @brief Non-enum edits to apply, as (field label, new value) pairs. Each fires once. */
        std::vector<std::pair<std::string, PropertyValue>> pendingEdits;

        /** @brief Values the inspector showed during the most recent frame, by label. */
        std::vector<std::pair<std::string, PropertyValue>> shownValues;

        /** @brief Tree nodes to report as clicked, by entity id. Each fires once. */
        std::vector<Uuid> pendingNodeClicks;

        /** @brief Tree nodes to report as double-clicked, by entity id. Each fires once. */
        std::vector<Uuid> pendingNodeDoubleClicks;

        /** @brief Drops to deliver, as (drop target entity, dragged payload). Each fires once. */
        std::vector<std::pair<Uuid, std::string>> pendingDrops;

        /** @brief Text to type into the next rename field, and whether to commit it. */
        std::optional<std::pair<std::string, bool>> pendingRename;

        /** @brief Modifiers to report as held. */
        UiKeyModifiers modifiers;

        /** @brief The most recent node drawn, so drag and drop calls can be attributed to it. */
        Uuid lastNode;

        /** @brief The most recent property field drawn, for the same reason. */
        std::string lastField;

        /** @brief Entity ids the hierarchy drew a node for during the most recent frame. */
        std::vector<Uuid> drawnNodes;

        /** @brief True when a rename field was drawn during the most recent frame. */
        bool sawRenameField = false;

        /** @brief Button labels drawn during the most recent frame. */
        std::vector<std::string> drawnButtons;

        /** @brief Enum options last offered for a field, by label. */
        std::vector<std::pair<std::string, std::vector<std::string>>> offeredOptions;

        UiImageInteraction image(const std::string& id,
                                 UiTextureId texture,
                                 float width,
                                 float height,
                                 bool flipVertically) override
        {
            (void)id;
            (void)texture;
            (void)width;
            (void)height;
            (void)flipVertically;
            return interaction;
        }

        /** @brief Clears the per-frame record. renderFrame() opens the dock space first. */
        void beginDockSpace() override
        {
            drawnButtons.clear();
            offeredOptions.clear();
            shownValues.clear();
            shownChecks.clear();
            drawnNodes.clear();
            sawRenameField = false;
        }

        [[nodiscard]] UiKeyModifiers getModifiers() const override { return modifiers; }

        UiTreeNodeResult treeNode(const Uuid& id,
                                  const std::string& label,
                                  bool selected,
                                  bool leaf) override
        {
            drawnNodes.push_back(id);
            lastNode = id;

            UiTreeNodeResult result = NullEditorUi::treeNode(id, label, selected, leaf);

            for (auto entry = pendingNodeClicks.begin(); entry != pendingNodeClicks.end(); ++entry)
            {
                if (*entry != id) { continue; }
                pendingNodeClicks.erase(entry);
                result.clicked = true;
                break;
            }
            for (auto entry = pendingNodeDoubleClicks.begin();
                 entry != pendingNodeDoubleClicks.end(); ++entry)
            {
                if (*entry != id) { continue; }
                pendingNodeDoubleClicks.erase(entry);
                result.doubleClicked = true;
                break;
            }
            return result;
        }

        /** @brief Asset payloads to deliver to the next field with a matching label. */
        std::vector<std::pair<std::string, std::string>> pendingAssetDrops;

        std::optional<std::string> acceptDrop(const std::string& type) override
        {
            if (type == "asset")
            {
                for (auto entry = pendingAssetDrops.begin(); entry != pendingAssetDrops.end(); ++entry)
                {
                    if (entry->first != lastField) { continue; }
                    const std::string payload = entry->second;
                    pendingAssetDrops.erase(entry);
                    return payload;
                }
                return std::nullopt;
            }

            for (auto entry = pendingDrops.begin(); entry != pendingDrops.end(); ++entry)
            {
                if (entry->first != lastNode) { continue; }
                const std::string payload = entry->second;
                pendingDrops.erase(entry);
                return payload;
            }
            return std::nullopt;
        }

        UiTextFieldResult inputText(const std::string& id, std::string& text, bool takeFocus) override
        {
            (void)id;
            (void)takeFocus;
            sawRenameField = true;

            if (!pendingRename) { return UiTextFieldResult{}; }

            UiTextFieldResult result;
            text = pendingRename->first;
            result.changed = true;
            result.committed = pendingRename->second;
            if (result.committed) { pendingRename.reset(); }
            return result;
        }

        bool checkbox(const std::string& label, bool& value) override
        {
            shownChecks.emplace_back(label, value);

            for (auto entry = pendingChecks.begin(); entry != pendingChecks.end(); ++entry)
            {
                if (entry->first != label) { continue; }
                value = entry->second;
                pendingChecks.erase(entry);
                return true;
            }
            return false;
        }

        /** @brief Checkbox states to apply, as (label, new value) pairs. Each fires once. */
        std::vector<std::pair<std::string, bool>> pendingChecks;

        /** @brief Checkbox states shown during the most recent frame. */
        std::vector<std::pair<std::string, bool>> shownChecks;

        /** @brief Returns the state @p label was drawn with, defaulting to false. */
        [[nodiscard]] bool shownCheckFor(const std::string& label) const
        {
            for (const auto& [field, value] : shownChecks)
            {
                if (field == label) { return value; }
            }
            return false;
        }

        bool button(const std::string& label) override
        {
            drawnButtons.push_back(label);

            for (auto entry = pendingClicks.begin(); entry != pendingClicks.end(); ++entry)
            {
                if (*entry != label) { continue; }
                pendingClicks.erase(entry);
                return true;
            }
            return false;
        }

        bool propertyField(const std::string& label,
                           PropertyValue& value,
                           const std::vector<std::string>& enumOptions = {},
                           bool readOnly = false) override
        {
            (void)readOnly;
            if (!enumOptions.empty()) { offeredOptions.emplace_back(label, enumOptions); }
            shownValues.emplace_back(label, value);
            lastField = label;

            for (auto entry = pendingChoices.begin(); entry != pendingChoices.end(); ++entry)
            {
                if (entry->first != label) { continue; }
                value = PropertyValue{PropertyValue::EnumValue{entry->second}};
                pendingChoices.erase(entry);
                return true;
            }

            for (auto entry = pendingEdits.begin(); entry != pendingEdits.end(); ++entry)
            {
                if (entry->first != label) { continue; }
                value = entry->second;
                pendingEdits.erase(entry);
                return true;
            }
            return false;
        }

        /** @brief Returns true when a button with @p label was drawn last frame. */
        [[nodiscard]] bool sawButton(const std::string& label) const
        {
            return std::find(drawnButtons.begin(), drawnButtons.end(), label) != drawnButtons.end();
        }

        /** @brief Returns the value last shown for @p label. */
        [[nodiscard]] PropertyValue shownValueFor(const std::string& label) const
        {
            for (const auto& [field, value] : shownValues)
            {
                if (field == label) { return value; }
            }
            return PropertyValue{};
        }

        /** @brief Returns the options last offered for @p label, or an empty list. */
        [[nodiscard]] std::vector<std::string> optionsFor(const std::string& label) const
        {
            for (const auto& [field, options] : offeredOptions)
            {
                if (field == label) { return options; }
            }
            return {};
        }
    };

    /** @brief Application, its scripted UI, and the id of the one entity in its scene. */
    struct GizmoFixture
    {
        std::unique_ptr<EditorApplication> application;
        ScriptedUi* ui = nullptr;
        Uuid entityId;

        /** @brief Feeds one frame of pointer input at a viewport-local point. */
        void step(const UiImageInteraction& input)
        {
            ui->interaction = input;
            application->renderFrame();
        }

        /** @brief Returns true when any console message contains @p needle. */
        [[nodiscard]] bool logContains(std::string_view needle) const
        {
            for (const auto& entry : ui->getLog())
            {
                if (entry.message.find(needle) != std::string::npos) { return true; }
            }
            return false;
        }

        [[nodiscard]] EditorVector3 getPosition() const
        {
            return application->getContext().getScene()
                .findEntity(entityId)->findComponent(BuiltinComponentIds::kTransform)
                ->getProperty("position").get<EditorVector3>();
        }
    };

    /**
     * @brief Builds an editor holding one entity at world (100, 220), selected.
     *
     * With the null UI's 1280x720 content region and the camera's defaults -- centre (0, 0), zoom 1
     * -- world (100, 220) lands at screen (740, 580), so the gizmo's origin and the coordinates the
     * tests press at are exact rather than approximate.
     */
    GizmoFixture makeGizmoFixture()
    {
        GizmoFixture fixture;
        auto ui = std::make_unique<ScriptedUi>();
        fixture.ui = ui.get();
        fixture.application =
            std::make_unique<EditorApplication>(std::move(ui), std::make_unique<NullEditorViewport>());

        EditorOptions options;
        options.headless = true;
        fixture.application->initialize(options);

        EditorContext& context = fixture.application->getContext();
        EditorEntity entity{Uuid::generate(), "Player"};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        transform.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kTransform));
        transform.setProperty("position", PropertyValue{EditorVector3{100.0f, 220.0f, 0.0f}});
        entity.addComponent(std::move(transform));

        fixture.entityId = entity.getId();
        context.getScene().addEntity(std::move(entity));
        context.select(fixture.entityId);

        // One frame with no input, so the viewport has sized the camera before any press is
        // hit-tested against a gizmo whose position depends on that size.
        fixture.step(UiImageInteraction{});
        return fixture;
    }

    /** @brief A hovered frame with the left button down at a viewport-local point. */
    UiImageInteraction leftAt(float x, float y, bool pressed)
    {
        UiImageInteraction input;
        input.hovered = true;
        input.localMouseX = x;
        input.localMouseY = y;
        input.leftPressed = pressed;
        input.leftDown = true;
        return input;
    }
}

CNA_EDITOR_TEST(AGizmoDragMovesTheSelectedEntityAlongTheGrabbedAxis)
{
    GizmoFixture fixture = makeGizmoFixture();

    // (790, 580) is on the X arm: the origin is at (740, 580) and the arm runs 72 pixels right.
    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(830.0f, 580.0f, false));
    fixture.step(leftAt(830.0f, 620.0f, false));

    UiImageInteraction release;
    release.hovered = true;
    release.localMouseX = 830.0f;
    release.localMouseY = 620.0f;
    release.leftReleased = true;
    fixture.step(release);

    // Moved 40 along X, and not at all along Y despite the pointer having moved 40 down.
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().y, 220.0f);
}

CNA_EDITOR_TEST(AGizmoDragOnTheCentreHandleMovesOnBothAxes)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(leftAt(740.0f, 580.0f, true));
    fixture.step(leftAt(765.0f, 605.0f, false));

    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 125.0f);
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().y, 245.0f);
}

CNA_EDITOR_TEST(AGizmoDragIsOneUndoEntryThatReturnsToWhereItStarted)
{
    GizmoFixture fixture = makeGizmoFixture();
    CommandHistory& history = fixture.application->getContext().getHistory();
    const std::size_t before = history.getCount();

    fixture.step(leftAt(790.0f, 580.0f, true));
    for (float x = 795.0f; x <= 830.0f; x += 5.0f) { fixture.step(leftAt(x, 580.0f, false)); }

    UiImageInteraction release;
    release.hovered = true;
    release.localMouseX = 830.0f;
    release.localMouseY = 580.0f;
    release.leftReleased = true;
    fixture.step(release);

    // Eight moved frames, one entry: this is what MergePolicy::MergeWithPrevious is for.
    CNA_EDITOR_EXPECT_EQ(history.getCount(), before + 1);
    CNA_EDITOR_EXPECT(history.undo());
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 100.0f);
}

CNA_EDITOR_TEST(TwoGizmoDragsAreTwoUndoEntries)
{
    GizmoFixture fixture = makeGizmoFixture();
    CommandHistory& history = fixture.application->getContext().getHistory();
    const std::size_t before = history.getCount();

    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(810.0f, 580.0f, false));
    fixture.step(UiImageInteraction{});

    // The second drag grabs the arm at its new place -- the gizmo moved with the entity.
    fixture.step(leftAt(810.0f, 580.0f, true));
    fixture.step(leftAt(830.0f, 580.0f, false));
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);

    // Two entries, not one. The merge key is entity + component + property and matches across both
    // drags, so only the interaction boundary keeps them apart -- undoing a move the user made a
    // minute ago because they later moved the same entity again is a real way to lose work.
    CNA_EDITOR_EXPECT_EQ(history.getCount(), before + 2);
    CNA_EDITOR_EXPECT(history.undo());
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 120.0f);
}

CNA_EDITOR_TEST(AGizmoDragSurvivesThePointerLeavingTheViewport)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(leftAt(790.0f, 580.0f, true));

    // Still held, no longer over the panel. Ending the drag here would drop the entity wherever
    // the pointer happened to cross the edge.
    UiImageInteraction outside;
    outside.hovered = false;
    outside.leftDown = true;
    outside.localMouseX = 830.0f;
    outside.localMouseY = 580.0f;
    fixture.step(outside);

    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);
}

CNA_EDITOR_TEST(AGizmoPressDoesNotAlsoChangeTheSelection)
{
    GizmoFixture fixture = makeGizmoFixture();

    // The entity has no SpriteRenderer, so the picker would find nothing here and deselect it --
    // which is exactly what must not happen when the press landed on a handle.
    UiImageInteraction press = leftAt(790.0f, 580.0f, true);
    press.clicked = true;
    fixture.step(press);

    CNA_EDITOR_EXPECT_EQ(fixture.application->getContext().getPrimarySelection().toString(),
                         fixture.entityId.toString());
}

CNA_EDITOR_TEST(PressingAHandleWithoutMovingLeavesTheHistoryAlone)
{
    GizmoFixture fixture = makeGizmoFixture();
    CommandHistory& history = fixture.application->getContext().getHistory();
    const std::size_t before = history.getCount();

    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(790.0f, 580.0f, false));

    UiImageInteraction release;
    release.hovered = true;
    release.localMouseX = 790.0f;
    release.localMouseY = 580.0f;
    release.leftReleased = true;
    fixture.step(release);

    // An undo entry that restores the position the entity already had costs the user an undo to
    // reach a change they can actually see.
    CNA_EDITOR_EXPECT_EQ(history.getCount(), before);
}

CNA_EDITOR_TEST(APressAwayFromEveryHandleStillSelects)
{
    GizmoFixture fixture = makeGizmoFixture();

    UiImageInteraction press = leftAt(200.0f, 200.0f, true);
    fixture.step(press);

    UiImageInteraction click;
    click.hovered = true;
    click.localMouseX = 200.0f;
    click.localMouseY = 200.0f;
    click.clicked = true;
    click.leftReleased = true;
    fixture.step(click);

    // Nothing is under (200, 200), so the click clears the selection -- proof the press was not
    // swallowed by the gizmo.
    CNA_EDITOR_EXPECT(!fixture.application->getContext().getPrimarySelection().isValid());
}

CNA_EDITOR_TEST(ShortcutsDriveUndoAndRedo)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(830.0f, 580.0f, false));
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);

    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 100.0f);

    fixture.ui->pressShortcut(UiKey::Y, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);
}

CNA_EDITOR_TEST(DeleteRemovesTheSelectionAndClearsIt)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();
    const std::size_t before = context.getScene().getEntityCount();

    fixture.ui->pressShortcut(UiKey::Delete);
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), before - 1);

    // The selection must not be left pointing at an entity that is gone: the inspector would go on
    // showing it, and the next command would target a missing id.
    CNA_EDITOR_EXPECT(!context.getPrimarySelection().isValid());

    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), before);
}

CNA_EDITOR_TEST(DuplicateCopiesTheSubtreeWithFreshIdsAndSelectsTheCopy)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    // Give the entity a child, so the duplicate has a subtree to get wrong.
    EditorEntity child{Uuid::generate(), "Weapon"};
    EditorComponent transform{BuiltinComponentIds::kTransform};
    transform.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kTransform));
    child.addComponent(std::move(transform));
    child.setParentId(fixture.entityId);
    const Uuid childId = context.getScene().addEntity(std::move(child));

    const std::size_t before = context.getScene().getEntityCount();

    fixture.ui->pressShortcut(UiKey::D, withControl());
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), before + 2);

    const Uuid copyId = context.getPrimarySelection();
    CNA_EDITOR_EXPECT(copyId.isValid());
    CNA_EDITOR_EXPECT(copyId != fixture.entityId);

    // The copied root is a sibling of the original, not its child.
    const EditorEntity* copy = context.getScene().findEntity(copyId);
    CNA_EDITOR_EXPECT_EQ(copy->getParentId().toString(), Uuid{}.toString());
    CNA_EDITOR_EXPECT_EQ(copy->getName(), std::string{"Player Copy"});

    // The copied child hangs off the *copy*, not off the original -- a remapping mistake here is
    // invisible until the user moves one of them and both jump.
    const std::vector<Uuid> copiedChildren = context.getScene().getChildren(copyId);
    CNA_EDITOR_EXPECT_EQ(copiedChildren.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(copiedChildren.front() != childId);
    CNA_EDITOR_EXPECT_EQ(context.getScene().getChildren(fixture.entityId).size(), std::size_t{1});

    // One undo entry for the whole subtree.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), before);
}

CNA_EDITOR_TEST(FrameSelectedBringsTheSelectionIntoView)
{
    GizmoFixture fixture = makeGizmoFixture();

    // Look somewhere far away, so framing has real work to do.
    EditorCamera2D& camera = fixture.application->getViewport().getCamera();
    camera.setCenter(EditorVector2{9000.0f, 9000.0f});

    fixture.ui->pressShortcut(UiKey::F);
    fixture.step(UiImageInteraction{});

    // The entity has no sprite, so there are no bounds to fit -- framing must still centre on it
    // rather than quietly do nothing.
    CNA_EDITOR_EXPECT_EQ(camera.getCenter().x, 100.0f);
    CNA_EDITOR_EXPECT_EQ(camera.getCenter().y, 220.0f);
}

CNA_EDITOR_TEST(GizmoModeShortcutsSwitchTheManipulator)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.ui->pressShortcut(UiKey::E);
    fixture.step(UiImageInteraction{});

    // Rotate has no manipulator yet, so the translate gizmo must stop responding -- a press on
    // where its handle used to be falls through to the picker and clears the selection.
    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(830.0f, 580.0f, false));
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 100.0f);

    fixture.ui->pressShortcut(UiKey::W);
    fixture.step(UiImageInteraction{});

    fixture.step(leftAt(790.0f, 580.0f, true));
    fixture.step(leftAt(830.0f, 580.0f, false));
    CNA_EDITOR_EXPECT_EQ(fixture.getPosition().x, 140.0f);
}

CNA_EDITOR_TEST(AnArmedShortcutFiresExactlyOnce)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();
    const std::size_t before = context.getScene().getEntityCount();

    fixture.ui->pressShortcut(UiKey::D, withControl());
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    // Three frames, one duplicate. A shortcut that stayed armed would fill the scene.
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), before + 1);
}

CNA_EDITOR_TEST(AddingAComponentThroughTheInspectorIsUndoable)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Camera");
    fixture.step(UiImageInteraction{});

    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});

    const EditorEntity* entity = context.getScene().findEntity(fixture.entityId);
    CNA_EDITOR_EXPECT(entity->findComponent(BuiltinComponentIds::kCamera) != nullptr);

    // Every edit goes through a command, so this one undoes like any other (D-06).
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(context.getScene().findEntity(fixture.entityId)
                          ->findComponent(BuiltinComponentIds::kCamera) == nullptr);
}

CNA_EDITOR_TEST(TheAddPickerDropsAUniqueComponentTheEntityAlreadyHas)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(UiImageInteraction{});
    const std::vector<std::string> before = fixture.ui->optionsFor("##addComponentType");

    // The transform is unique and already present, so it must never have been on offer.
    CNA_EDITOR_EXPECT(std::find(before.begin(), before.end(), "Transform") == before.end());
    CNA_EDITOR_EXPECT(std::find(before.begin(), before.end(), "Rendering / Camera") != before.end());

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Camera");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    // Offering it again would be offering an entry AddComponentCommand refuses.
    const std::vector<std::string> after = fixture.ui->optionsFor("##addComponentType");
    CNA_EDITOR_EXPECT(std::find(after.begin(), after.end(), "Rendering / Camera") == after.end());
}

CNA_EDITOR_TEST(ARequiredComponentGetsNoRemoveButton)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(UiImageInteraction{});

    // The entity has only its transform, which is required: removing it would leave the entity
    // with no position at all, so the button is absent rather than present and dead.
    CNA_EDITOR_EXPECT(!fixture.ui->sawButton("Remove##0"));

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Camera");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    // The camera is not required, so it gets one -- at its own index.
    CNA_EDITOR_EXPECT(!fixture.ui->sawButton("Remove##0"));
    CNA_EDITOR_EXPECT(fixture.ui->sawButton("Remove##1"));
}

CNA_EDITOR_TEST(RemovingAComponentThroughTheInspectorIsUndoable)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Camera");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});

    fixture.ui->pendingClicks.push_back("Remove##1");
    fixture.step(UiImageInteraction{});

    const EditorEntity* entity = context.getScene().findEntity(fixture.entityId);
    CNA_EDITOR_EXPECT(entity->findComponent(BuiltinComponentIds::kCamera) == nullptr);

    // The transform survived: removing index 1 must not have taken index 0 with it.
    CNA_EDITOR_EXPECT(entity->findComponent(BuiltinComponentIds::kTransform) != nullptr);

    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(context.getScene().findEntity(fixture.entityId)
                          ->findComponent(BuiltinComponentIds::kCamera) != nullptr);
}

CNA_EDITOR_TEST(RotationIsEditedAsDegreesAndStoredAsAQuaternion)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingEdits.emplace_back("Rotation (deg)",
                                          PropertyValue{EditorVector3{0.0f, 0.0f, 45.0f}});
    fixture.step(UiImageInteraction{});

    const EditorComponent* transform = context.getScene().findEntity(fixture.entityId)
                                           ->findComponent(BuiltinComponentIds::kTransform);
    const EditorQuaternion stored = transform->getProperty("rotation").get<EditorQuaternion>();

    // The scene keeps a quaternion; only the inspector deals in degrees. Storing angles would put
    // the convention in the file, where every reader would have to agree with it forever.
    CNA_EDITOR_EXPECT(stored == quaternionFromEulerDegrees(EditorVector3{0.0f, 0.0f, 45.0f}));

    fixture.step(UiImageInteraction{});
    const EditorVector3 shown = fixture.ui->shownValueFor("Rotation (deg)").get<EditorVector3>();
    CNA_EDITOR_EXPECT_EQ(shown.z, 45.0f);
}

CNA_EDITOR_TEST(TheInspectorKeepsTheAnglesTheUserTypedAtGimbalLock)
{
    GizmoFixture fixture = makeGizmoFixture();

    // A pitch of 90 degrees is a pole: yaw and roll are no longer separable, and reading the
    // quaternion back reports the same rotation as a *different* (yaw, roll) pair.
    const EditorVector3 typed{90.0f, 40.0f, 25.0f};
    fixture.ui->pendingEdits.emplace_back("Rotation (deg)", PropertyValue{typed});
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    const EditorVector3 shown = fixture.ui->shownValueFor("Rotation (deg)").get<EditorVector3>();

    // Recomputing from the quaternion would show roll 0 and a folded yaw, so the two fields beside
    // the one being edited would jump the instant the pitch reached 90.
    CNA_EDITOR_EXPECT_EQ(shown.y, 40.0f);
    CNA_EDITOR_EXPECT_EQ(shown.z, 25.0f);

    // And the honest reading really does differ, which is what makes the cache worth having.
    CNA_EDITOR_EXPECT(eulerDegreesOf(quaternionFromEulerDegrees(typed)).z != 25.0f);
}

CNA_EDITOR_TEST(TheAngleCacheStopsApplyingOnceSomethingElseChangesTheRotation)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingEdits.emplace_back("Rotation (deg)",
                                          PropertyValue{EditorVector3{90.0f, 40.0f, 25.0f}});
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(fixture.ui->shownValueFor("Rotation (deg)").get<EditorVector3>().z, 25.0f);

    // Undo puts back a rotation the cache did not produce, so the cache must stop applying at once
    // -- otherwise the inspector would go on showing angles for a value the scene no longer holds.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    const EditorComponent* transform = context.getScene().findEntity(fixture.entityId)
                                           ->findComponent(BuiltinComponentIds::kTransform);
    const EditorQuaternion stored = transform->getProperty("rotation").get<EditorQuaternion>();

    const EditorVector3 shown = fixture.ui->shownValueFor("Rotation (deg)").get<EditorVector3>();
    const EditorVector3 honest = eulerDegreesOf(stored);
    CNA_EDITOR_EXPECT_EQ(shown.x, honest.x);
    CNA_EDITOR_EXPECT_EQ(shown.y, honest.y);
    CNA_EDITOR_EXPECT_EQ(shown.z, honest.z);
}

namespace
{
    /** @brief Adds a child entity named @p name under @p parentId and returns its id. */
    Uuid addChildEntity(EditorContext& context, const Uuid& parentId, const std::string& name)
    {
        EditorEntity entity{Uuid::generate(), name};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        transform.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kTransform));
        entity.addComponent(std::move(transform));
        entity.setParentId(parentId);
        return context.getScene().addEntity(std::move(entity));
    }
}

CNA_EDITOR_TEST(DoubleClickingAHierarchyNodeRenamesItInPlace)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingNodeDoubleClicks.push_back(fixture.entityId);
    fixture.step(UiImageInteraction{});

    // The row is now a text field rather than a tree node.
    fixture.ui->pendingRename = std::make_pair(std::string{"Hero"}, true);
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(fixture.ui->sawRenameField);

    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(fixture.entityId)->getName(),
                         std::string{"Hero"});

    // And the field is gone once the edit commits.
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(!fixture.ui->sawRenameField);

    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(fixture.entityId)->getName(),
                         std::string{"Player"});
}

CNA_EDITOR_TEST(AnEmptyRenameIsTreatedAsASlipAndKeepsTheOldName)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();
    const std::size_t before = context.getHistory().getCount();

    fixture.ui->pressShortcut(UiKey::F2);
    fixture.step(UiImageInteraction{});

    fixture.ui->pendingRename = std::make_pair(std::string{}, true);
    fixture.step(UiImageInteraction{});

    // An unnamed row in the hierarchy is unusable, and the old name is still right there to keep.
    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(fixture.entityId)->getName(),
                         std::string{"Player"});
    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCount(), before);
}

CNA_EDITOR_TEST(DraggingAnEntityOntoAnotherReparentsIt)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid otherId = addChildEntity(context, Uuid{}, "Crate");

    // Drop the Player onto the Crate.
    fixture.ui->pendingDrops.emplace_back(otherId, fixture.entityId.toString());
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(fixture.entityId)->getParentId().toString(),
                         otherId.toString());

    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(!context.getScene().findEntity(fixture.entityId)->getParentId().isValid());
}

CNA_EDITOR_TEST(DroppingAParentOntoItsOwnChildIsRefusedWithoutAnUndoEntry)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid childId = addChildEntity(context, fixture.entityId, "Weapon");
    const std::size_t before = context.getHistory().getCount();

    // Dropping the parent onto its own child would make a cycle. SceneDocument would refuse it
    // anyway, but the command would still land in the undo stack having done nothing.
    fixture.ui->pendingDrops.emplace_back(childId, fixture.entityId.toString());
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT(!context.getScene().findEntity(fixture.entityId)->getParentId().isValid());
    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(childId)->getParentId().toString(),
                         fixture.entityId.toString());
    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCount(), before);
    CNA_EDITOR_EXPECT(fixture.logContains("under one of its own children"));
}

CNA_EDITOR_TEST(DroppingAnEntityOntoItselfDoesNothing)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();
    const std::size_t before = context.getHistory().getCount();

    fixture.ui->pendingDrops.emplace_back(fixture.entityId, fixture.entityId.toString());
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCount(), before);
}

CNA_EDITOR_TEST(CtrlClickExtendsTheHierarchySelection)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid otherId = addChildEntity(context, Uuid{}, "Crate");

    fixture.ui->modifiers = withControl();
    fixture.ui->pendingNodeClicks.push_back(otherId);
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getSelection().size(), std::size_t{2});
    CNA_EDITOR_EXPECT(context.isSelected(fixture.entityId));
    CNA_EDITOR_EXPECT(context.isSelected(otherId));

    // Without the modifier a click replaces the selection, which is the ordinary case.
    fixture.ui->modifiers = UiKeyModifiers{};
    fixture.ui->pendingNodeClicks.push_back(otherId);
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getSelection().size(), std::size_t{1});
    CNA_EDITOR_EXPECT(context.isSelected(otherId));
}

CNA_EDITOR_TEST(TheHierarchyKeepsDrawingWhileAReparentIsPending)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid otherId = addChildEntity(context, Uuid{}, "Crate");
    const Uuid childId = addChildEntity(context, fixture.entityId, "Weapon");

    fixture.ui->pendingDrops.emplace_back(otherId, fixture.entityId.toString());
    fixture.step(UiImageInteraction{});

    // The reparent runs after the tree has finished drawing, so the traversal never walked a
    // child list that was being reordered underneath it. Every entity is still reachable.
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT_EQ(fixture.ui->drawnNodes.size(), context.getScene().getEntityCount());
    CNA_EDITOR_EXPECT_EQ(context.getScene().findEntity(childId)->getParentId().toString(),
                         fixture.entityId.toString());
}

namespace
{
    /** @brief Registers an asset of @p type at @p path and returns its id. */
    Uuid addAsset(EditorContext& context, const std::string& path, AssetType type)
    {
        AssetRecord record;
        record.id = Uuid::generate();
        record.sourcePath = path;
        record.type = type;

        const Uuid id = record.id;
        context.getAssets().add(std::move(record));
        return id;
    }
}

CNA_EDITOR_TEST(DroppingATextureOntoASpriteSlotSetsIt)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Sprite Renderer");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});

    const Uuid textureId = addAsset(context, "Textures/Player.png", AssetType::Texture2D);

    fixture.ui->pendingAssetDrops.emplace_back("Texture", textureId.toString());
    fixture.step(UiImageInteraction{});

    const EditorComponent* sprite = context.getScene().findEntity(fixture.entityId)
                                        ->findComponent(BuiltinComponentIds::kSpriteRenderer);
    CNA_EDITOR_EXPECT_EQ(sprite->getProperty("texture").get<PropertyValue::AssetReference>().id.toString(),
                         textureId.toString());

    // Setting a reference by drop is an edit like any other.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(!context.getScene().findEntity(fixture.entityId)
                           ->findComponent(BuiltinComponentIds::kSpriteRenderer)
                           ->getProperty("texture").get<PropertyValue::AssetReference>().id.isValid());
}

CNA_EDITOR_TEST(ASlotRefusesAnAssetOfTheWrongKindAndSaysWhy)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering / Sprite Renderer");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});

    const Uuid soundId = addAsset(context, "Audio/Jump.wav", AssetType::SoundEffect);

    fixture.ui->pendingAssetDrops.emplace_back("Texture", soundId.toString());
    fixture.step(UiImageInteraction{});

    // Accepting it would give a scene that loads and a sprite that never appears, with nothing
    // anywhere to explain why.
    const EditorComponent* sprite = context.getScene().findEntity(fixture.entityId)
                                        ->findComponent(BuiltinComponentIds::kSpriteRenderer);
    CNA_EDITOR_EXPECT(!sprite->getProperty("texture").get<PropertyValue::AssetReference>().id.isValid());
    CNA_EDITOR_EXPECT(fixture.logContains("this field takes a Texture2D"));
}

CNA_EDITOR_TEST(ASlotWithNoDeclaredKindTakesAnything)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    fixture.ui->pendingChoices.emplace_back("##addComponentType", "Rendering (3D) / Model Renderer");
    fixture.step(UiImageInteraction{});
    fixture.ui->pendingClicks.push_back("Add Component");
    fixture.step(UiImageInteraction{});

    // The material override declares no kind, because materials are not a tracked asset type yet.
    const Uuid rawId = addAsset(context, "Materials/Brick.json", AssetType::RawData);

    fixture.ui->pendingAssetDrops.emplace_back("Material Override", rawId.toString());
    fixture.step(UiImageInteraction{});

    const EditorComponent* model = context.getScene().findEntity(fixture.entityId)
                                       ->findComponent(BuiltinComponentIds::kModelRenderer);
    CNA_EDITOR_EXPECT_EQ(model->getProperty("material").get<PropertyValue::AssetReference>().id.toString(),
                         rawId.toString());
}

CNA_EDITOR_TEST(TheConsoleCopiesWhatItIsShowing)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    context.log(LogSeverity::Trace, "a trace line");
    context.log(LogSeverity::Warning, "a warning line");
    context.log(LogSeverity::Error, "an error line");

    fixture.ui->pendingClicks.push_back("Copy");
    fixture.step(UiImageInteraction{});

    // Unfiltered, Copy takes everything.
    CNA_EDITOR_EXPECT(fixture.ui->getClipboardText().find("a trace line") != std::string::npos);
    CNA_EDITOR_EXPECT(fixture.ui->getClipboardText().find("an error line") != std::string::npos);

    fixture.ui->pendingChoices.emplace_back("##consoleSeverity", "Warning");
    fixture.step(UiImageInteraction{});

    fixture.ui->pendingClicks.push_back("Copy");
    fixture.step(UiImageInteraction{});

    // Filtered, it takes what the user can see. Copying hidden messages would be a surprise.
    CNA_EDITOR_EXPECT(fixture.ui->getClipboardText().find("a trace line") == std::string::npos);
    CNA_EDITOR_EXPECT(fixture.ui->getClipboardText().find("a warning line") != std::string::npos);
    CNA_EDITOR_EXPECT(fixture.ui->getClipboardText().find("an error line") != std::string::npos);
}

CNA_EDITOR_TEST(TheConsoleClearButtonEmptiesIt)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    context.log(LogSeverity::Info, "something happened");
    CNA_EDITOR_EXPECT(!fixture.ui->getLog().empty());

    fixture.ui->pendingClicks.push_back("Clear");
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT(fixture.ui->getLog().empty());
}

CNA_EDITOR_TEST(TheConsoleScrollLockIsRememberedAcrossFrames)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(fixture.ui->shownCheckFor("Auto-scroll"));

    fixture.ui->pendingChecks.emplace_back("Auto-scroll", false);
    fixture.step(UiImageInteraction{});

    // The setting is the console's own state, so it has to survive frames that do not touch it.
    // A checkbox that reset every frame would be a scroll-lock that never locks.
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(!fixture.ui->shownCheckFor("Auto-scroll"));

    fixture.ui->pendingChecks.emplace_back("Auto-scroll", true);
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(fixture.ui->shownCheckFor("Auto-scroll"));
}

CNA_EDITOR_TEST(TheConsoleSeverityFilterIsRememberedAcrossFrames)
{
    GizmoFixture fixture = makeGizmoFixture();

    fixture.ui->pendingChoices.emplace_back("##consoleSeverity", "Error");
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(fixture.ui->shownValueFor("##consoleSeverity")
                             .get<PropertyValue::EnumValue>().name,
                         std::string{"Error"});
}
