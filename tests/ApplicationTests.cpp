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
#include "CNA/Editor/Assets/AssetImporters.hpp"
#include "CNA/Editor/Project/RecoveryStore.hpp"
#include "CNA/Editor/Scene/MissingReferences.hpp"
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
    CNA_EDITOR_EXPECT_EQ(panels.size(), std::size_t{7});

    const auto contains = [&](const std::string& title) {
        return std::find(panels.begin(), panels.end(), title) != panels.end();
    };
    CNA_EDITOR_EXPECT(contains("Scene Hierarchy"));
    CNA_EDITOR_EXPECT(contains("Viewport"));
    CNA_EDITOR_EXPECT(contains("Inspector"));
    CNA_EDITOR_EXPECT(contains("Assets"));
    CNA_EDITOR_EXPECT(contains("Console"));
    CNA_EDITOR_EXPECT(contains("Validation"));
    CNA_EDITOR_EXPECT(contains("History"));

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
            drawnText.clear();
            drawnTextFields.clear();
            drawnNodes.clear();
            drawnStringNodes.clear();
            drawnStringNodeLabels.clear();
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

        /** @brief Folder rows the browser drew, and drops to deliver to them. */
        std::vector<std::string> drawnStringNodes;
        std::vector<std::pair<std::string, std::string>> pendingFolderDrops;
        std::string lastStringNode;

        /** @brief Labels of the string-keyed rows, in draw order, for asserting on wording. */
        std::vector<std::string> drawnStringNodeLabels;

        /** @brief String-keyed rows to report as clicked, matched on an id prefix. */
        std::vector<std::string> pendingStringNodeClicks;

        UiTreeNodeResult treeNode(const std::string& id,
                                  const std::string& label,
                                  bool selected,
                                  bool leaf) override
        {
            drawnStringNodes.push_back(id);
            drawnStringNodeLabels.push_back(label);
            lastStringNode = id;

            UiTreeNodeResult result = NullEditorUi::treeNode(id, label, selected, leaf);

            // Matched on a prefix rather than in full: a validation row carries its position in
            // the report as part of its id, which a test should not have to predict.
            for (auto entry = pendingStringNodeClicks.begin();
                 entry != pendingStringNodeClicks.end(); ++entry)
            {
                if (id.rfind(*entry, 0) != 0) { continue; }
                pendingStringNodeClicks.erase(entry);
                result.clicked = true;
                break;
            }
            return result;
        }

        /** @brief Returns true when a string-keyed node with @p id was drawn last frame. */
        [[nodiscard]] bool sawStringNode(const std::string& id) const
        {
            return std::find(drawnStringNodes.begin(), drawnStringNodes.end(), id)
                != drawnStringNodes.end();
        }

        std::optional<std::string> acceptDrop(const std::string& type) override
        {
            if (type == "asset")
            {
                for (auto entry = pendingFolderDrops.begin(); entry != pendingFolderDrops.end(); ++entry)
                {
                    if (entry->first != lastStringNode) { continue; }
                    const std::string payload = entry->second;
                    pendingFolderDrops.erase(entry);
                    return payload;
                }
            }

            if (type == "asset")
            {
                // Matched against the last field *or* the last tree node: an asset slot in the
                // inspector is a labelled field, a row in the missing-references report is a node.
                for (auto entry = pendingAssetDrops.begin(); entry != pendingAssetDrops.end(); ++entry)
                {
                    if (entry->first != lastField && entry->first != lastNode.toString()) { continue; }
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
            (void)takeFocus;
            drawnTextFields.push_back(id);

            // Only a rename field takes the scripted text. The asset browser's filter is an
            // inputText too, and typing a scripted name into it would silently make the panel
            // show nothing while the test believed it had renamed something.
            const bool isRename = id.rfind("##rename-", 0) == 0;
            if (isRename) { sawRenameField = true; }

            for (auto entry = pendingFieldEdits.begin(); entry != pendingFieldEdits.end(); ++entry)
            {
                if (entry->first != id) { continue; }
                text = entry->second;
                pendingFieldEdits.erase(entry);
                return UiTextFieldResult{true, true};
            }

            if (!isRename || !pendingRename) { return UiTextFieldResult{}; }

            UiTextFieldResult result;
            text = pendingRename->first;
            result.changed = true;
            result.committed = pendingRename->second;
            if (result.committed) { pendingRename.reset(); }
            return result;
        }

        /** @brief Ids of text fields drawn during the most recent frame. */
        std::vector<std::string> drawnTextFields;

        /** @brief Text to type into the next field whose id is @p id, committing it. */
        void typeInto(const std::string& id, std::string text)
        {
            pendingFieldEdits.emplace_back(id, std::move(text));
        }

        /** @brief Scripted edits for fields other than renames, by id. */
        std::vector<std::pair<std::string, std::string>> pendingFieldEdits;

        void text(const std::string& value) override { drawnText.push_back(value); }

        /** @brief Text drawn during the most recent frame. */
        std::vector<std::string> drawnText;

        /** @brief Returns true when @p value was drawn last frame. */
        [[nodiscard]] bool sawText(const std::string& value) const
        {
            return std::find(drawnText.begin(), drawnText.end(), value) != drawnText.end();
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

CNA_EDITOR_TEST(RelinkingFromTheReportFixesEveryReferenceAtOnce)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid goneId = Uuid::generate();
    const Uuid replacementId = addAsset(context, "Textures/Correct.png", AssetType::Texture2D);

    // Two entities pointing at the same missing texture -- the ordinary shape of the problem.
    for (int index = 0; index < 2; ++index)
    {
        EditorEntity entity{Uuid::generate(), "Broken" + std::to_string(index)};
        EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
        sprite.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kSpriteRenderer));
        sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{goneId}});
        entity.addComponent(std::move(sprite));
        context.getScene().addEntity(std::move(entity));
    }

    CNA_EDITOR_EXPECT_EQ(findMissingReferences(context.getScene(), context.getAssets()).size(),
                         std::size_t{2});

    fixture.ui->pendingAssetDrops.emplace_back(goneId.toString(), replacementId.toString());
    fixture.step(UiImageInteraction{});

    // Both now point at the replacement. Asserting on the stored ids rather than on the report
    // going empty: the scratch asset has no file behind it, so it would be reported as missing for
    // an entirely different reason and the test would be checking the wrong thing.
    const auto texturesInScene = [&] {
        std::vector<std::string> ids;
        for (const EditorEntity& entity : context.getScene().getEntities())
        {
            if (const EditorComponent* sprite = entity.findComponent(BuiltinComponentIds::kSpriteRenderer))
            {
                ids.push_back(sprite->getProperty("texture").get<PropertyValue::AssetReference>().id.toString());
            }
        }
        return ids;
    };

    CNA_EDITOR_EXPECT_EQ(texturesInScene().size(), std::size_t{2});
    for (const std::string& id : texturesInScene()) { CNA_EDITOR_EXPECT_EQ(id, replacementId.toString()); }

    // One undo entry for the whole relink: undoing a fix of forty sprites must not be forty
    // presses of Ctrl+Z.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    for (const std::string& id : texturesInScene()) { CNA_EDITOR_EXPECT_EQ(id, goneId.toString()); }
}

CNA_EDITOR_TEST(TheReportSaysSoWhenNothingIsBroken)
{
    GizmoFixture fixture = makeGizmoFixture();
    fixture.step(UiImageInteraction{});

    // An empty panel reads as "not implemented yet", which is the wrong thing for a report whose
    // good state is emptiness.
    CNA_EDITOR_EXPECT(fixture.ui->sawText("No broken asset references."));
}

CNA_EDITOR_TEST(SelectingAnAssetSwitchesTheInspectorToItsImportSettings)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid textureId = addAsset(context, "Textures/Hero.png", AssetType::Texture2D);
    context.getAssets().findMutable(textureId)->importerId = ImporterIds::kTexture;

    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(fixture.ui->sawText("Entity: Player"));

    fixture.ui->pendingNodeClicks.push_back(textureId);
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});

    // One inspector showing one thing. Two independent selections would leave the user unable to
    // tell which the panel is about.
    CNA_EDITOR_EXPECT(fixture.ui->sawText("Asset: Textures/Hero.png"));
    CNA_EDITOR_EXPECT(!fixture.ui->sawText("Entity: Player"));
    CNA_EDITOR_EXPECT(context.getSelection().empty());

    // The texture importer's declared settings are what it offers.
    CNA_EDITOR_EXPECT_EQ(fixture.ui->optionsFor("Wrap Mode").size(), std::size_t{3});

    // And selecting an entity again takes the inspector back.
    fixture.ui->pendingNodeClicks.push_back(fixture.entityId);
    fixture.step(UiImageInteraction{});
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(fixture.ui->sawText("Entity: Player"));
    CNA_EDITOR_EXPECT(!context.getSelectedAsset().isValid());
}

CNA_EDITOR_TEST(EditingAnImportSettingGoesThroughTheUndoStack)
{
    GizmoFixture fixture = makeGizmoFixture();
    EditorContext& context = fixture.application->getContext();

    const Uuid textureId = addAsset(context, "Textures/Hero.png", AssetType::Texture2D);
    context.getAssets().findMutable(textureId)->importerId = ImporterIds::kTexture;
    context.selectAsset(textureId);

    fixture.ui->pendingEdits.emplace_back("Filter Mode",
                                          PropertyValue{PropertyValue::EnumValue{"Point"}});
    fixture.step(UiImageInteraction{});

    CNA_EDITOR_EXPECT_EQ(context.getAssets().find(textureId)->importerSettings["filterMode"].asString(),
                         std::string{"Point"});

    // The asset database is a document like the scene, so its edits undo like the scene's do --
    // an editor where some edits undo and others quietly do not is worse than one where none do.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step(UiImageInteraction{});
    CNA_EDITOR_EXPECT(context.getAssets().find(textureId)->importerSettings["filterMode"].isNull());
}

CNA_EDITOR_TEST(AnExternallyEditedAssetIsReloadedAndReported)
{
    const std::filesystem::path directory = makeScratchDirectory("watchapp");
    writeFile(directory / "HelloSprites.cnaproject",
              R"({"formatVersion":1,"name":"Watched","kind":"CnaNative","assetDirectory":"Assets"})");
    writeFile(directory / "Assets" / "Textures" / "Hero.png", "first contents");

    EditorApplication application{std::make_unique<NullEditorUi>(),
                                 std::make_unique<NullEditorViewport>()};
    EditorOptions options;
    options.headless = true;
    options.projectPath = (directory / "HelloSprites.cnaproject").generic_string();
    application.initialize(options);

    EditorContext& context = application.getContext();
    CNA_EDITOR_EXPECT_EQ(context.getAssets().getCount(), std::size_t{1});
    const std::string path = context.getAssets().getAll().front()->sourcePath;

    application.getAssetWatcher().setInterval(0.0);

    writeFile(directory / "Assets" / "Textures" / "Hero.png",
              "second contents, a different length entirely");

    application.renderFrame(0.0);

    const auto& ui = static_cast<NullEditorUi&>(application.getUi());
    bool reported = false;
    for (const auto& entry : ui.getLog())
    {
        if (entry.message.find("Reloaded '" + path + "'") != std::string::npos) { reported = true; }
    }

    // Without this the editor goes on showing the art from before the edit, and the only fix is
    // to restart it.
    CNA_EDITOR_EXPECT(reported);

    std::filesystem::remove_all(directory);
}

namespace
{
    /** @brief An editor over a real on-disk project with a few tracked assets. */
    struct BrowserFixture
    {
        std::filesystem::path directory;
        std::unique_ptr<EditorApplication> application;
        ScriptedUi* ui = nullptr;

        explicit BrowserFixture(const std::string& name)
        {
            directory = makeScratchDirectory(name);
            writeFile(directory / "Game.cnaproject",
                      R"({"formatVersion":1,"name":"Browsed","kind":"CnaNative","assetDirectory":"Assets"})");
            writeFile(directory / "Assets" / "Textures" / "hero.png", "hero");
            writeFile(directory / "Assets" / "Textures" / "enemy.png", "enemy");
            writeFile(directory / "Assets" / "Audio" / "jump.wav", "jump");

            auto scripted = std::make_unique<ScriptedUi>();
            ui = scripted.get();
            application = std::make_unique<EditorApplication>(std::move(scripted),
                                                             std::make_unique<NullEditorViewport>());

            EditorOptions options;
            options.headless = true;
            options.projectPath = (directory / "Game.cnaproject").generic_string();
            application->initialize(options);
        }

        ~BrowserFixture()
        {
            std::error_code errorCode;
            std::filesystem::remove_all(directory, errorCode);
        }

        BrowserFixture(const BrowserFixture&) = delete;
        BrowserFixture& operator=(const BrowserFixture&) = delete;

        [[nodiscard]] EditorContext& context() { return application->getContext(); }

        void step() { application->renderFrame(0.0); }

        [[nodiscard]] Uuid idOf(const std::string& path) const
        {
            const AssetRecord* record =
                const_cast<BrowserFixture*>(this)->context().getAssets().findByPath(path);
            return record != nullptr ? record->id : Uuid{};
        }
    };
}

CNA_EDITOR_TEST(TheAssetBrowserShowsAFolderTree)
{
    BrowserFixture fixture{"browsertree"};
    fixture.step();

    CNA_EDITOR_EXPECT_EQ(fixture.context().getAssets().getCount(), std::size_t{3});
    CNA_EDITOR_EXPECT(fixture.ui->sawStringNode("folder:Assets"));
    CNA_EDITOR_EXPECT(fixture.ui->sawStringNode("folder:Assets/Textures"));
    CNA_EDITOR_EXPECT(fixture.ui->sawStringNode("folder:Assets/Audio"));
    CNA_EDITOR_EXPECT(fixture.ui->sawText("3 assets"));
}

CNA_EDITOR_TEST(TheAssetBrowserFilterHidesWhatDoesNotMatch)
{
    BrowserFixture fixture{"browserfilter"};

    fixture.ui->typeInto("##assetFilter", "audio");
    fixture.step();
    fixture.step();

    // Only the matching branch survives; a filtered tree never shows an empty folder.
    CNA_EDITOR_EXPECT(fixture.ui->sawStringNode("folder:Assets/Audio"));
    CNA_EDITOR_EXPECT(!fixture.ui->sawStringNode("folder:Assets/Textures"));
    CNA_EDITOR_EXPECT(fixture.ui->sawText("1 of 3 assets"));

    // A filter matching nothing says so rather than looking like a broken panel.
    fixture.ui->typeInto("##assetFilter", "zzz");
    fixture.step();
    fixture.step();
    CNA_EDITOR_EXPECT(fixture.ui->sawText("Nothing matches 'zzz'."));
}

CNA_EDITOR_TEST(DroppingAnAssetOnAFolderMovesTheFileAndNotAnyScene)
{
    BrowserFixture fixture{"browsermove"};
    EditorContext& context = fixture.context();

    const Uuid heroId = fixture.idOf("Assets/Textures/hero.png");
    CNA_EDITOR_EXPECT(heroId.isValid());

    // Point a scene at it, then move it. The reference is a Uuid, so it must survive untouched.
    EditorEntity entity{Uuid::generate(), "Player"};
    EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
    sprite.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kSpriteRenderer));
    sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{heroId}});
    entity.addComponent(std::move(sprite));
    context.getScene().addEntity(std::move(entity));

    fixture.step();
    fixture.ui->pendingFolderDrops.emplace_back("folder:Assets/Audio", heroId.toString());
    fixture.step();

    CNA_EDITOR_EXPECT_EQ(context.getAssets().find(heroId)->sourcePath,
                         std::string{"Assets/Audio/hero.png"});
    CNA_EDITOR_EXPECT(std::filesystem::exists(fixture.directory / "Assets" / "Audio" / "hero.png"));

    // The id never changed, so nothing referencing it broke (D-08).
    CNA_EDITOR_EXPECT(findMissingReferences(context.getScene(), context.getAssets()).empty());

    // And it undoes like any other document change.
    fixture.ui->pressShortcut(UiKey::Z, withControl());
    fixture.step();
    CNA_EDITOR_EXPECT_EQ(context.getAssets().find(heroId)->sourcePath,
                         std::string{"Assets/Textures/hero.png"});
}

CNA_EDITOR_TEST(RenamingAnAssetKeepsItInItsFolder)
{
    BrowserFixture fixture{"browserrename"};
    EditorContext& context = fixture.context();

    const Uuid heroId = fixture.idOf("Assets/Textures/hero.png");

    fixture.ui->pendingNodeDoubleClicks.push_back(heroId);
    fixture.step();

    fixture.ui->pendingRename = std::make_pair(std::string{"champion.png"}, true);
    fixture.step();

    CNA_EDITOR_EXPECT_EQ(context.getAssets().find(heroId)->sourcePath,
                         std::string{"Assets/Textures/champion.png"});
    CNA_EDITOR_EXPECT(std::filesystem::exists(fixture.directory / "Assets" / "Textures" / "champion.png"));
    CNA_EDITOR_EXPECT(std::filesystem::exists(fixture.directory / "Assets" / "Textures" / "champion.png.cnaasset"));
}

CNA_EDITOR_TEST(ARenameContainingASeparatorIsRefusedWithAReason)
{
    BrowserFixture fixture{"browserrenamepath"};
    EditorContext& context = fixture.context();

    const Uuid heroId = fixture.idOf("Assets/Textures/hero.png");

    fixture.ui->pendingNodeDoubleClicks.push_back(heroId);
    fixture.step();

    fixture.ui->pendingRename = std::make_pair(std::string{"../escaped.png"}, true);
    fixture.step();

    // A name with a separator in it is a move disguised as a rename, and the folder drop already
    // does that unambiguously.
    CNA_EDITOR_EXPECT_EQ(context.getAssets().find(heroId)->sourcePath,
                         std::string{"Assets/Textures/hero.png"});

    bool explained = false;
    for (const auto& entry : fixture.ui->getLog())
    {
        if (entry.message.find("cannot contain a path separator") != std::string::npos)
        {
            explained = true;
        }
    }
    CNA_EDITOR_EXPECT(explained);
}

CNA_EDITOR_TEST(TheValidationPanelReportsAnIssueAndSelectsItsEntity)
{
    auto scripted = std::make_unique<ScriptedUi>();
    ScriptedUi* ui = scripted.get();
    EditorApplication application{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();

    // The starting scene already holds a primary camera, so a second one is a conflict rather
    // than a scene assembled to produce a warning.
    EditorEntity second{Uuid::generate(), "Cutscene Camera"};
    EditorComponent transform{BuiltinComponentIds::kTransform};
    transform.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kTransform));
    second.addComponent(std::move(transform));
    EditorComponent camera{BuiltinComponentIds::kCamera};
    camera.applyDefaults(*context.getComponentRegistry().find(BuiltinComponentIds::kCamera));
    second.addComponent(std::move(camera));
    const Uuid secondId = context.getScene().addEntity(std::move(second));

    application.renderFrame();

    CNA_EDITOR_EXPECT(ui->sawText("2 error(s), 0 warning(s)"));

    const auto sawIssueLabel = [&](std::string_view needle) {
        for (const std::string& label : ui->drawnStringNodeLabels)
        {
            if (label.find(needle) != std::string::npos) { return true; }
        }
        return false;
    };
    CNA_EDITOR_EXPECT(sawIssueLabel("[error] Cutscene Camera: Marked primary"));

    // Clicking the row is the whole point of reporting one: finding which of two hundred entities
    // carries the fault is the hunt the panel exists to prevent.
    context.clearSelection();
    ui->pendingStringNodeClicks.push_back("issue-1-duplicate-primary-camera");
    application.renderFrame();

    CNA_EDITOR_EXPECT(context.getPrimarySelection() == secondId);

    // Removing the second camera empties the report rather than leaving a stale row behind.
    context.execute(std::make_unique<DeleteEntityCommand>(context.getScene(), secondId));
    application.renderFrame();

    CNA_EDITOR_EXPECT(ui->sawText("No scene issues."));
    CNA_EDITOR_EXPECT(ui->sawText("No broken asset references."));
}

CNA_EDITOR_TEST(TheHistoryPanelListsEveryEntryIncludingTheUndoneOnes)
{
    auto scripted = std::make_unique<ScriptedUi>();
    ScriptedUi* ui = scripted.get();
    EditorApplication application{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();
    context.getHistory().markSaved();

    for (int index = 0; index < 3; ++index)
    {
        EditorEntity entity{Uuid::generate(), "Entity" + std::to_string(index)};
        entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
        context.execute(std::make_unique<CreateEntityCommand>(context.getScene(), std::move(entity)));
    }

    application.renderFrame();
    CNA_EDITOR_EXPECT(ui->sawText("3 of 3 applied, keeping 512"));

    const auto rowLabel = [&](std::size_t position) {
        const std::string id = "history-" + std::to_string(position);
        for (std::size_t index = 0; index < ui->drawnStringNodes.size(); ++index)
        {
            if (ui->drawnStringNodes[index] == id) { return ui->drawnStringNodeLabels[index]; }
        }
        return std::string{};
    };

    // One row per position rather than per entry, so the state the document was opened in is
    // reachable -- it is the one a user asking to "put it back" is aiming at.
    CNA_EDITOR_EXPECT(rowLabel(0).find("Opened") != std::string::npos);
    CNA_EDITOR_EXPECT(rowLabel(0).find("(saved)") != std::string::npos);
    CNA_EDITOR_EXPECT(rowLabel(3).find("> ") == 0);

    application.undo();
    application.undo();
    application.renderFrame();

    CNA_EDITOR_EXPECT(ui->sawText("1 of 3 applied, keeping 512"));

    // Undone entries stay listed. Hiding them would hide exactly what the user is trying to
    // get back to.
    CNA_EDITOR_EXPECT(rowLabel(1).find("> ") == 0);
    CNA_EDITOR_EXPECT(rowLabel(2).find("(undone)") != std::string::npos);
    CNA_EDITOR_EXPECT(rowLabel(3).find("(undone)") != std::string::npos);
}

CNA_EDITOR_TEST(ClickingAHistoryRowMovesTheCursorToIt)
{
    auto scripted = std::make_unique<ScriptedUi>();
    ScriptedUi* ui = scripted.get();
    EditorApplication application{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    EditorContext& context = application.getContext();
    const std::size_t entitiesBefore = context.getScene().getEntityCount();

    std::vector<Uuid> added;
    for (int index = 0; index < 4; ++index)
    {
        EditorEntity entity{Uuid::generate(), "Entity" + std::to_string(index)};
        entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
        added.push_back(entity.getId());
        context.execute(std::make_unique<CreateEntityCommand>(context.getScene(), std::move(entity)));
    }
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), entitiesBefore + 4);

    // Jumping back four positions in one click is the whole point: fifteen presses of Ctrl+Z with
    // no idea how many are left is how a person loses work they meant to keep.
    ui->pendingStringNodeClicks.push_back("history-1");
    application.renderFrame();

    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCursor(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), entitiesBefore + 1);
    CNA_EDITOR_EXPECT(context.getScene().findEntity(added[0]) != nullptr);
    CNA_EDITOR_EXPECT(context.getScene().findEntity(added[3]) == nullptr);

    // And forward again, through the same rows.
    ui->pendingStringNodeClicks.push_back("history-4");
    application.renderFrame();

    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCursor(), std::size_t{4});
    CNA_EDITOR_EXPECT_EQ(context.getScene().getEntityCount(), entitiesBefore + 4);
    CNA_EDITOR_EXPECT(context.getScene().findEntity(added[3]) != nullptr);

    // Navigating goes through the application's own undo, so a jump prunes the selection the same
    // way Ctrl+Z does -- otherwise the inspector would keep showing an entity that no longer exists.
    context.select(added[3]);
    ui->pendingStringNodeClicks.push_back("history-0");
    application.renderFrame();

    CNA_EDITOR_EXPECT_EQ(context.getHistory().getCursor(), std::size_t{0});
    CNA_EDITOR_EXPECT(context.getSelection().empty());
}

CNA_EDITOR_TEST(AnEmptyHistorySaysSoRatherThanDrawingNothing)
{
    auto scripted = std::make_unique<ScriptedUi>();
    ScriptedUi* ui = scripted.get();
    EditorApplication application{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    CNA_EDITOR_EXPECT(application.initialize(options));

    application.renderFrame();
    CNA_EDITOR_EXPECT(ui->sawText("Nothing to undo yet."));
}

namespace
{
    /** @brief A project on disk, an editor over it, and a scratch directory for its snapshots. */
    struct RecoveryFixture
    {
        std::filesystem::path directory;
        std::unique_ptr<EditorApplication> application;
        ScriptedUi* ui = nullptr;

        explicit RecoveryFixture(const std::string& name, double autosaveSeconds = 1.0)
        {
            directory = makeScratchDirectory(name);
            writeFile(directory / "Game.cnaproject",
                      R"({"formatVersion":1,"name":"Recovered","kind":"CnaNative",)"
                      R"("assetDirectory":"Assets","startupScene":"Scenes/Level01.cnascene"})");
            writeFile(directory / "Scenes" / "Level01.cnascene",
                      R"({"formatVersion":1,"sceneId":"c486b3f0-2a41-4d6b-9f18-7e0c5a1b4d92",)"
                      R"("name":"Level01","entities":[]})");

            auto scripted = std::make_unique<ScriptedUi>();
            ui = scripted.get();
            application = std::make_unique<EditorApplication>(std::move(scripted),
                                                             std::make_unique<NullEditorViewport>());

            EditorOptions options;
            options.headless = true;
            options.autosaveSeconds = autosaveSeconds;
            options.recoveryDirectory = (directory / "recovery").generic_string();
            options.projectPath = (directory / "Game.cnaproject").generic_string();
            application->initialize(options);
        }

        ~RecoveryFixture()
        {
            application.reset();
            std::error_code errorCode;
            std::filesystem::remove_all(directory, errorCode);
        }

        [[nodiscard]] EditorContext& context() { return application->getContext(); }
        [[nodiscard]] RecoveryStore store() const
        {
            return RecoveryStore{(directory / "recovery").generic_string()};
        }
        [[nodiscard]] std::string projectPath() const
        {
            return (directory / "Game.cnaproject").generic_string();
        }

        /** @brief Adds one entity through the undo stack, so the document becomes dirty. */
        Uuid addEntity(const std::string& name)
        {
            EditorEntity entity{Uuid::generate(), name};
            entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
            const Uuid id = entity.getId();
            context().execute(std::make_unique<CreateEntityCommand>(context().getScene(), std::move(entity)));
            return id;
        }
    };
}

CNA_EDITOR_TEST(AnUnsavedSceneIsSnapshottedAndTheSnapshotGoesAwayOnSave)
{
    RecoveryFixture fixture{"autosave"};

    // Nothing to rescue while the document matches its file.
    fixture.application->renderFrame(5.0);
    CNA_EDITOR_EXPECT(!fixture.store().findForProject(fixture.projectPath()).has_value());

    fixture.addEntity("Player");

    // Below the interval: the clock is passed in, so this is exact rather than a race.
    fixture.application->renderFrame(0.4);
    CNA_EDITOR_EXPECT(!fixture.store().findForProject(fixture.projectPath()).has_value());

    fixture.application->renderFrame(0.7);
    const std::optional<RecoverySnapshot> snapshot = fixture.store().findForProject(fixture.projectPath());
    CNA_EDITOR_EXPECT(snapshot.has_value());
    CNA_EDITOR_EXPECT_EQ(snapshot->sceneName, std::string{"Level01"});
    CNA_EDITOR_EXPECT_EQ(snapshot->scene["entities"].getElements().size(), std::size_t{1});

    // The scene file itself is untouched until the user saves. A snapshot that wrote through to
    // the document would be an autosave, which is a different feature with different consent.
    SceneDocument onDisk;
    ComponentRegistry registry;
    registerBuiltinComponents(registry);
    CNA_EDITOR_EXPECT(onDisk.loadFromFile(
        (fixture.directory / "Scenes" / "Level01.cnascene").generic_string(), registry).succeeded);
    CNA_EDITOR_EXPECT_EQ(onDisk.getEntityCount(), std::size_t{0});

    // Saving makes the snapshot pointless, and leaving it would train users to click "discard"
    // on a message they stopped reading.
    fixture.application->saveScene();
    fixture.application->renderFrame(2.0);
    CNA_EDITOR_EXPECT(!fixture.store().findForProject(fixture.projectPath()).has_value());
}

CNA_EDITOR_TEST(RecoveredWorkIsOfferedRatherThanRestoredBehindTheUsersBack)
{
    RecoveryFixture first{"recoveroffer"};
    first.addEntity("Player");
    first.application->renderFrame(2.0);
    CNA_EDITOR_EXPECT(first.store().findForProject(first.projectPath()).has_value());

    // Open the same project again, as if the editor had been killed. The snapshot directory is
    // the fixture's, so a second application over the same files sees the first one's work.
    auto scripted = std::make_unique<ScriptedUi>();
    ScriptedUi* ui = scripted.get();
    EditorApplication reopened{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    options.autosaveSeconds = 1.0;
    options.recoveryDirectory = (first.directory / "recovery").generic_string();
    options.projectPath = first.projectPath();
    CNA_EDITOR_EXPECT(reopened.initialize(options));

    // Offered, not applied: the document is still the file on disk.
    CNA_EDITOR_EXPECT(reopened.getRecoverableScene() != nullptr);
    CNA_EDITOR_EXPECT_EQ(reopened.getContext().getScene().getEntityCount(), std::size_t{0});

    bool warned = false;
    for (const auto& entry : ui->getLog())
    {
        if (entry.message.find("Unsaved changes to scene 'Level01'") != std::string::npos)
        {
            warned = entry.severity == LogSeverity::Warning;
        }
    }
    CNA_EDITOR_EXPECT(warned);

    // Until it is answered, autosave must not overwrite it: this session's unsaved seconds are
    // worth less than the previous session's unsaved work.
    reopened.getContext().getScene().setName("Touched");
    EditorEntity entity{Uuid::generate(), "Later"};
    entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
    reopened.getContext().execute(
        std::make_unique<CreateEntityCommand>(reopened.getContext().getScene(), std::move(entity)));
    reopened.renderFrame(5.0);

    const RecoveryStore store{options.recoveryDirectory};
    CNA_EDITOR_EXPECT_EQ(store.findForProject(options.projectPath)->sceneName, std::string{"Level01"});

    // Accepting it brings the work back, leaves the file alone, and reports the document as
    // holding changes that were never saved.
    reopened.recoverScene();
    CNA_EDITOR_EXPECT(reopened.getRecoverableScene() == nullptr);
    CNA_EDITOR_EXPECT_EQ(reopened.getContext().getScene().getEntityCount(), std::size_t{1});
    CNA_EDITOR_EXPECT(reopened.getContext().getHistory().isDirty());
    CNA_EDITOR_EXPECT_EQ(reopened.getContext().getHistory().getCount(), std::size_t{0});
}

CNA_EDITOR_TEST(DiscardingARecoveredSceneRemovesTheSnapshotForGood)
{
    RecoveryFixture first{"recoverydiscard"};
    first.addEntity("Player");
    first.application->renderFrame(2.0);

    auto scripted = std::make_unique<ScriptedUi>();
    EditorApplication reopened{std::move(scripted), std::make_unique<NullEditorViewport>()};

    EditorOptions options;
    options.headless = true;
    options.autosaveSeconds = 1.0;
    options.recoveryDirectory = (first.directory / "recovery").generic_string();
    options.projectPath = first.projectPath();
    CNA_EDITOR_EXPECT(reopened.initialize(options));
    CNA_EDITOR_EXPECT(reopened.getRecoverableScene() != nullptr);

    reopened.discardRecoveredScene();

    CNA_EDITOR_EXPECT(reopened.getRecoverableScene() == nullptr);
    const RecoveryStore store{options.recoveryDirectory};
    CNA_EDITOR_EXPECT(!store.findForProject(options.projectPath).has_value());

    // And the editor goes back to protecting the current session.
    EditorEntity entity{Uuid::generate(), "Later"};
    entity.addComponent(EditorComponent{BuiltinComponentIds::kTransform});
    reopened.getContext().execute(
        std::make_unique<CreateEntityCommand>(reopened.getContext().getScene(), std::move(entity)));
    reopened.renderFrame(5.0);
    CNA_EDITOR_EXPECT(store.findForProject(options.projectPath).has_value());
}

CNA_EDITOR_TEST(AutosaveCanBeTurnedOffEntirely)
{
    RecoveryFixture fixture{"autosaveoff", 0.0};

    fixture.addEntity("Player");
    fixture.application->renderFrame(600.0);

    CNA_EDITOR_EXPECT(!fixture.store().findForProject(fixture.projectPath()).has_value());
}

CNA_EDITOR_TEST(OptionsParseTheAutosaveAndRecoveryFlags)
{
    const char* argv[] = {"cna-editor", "--autosave=5", "--recovery-dir=/tmp/snapshots"};
    const EditorOptions options = EditorOptions::parse(3, argv);

    CNA_EDITOR_EXPECT(!options.hasError);
    CNA_EDITOR_EXPECT(options.autosaveSeconds > 4.9 && options.autosaveSeconds < 5.1);
    CNA_EDITOR_EXPECT_EQ(options.recoveryDirectory, std::string{"/tmp/snapshots"});

    const char* bad[] = {"cna-editor", "--autosave=often"};
    CNA_EDITOR_EXPECT(EditorOptions::parse(2, bad).hasError);

    // A negative interval is clamped rather than rejected: it means the same thing as zero, and
    // failing to start over it would be a poor trade.
    const char* negative[] = {"cna-editor", "--autosave=-1"};
    const EditorOptions clamped = EditorOptions::parse(2, negative);
    CNA_EDITOR_EXPECT(!clamped.hasError);
    CNA_EDITOR_EXPECT_EQ(clamped.autosaveSeconds, 0.0);
}
