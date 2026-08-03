// SPDX-License-Identifier: MS-PL
/**
 * @file ProjectAndAssetTests.cpp
 * @brief Tests for the project format, the backend table, the asset database and the wire protocol.
 */

#include "TestHarness.hpp"

#include <filesystem>
#include <fstream>

#include "CNA/Editor/Assets/AssetCommands.hpp"
#include "CNA/Editor/Assets/AssetDatabase.hpp"
#include "CNA/Editor/Assets/AssetImporters.hpp"
#include "CNA/Editor/Assets/AssetWatcher.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/MissingReferences.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"
#include "CNA/Editor/Plugins/Plugin.hpp"
#include "CNA/Editor/Project/Project.hpp"
#include "CNA/Editor/RuntimeBridge/EditorProtocol.hpp"

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
}

CNA_EDITOR_TEST(ProjectRoundTripsThroughAFile)
{
    const std::filesystem::path directory = makeScratchDirectory("project");
    const std::string path = (directory / "MyGame.cnaproject").generic_string();

    Project original = Project::createDefault("MyGame", directory.generic_string());
    original.setDefaultGraphicsBackend("vulkan");
    original.setModules({"cna-core", "cna-graphics-2d", "cna-audio"});
    original.setTargetPlatforms({"linux-x64", "windows-x64"});

    std::string errorMessage;
    CNA_EDITOR_EXPECT(original.saveToFile(path, &errorMessage));
    CNA_EDITOR_EXPECT_EQ(errorMessage, std::string{});

    Project restored;
    const ProjectLoadResult result = restored.loadFromFile(path);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{0});
    CNA_EDITOR_EXPECT_EQ(restored.getName(), std::string{"MyGame"});
    CNA_EDITOR_EXPECT(restored.getKind() == ProjectKind::CnaNative);
    CNA_EDITOR_EXPECT_EQ(restored.getDefaultGraphicsBackend(), std::string{"vulkan"});
    CNA_EDITOR_EXPECT_EQ(restored.getModules().size(), std::size_t{3});
    CNA_EDITOR_EXPECT_EQ(restored.getStartupScene(), std::string{"Scenes/MainMenu.cnascene"});
    CNA_EDITOR_EXPECT_EQ(restored.getRootPath(), directory.generic_string());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(ProjectWarnsAboutAnUnknownBackend)
{
    JsonValue json = JsonValue::makeObject();
    json.set("formatVersion", JsonValue{Project::kFormatVersion});
    json.set("name", JsonValue{"MyGame"});
    json.set("defaultGraphicsBackend", JsonValue{"glide"});

    Project project;
    const ProjectLoadResult result = project.loadFromJson(json);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
}

CNA_EDITOR_TEST(XnaCompatibleProjectWarnsAboutAStartupScene)
{
    // An XNA-style game owns its own object model. Declaring a scene the editor will never load
    // is a real mismatch worth surfacing, not something to silently honour.
    JsonValue json = JsonValue::makeObject();
    json.set("formatVersion", JsonValue{Project::kFormatVersion});
    json.set("name", JsonValue{"PortedGame"});
    json.set("kind", JsonValue{"XnaCompatible"});
    json.set("startupScene", JsonValue{"Scenes/Main.cnascene"});

    Project project;
    const ProjectLoadResult result = project.loadFromJson(json);
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT(project.getKind() == ProjectKind::XnaCompatible);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
}

CNA_EDITOR_TEST(BackendTableCoversEveryCnaBackend)
{
    // Mirrors cmake/BackendSelection.cmake in the CNA revision this editor targets. When CNA gains
    // a backend, this count changes and this test is the reminder to update the table.
    CNA_EDITOR_EXPECT_EQ(getKnownBackends().size(), std::size_t{14});

    CNA_EDITOR_EXPECT(findBackend("easygl") != nullptr);
    CNA_EDITOR_EXPECT(findBackend("EASYGL") != nullptr);
    CNA_EDITOR_EXPECT(findBackend("glide") == nullptr);

    CNA_EDITOR_EXPECT(findBackend("easygl")->support == BackendEditorSupport::EditorSupported);
    // A CPU rasterizer is a fine reference renderer and a hopeless interactive UI host.
    CNA_EDITOR_EXPECT(findBackend("software")->support == BackendEditorSupport::PreviewOnly);
    // Historical and Emscripten-only backends are exactly why the player is a separate process.
    CNA_EDITOR_EXPECT(findBackend("dx3")->support == BackendEditorSupport::RuntimeOnly);
    CNA_EDITOR_EXPECT(findBackend("canvas")->support == BackendEditorSupport::RuntimeOnly);
}

CNA_EDITOR_TEST(AssetDatabaseAssignsStableIdsAndWritesSidecars)
{
    const std::filesystem::path directory = makeScratchDirectory("assets");
    writeFile(directory / "Assets" / "Textures" / "player.png", "not-a-real-png");
    writeFile(directory / "Assets" / "Sounds" / "click.wav", "not-a-real-wav");

    AssetDatabase database;
    database.setProjectRoot(directory.generic_string());

    const AssetScanResult first = database.scan("Assets");
    CNA_EDITOR_EXPECT(first.succeeded);
    CNA_EDITOR_EXPECT_EQ(first.discoveredCount, std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(first.newCount, std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(database.getCount(), std::size_t{2});

    const AssetRecord* texture = database.findByPath("Assets/Textures/player.png");
    CNA_EDITOR_EXPECT(texture != nullptr);
    CNA_EDITOR_EXPECT(texture->type == AssetType::Texture2D);
    CNA_EDITOR_EXPECT_EQ(texture->importerId, std::string{"CNA.TextureImporter"});
    const Uuid textureId = texture->id;

    // The sidecar must be on disk, so the id survives a restart.
    CNA_EDITOR_EXPECT(std::filesystem::exists(
        (directory / "Assets" / "Textures" / "player.png.cnaasset").generic_string()));

    // A second scan from a fresh database must recover the same ids, not mint new ones.
    AssetDatabase reopened;
    reopened.setProjectRoot(directory.generic_string());
    const AssetScanResult second = reopened.scan("Assets");
    CNA_EDITOR_EXPECT(second.succeeded);
    CNA_EDITOR_EXPECT_EQ(second.newCount, std::size_t{0});
    CNA_EDITOR_EXPECT(reopened.find(textureId) != nullptr);

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(AssetDatabaseKeepsIdentityAcrossAMove)
{
    // This is the entire reason assets are referenced by id and not by path: moving a file must
    // not touch a single scene.
    const std::filesystem::path directory = makeScratchDirectory("assetmove");
    writeFile(directory / "Assets" / "player.png", "pixels");

    AssetDatabase database;
    database.setProjectRoot(directory.generic_string());
    CNA_EDITOR_EXPECT(database.scan("Assets").succeeded);

    const Uuid originalId = database.findByPath("Assets/player.png")->id;

    std::filesystem::create_directories(directory / "Assets" / "Characters");
    std::filesystem::rename(directory / "Assets" / "player.png",
                            directory / "Assets" / "Characters" / "player.png");
    std::filesystem::rename(directory / "Assets" / "player.png.cnaasset",
                            directory / "Assets" / "Characters" / "player.png.cnaasset");

    AssetDatabase rescanned;
    rescanned.setProjectRoot(directory.generic_string());
    const AssetScanResult result = rescanned.scan("Assets");
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.newCount, std::size_t{0});

    const AssetRecord* moved = rescanned.find(originalId);
    CNA_EDITOR_EXPECT(moved != nullptr);
    CNA_EDITOR_EXPECT_EQ(moved->sourcePath, std::string{"Assets/Characters/player.png"});
    CNA_EDITOR_EXPECT(rescanned.findByPath("Assets/player.png") == nullptr);

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(AssetDatabaseReportsMissingSourcesRatherThanDroppingThem)
{
    const std::filesystem::path directory = makeScratchDirectory("assetmissing");
    writeFile(directory / "Assets" / "gone.png", "pixels");

    AssetDatabase database;
    database.setProjectRoot(directory.generic_string());
    CNA_EDITOR_EXPECT(database.scan("Assets").succeeded);

    const Uuid id = database.findByPath("Assets/gone.png")->id;
    std::filesystem::remove(directory / "Assets" / "gone.png");

    // The record survives: a file that is gone today may be one git checkout away from returning,
    // and deleting the record would break every reference to it permanently.
    CNA_EDITOR_EXPECT(database.find(id) != nullptr);
    CNA_EDITOR_EXPECT(database.isMissing(id));
    CNA_EDITOR_EXPECT_EQ(database.getMissingAssets().size(), std::size_t{1});

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(AssetDatabaseTolerationOfAnAbsentAssetDirectory)
{
    const std::filesystem::path directory = makeScratchDirectory("assetempty");

    AssetDatabase database;
    database.setProjectRoot(directory.generic_string());

    // A brand-new project has no Assets directory yet; that is normal, not an error.
    const AssetScanResult result = database.scan("Assets");
    CNA_EDITOR_EXPECT(result.succeeded);
    CNA_EDITOR_EXPECT_EQ(result.warnings.size(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(database.getCount(), std::size_t{0});

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(AssetSidecarStampSurvivesAJsonRoundTrip)
{
    // Regression: the stamp was originally the filesystem clock's native tick count -- around
    // 4.6e18 nanoseconds -- which is past the range a double represents exactly. JSON numbers are
    // doubles, so it came back changed and every asset looked modified on every scan.
    const std::filesystem::path directory = makeScratchDirectory("assetstamp");
    writeFile(directory / "Assets" / "stamped.png", "pixels");

    AssetDatabase database;
    database.setProjectRoot(directory.generic_string());
    CNA_EDITOR_EXPECT(database.scan("Assets").succeeded);

    const std::int64_t stamp = database.findByPath("Assets/stamped.png")->sourceModifiedTime;
    CNA_EDITOR_EXPECT(stamp != 0);

    AssetDatabase reopened;
    reopened.setProjectRoot(directory.generic_string());
    CNA_EDITOR_EXPECT(reopened.scan("Assets").succeeded);
    CNA_EDITOR_EXPECT_EQ(reopened.findByPath("Assets/stamped.png")->sourceModifiedTime, stamp);

    // And the sidecar itself must hold a plain integer, not scientific notation.
    std::ifstream sidecar{(directory / "Assets" / "stamped.png.cnaasset").generic_string()};
    std::string text{std::istreambuf_iterator<char>{sidecar}, std::istreambuf_iterator<char>{}};
    const std::size_t fieldOffset = text.find("\"modifiedTime\"");
    CNA_EDITOR_EXPECT(fieldOffset != std::string::npos);
    const std::size_t valueOffset = text.find(':', fieldOffset) + 1;
    const std::string writtenValue = text.substr(valueOffset, text.find('\n', valueOffset) - valueOffset);
    CNA_EDITOR_EXPECT(writtenValue.find('e') == std::string::npos);
    CNA_EDITOR_EXPECT(writtenValue.find('E') == std::string::npos);

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(AssetTypeGuessingCoversTheCommonExtensions)
{
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("a/b/c.PNG") == AssetType::Texture2D);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.wav") == AssetType::SoundEffect);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.ogg") == AssetType::Song);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.gltf") == AssetType::Model);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.cnj") == AssetType::Model);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.cnascene") == AssetType::Scene);
    CNA_EDITOR_EXPECT(AssetDatabase::guessTypeFromExtension("x.zzz") == AssetType::Unknown);
}

CNA_EDITOR_TEST(ProtocolMessagesRoundTripThroughTheWire)
{
    const EditorMessage original = EditorMessage::makeLoadScene("Scenes/Level01.cnascene");
    const std::string encoded = original.encode();

    // The framing is the newline, so the body must contain exactly one and it must be last.
    CNA_EDITOR_EXPECT_EQ(encoded.back(), '\n');
    CNA_EDITOR_EXPECT_EQ(encoded.find('\n'), encoded.size() - 1);

    const std::optional<EditorMessage> decoded = EditorMessage::decode(encoded.substr(0, encoded.size() - 1));
    CNA_EDITOR_EXPECT(decoded.has_value());
    CNA_EDITOR_EXPECT(decoded->type == EditorMessageType::LoadScene);
    CNA_EDITOR_EXPECT_EQ(decoded->payload["scenePath"].asString(), std::string{"Scenes/Level01.cnascene"});
}

CNA_EDITOR_TEST(ProtocolSetPropertyCarriesItsOwnType)
{
    const Uuid entityId = Uuid::generate();
    const EditorMessage message = EditorMessage::makeSetProperty(
        entityId, "CNA.Transform", "position", PropertyValue{EditorVector3{1.0f, 2.0f, 3.0f}});

    // Unlike a scene file, the wire must be self-describing: the player's component registry may
    // not match the editor's after a plugin reload.
    CNA_EDITOR_EXPECT_EQ(message.payload["valueType"].asString(), std::string{"vector3"});
    CNA_EDITOR_EXPECT_EQ(message.payload["entityId"].asString(), entityId.toString());

    const PropertyValue restored =
        PropertyValue::fromJson(message.payload["value"],
                                parsePropertyType(message.payload["valueType"].asString()));
    CNA_EDITOR_EXPECT_EQ(restored.get<EditorVector3>().y, 2.0f);
}

CNA_EDITOR_TEST(MessageDecoderReassemblesSplitMessages)
{
    // A stream socket delivers arbitrary chunks. A reader that assumes one recv() equals one
    // message works until a message straddles a packet boundary, and then fails unreproducibly.
    const std::string wire = EditorMessage::makeHello("/tmp/MyGame").encode()
                           + EditorMessage::makeReportLog("info", "ready").encode();

    MessageStreamDecoder decoder;
    std::vector<EditorMessage> messages;
    for (std::size_t offset = 0; offset < wire.size(); offset += 7)
    {
        const std::vector<EditorMessage> batch = decoder.feed(std::string_view{wire}.substr(offset, 7));
        messages.insert(messages.end(), batch.begin(), batch.end());
    }

    CNA_EDITOR_EXPECT_EQ(messages.size(), std::size_t{2});
    CNA_EDITOR_EXPECT(messages[0].type == EditorMessageType::Hello);
    CNA_EDITOR_EXPECT_EQ(messages[0].payload["protocolVersion"].asInt(), kEditorProtocolVersion);
    CNA_EDITOR_EXPECT(messages[1].type == EditorMessageType::ReportLog);
    CNA_EDITOR_EXPECT_EQ(decoder.getDroppedCount(), std::uint64_t{0});
}

CNA_EDITOR_TEST(MessageDecoderSkipsGarbageWithoutLosingTheStream)
{
    MessageStreamDecoder decoder;
    const std::string wire = "this is not json\n"
                           + EditorMessage::makeHello("/tmp/MyGame").encode()
                           + "{\"type\":\"somethingFromTheFuture\"}\n"
                           + EditorMessage::makeReportLog("warn", "still here").encode();

    const std::vector<EditorMessage> messages = decoder.feed(wire);

    // A peer from a newer revision sending something unrecognised must not kill a play session.
    CNA_EDITOR_EXPECT_EQ(messages.size(), std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(decoder.getDroppedCount(), std::uint64_t{2});
}

CNA_EDITOR_TEST(PluginHostRejectsIncompatibleManifestsWithoutLoadingThem)
{
    const std::filesystem::path directory = makeScratchDirectory("plugins");

    writeFile(directory / "good" / "plugin.json",
              R"({"id":"org.openeggbert.mc3","name":"MC3 Tools","version":"0.1.0",)"
              R"("editorApiVersion":1,"library":"libmc3.so"})");
    writeFile(directory / "good" / "libmc3.so", "not-a-real-library");

    writeFile(directory / "stale" / "plugin.json",
              R"({"id":"org.example.stale","editorApiVersion":0,"library":"libstale.so"})");

    writeFile(directory / "broken" / "plugin.json", "{ this is not json");

    PluginHost host;
    const std::vector<LoadedPlugin> plugins = host.discover(directory.generic_string());
    CNA_EDITOR_EXPECT_EQ(plugins.size(), std::size_t{3});

    std::size_t apiMismatchCount = 0;
    std::size_t malformedCount = 0;
    for (const LoadedPlugin& plugin : plugins)
    {
        if (plugin.error.find("editor API version") != std::string::npos) { ++apiMismatchCount; }
        if (plugin.error.find("malformed") != std::string::npos) { ++malformedCount; }
        // Nothing loads yet, by design: an ABI mismatch that reaches dlopen is a crash, not an
        // error message, so validation has to be right before loading exists at all.
        CNA_EDITOR_EXPECT(!plugin.loaded);
    }
    CNA_EDITOR_EXPECT_EQ(apiMismatchCount, std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(malformedCount, std::size_t{1});

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(MissingReferencesFindsBrokenAssetSlots)
{
    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    // A real project on disk, so "tracked and present" and "tracked but the file is gone" are
    // genuinely different states rather than both resolving to absent.
    const std::filesystem::path directory = makeScratchDirectory("missingrefs");
    writeFile(directory / "Textures" / "Present.png", "not really a png");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());

    const Uuid presentId = Uuid::generate();
    AssetRecord present;
    present.id = presentId;
    present.sourcePath = "Textures/Present.png";
    present.type = AssetType::Texture2D;
    assets.add(std::move(present));

    const Uuid strandedId = Uuid::generate();
    AssetRecord stranded;
    stranded.id = strandedId;
    stranded.sourcePath = "Textures/Deleted.png";
    stranded.type = AssetType::Texture2D;
    assets.add(std::move(stranded));

    SceneDocument scene;
    const Uuid goneId = Uuid::generate();

    const auto addSprite = [&](const std::string& name, const Uuid& textureId) {
        EditorEntity entity{Uuid::generate(), name};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        transform.applyDefaults(*registry.find(BuiltinComponentIds::kTransform));
        entity.addComponent(std::move(transform));

        EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
        sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
        sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{textureId}});
        entity.addComponent(std::move(sprite));
        return scene.addEntity(std::move(entity));
    };

    addSprite("Good", presentId);
    addSprite("BrokenA", goneId);
    addSprite("BrokenB", goneId);
    addSprite("Stranded", strandedId);
    addSprite("Empty", Uuid{});

    const std::vector<MissingReference> missing = findMissingReferences(scene, assets);

    // The resolvable one is fine, and an empty slot is an ordinary state rather than a fault --
    // a sprite that has not been given a texture yet is not broken.
    CNA_EDITOR_EXPECT_EQ(missing.size(), std::size_t{3});

    std::size_t notInDatabase = 0;
    std::size_t fileMissing = 0;
    for (const MissingReference& reference : missing)
    {
        CNA_EDITOR_EXPECT_EQ(reference.propertyName, std::string{"texture"});
        if (reference.reason == MissingReference::Reason::NotInDatabase)
        {
            ++notInDatabase;
            CNA_EDITOR_EXPECT_EQ(reference.assetId.toString(), goneId.toString());
        }
        else
        {
            ++fileMissing;
            CNA_EDITOR_EXPECT_EQ(reference.assetId.toString(), strandedId.toString());
        }
    }

    // The two reasons are genuinely different and both matter: an id deleted from the database is
    // invisible to AssetDatabase::getMissingAssets(), and a tracked asset whose file vanished is
    // invisible to a check that only looks the id up.
    CNA_EDITOR_EXPECT_EQ(notInDatabase, std::size_t{2});
    CNA_EDITOR_EXPECT_EQ(fileMissing, std::size_t{1});

    // Grouped by id, because a broken reference is almost always one asset that many entities
    // point at, and the fix is the same for all of them.
    CNA_EDITOR_EXPECT_EQ(collectMissingAssetIds(missing).size(), std::size_t{2});

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(MissingReferencesChecksComponentsWithNoDescriptor)
{
    // A component whose plugin failed to load keeps its data and must still have its references
    // checked -- that scene is the one most likely to be broken.
    AssetDatabase assets;
    SceneDocument scene;

    EditorEntity entity{Uuid::generate(), "Exotic"};
    EditorComponent unknown{"ThirdParty.Decal"};
    unknown.setProperty("atlas", PropertyValue{PropertyValue::AssetReference{Uuid::generate()}});
    entity.addComponent(std::move(unknown));
    scene.addEntity(std::move(entity));

    const std::vector<MissingReference> missing = findMissingReferences(scene, assets);
    CNA_EDITOR_EXPECT_EQ(missing.size(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(missing.front().componentTypeId, std::string{"ThirdParty.Decal"});
}

CNA_EDITOR_TEST(RelinkingRewritesEveryReferenceAsOneUndoEntry)
{
    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    SceneDocument scene;
    CommandHistory history;

    const Uuid goneId = Uuid::generate();
    const Uuid replacementId = Uuid::generate();

    std::vector<Uuid> sprites;
    for (int index = 0; index < 3; ++index)
    {
        EditorEntity entity{Uuid::generate(), "Sprite" + std::to_string(index)};
        EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
        sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
        sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{goneId}});
        entity.addComponent(std::move(sprite));
        sprites.push_back(scene.addEntity(std::move(entity)));
    }

    auto command = std::make_unique<RelinkAssetCommand>(scene, goneId, replacementId);
    CNA_EDITOR_EXPECT(command->isValid());
    CNA_EDITOR_EXPECT_EQ(command->getReferenceCount(), std::size_t{3});
    history.execute(std::move(command));

    const auto textureOf = [&](const Uuid& entityId) {
        return scene.findEntity(entityId)->findComponent(BuiltinComponentIds::kSpriteRenderer)
            ->getProperty("texture").get<PropertyValue::AssetReference>().id;
    };

    for (const Uuid& id : sprites) { CNA_EDITOR_EXPECT_EQ(textureOf(id).toString(), replacementId.toString()); }

    // One entry, not three. Undoing a relink of forty sprites must not be forty presses.
    CNA_EDITOR_EXPECT_EQ(history.getCount(), std::size_t{1});
    CNA_EDITOR_EXPECT(history.undo());
    for (const Uuid& id : sprites) { CNA_EDITOR_EXPECT_EQ(textureOf(id).toString(), goneId.toString()); }

    // Redo rewrites the same set the undo restored, because the targets were found once at
    // construction. Re-scanning in execute() would find nothing the second time.
    CNA_EDITOR_EXPECT(history.redo());
    for (const Uuid& id : sprites) { CNA_EDITOR_EXPECT_EQ(textureOf(id).toString(), replacementId.toString()); }
}

CNA_EDITOR_TEST(RelinkingToNilClearsTheReferences)
{
    ComponentRegistry registry;
    registerBuiltinComponents(registry);

    SceneDocument scene;
    const Uuid goneId = Uuid::generate();

    EditorEntity entity{Uuid::generate(), "Sprite"};
    EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
    sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
    sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{goneId}});
    entity.addComponent(std::move(sprite));
    const Uuid entityId = scene.addEntity(std::move(entity));

    RelinkAssetCommand clear{scene, goneId, Uuid{}};
    CNA_EDITOR_EXPECT(clear.isValid());
    clear.execute();

    // Clearing is the right answer when the asset is simply gone and nothing should replace it.
    CNA_EDITOR_EXPECT(!scene.findEntity(entityId)->findComponent(BuiltinComponentIds::kSpriteRenderer)
                           ->getProperty("texture").get<PropertyValue::AssetReference>().id.isValid());
}

CNA_EDITOR_TEST(RelinkingRefusesWhenNothingRefersToTheOldId)
{
    SceneDocument scene;
    RelinkAssetCommand nothing{scene, Uuid::generate(), Uuid::generate()};

    // A command that would do nothing must not reach the undo stack.
    CNA_EDITOR_EXPECT(!nothing.isValid());

    RelinkAssetCommand sameId{scene, Uuid{}, Uuid{}};
    CNA_EDITOR_EXPECT(!sameId.isValid());
}

CNA_EDITOR_TEST(TheTextureImporterDeclaresItsSettings)
{
    ComponentRegistry importers;
    registerBuiltinImporters(importers);

    const ComponentDescriptor* texture = importers.find(ImporterIds::kTexture);
    CNA_EDITOR_EXPECT(texture != nullptr);

    const auto property = [&](const std::string& name) -> const PropertyDescriptor* {
        for (const PropertyDescriptor& candidate : texture->properties)
        {
            if (candidate.name == name) { return &candidate; }
        }
        return nullptr;
    };

    // Premultiplied by default, because XNA's SpriteBatch blends that way -- an importer that
    // defaulted the other way would halo the edge of every sprite in a default project.
    CNA_EDITOR_EXPECT(property("premultiplyAlpha") != nullptr);
    CNA_EDITOR_EXPECT(property("premultiplyAlpha")->defaultValue.get<bool>());

    // Mipmaps off, because most 2D art is drawn at its native size and they cost a third more
    // memory for nothing.
    CNA_EDITOR_EXPECT(!property("generateMipmaps")->defaultValue.get<bool>());

    // The pixel size is a fact about the file, not a choice about it.
    CNA_EDITOR_EXPECT(property("pixelSize")->readOnly);

    CNA_EDITOR_EXPECT_EQ(property("wrapMode")->enumOptions.size(), std::size_t{3});
}

CNA_EDITOR_TEST(ImporterSettingsSurviveTheSidecarAndUndoBackToAbsent)
{
    const std::filesystem::path directory = makeScratchDirectory("importsettings");
    writeFile(directory / "Textures" / "Hero.png", "not really a png");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Textures");

    CNA_EDITOR_EXPECT_EQ(assets.getCount(), std::size_t{1});
    const Uuid assetId = assets.getAll().front()->id;
    CNA_EDITOR_EXPECT_EQ(assets.find(assetId)->importerId, std::string{ImporterIds::kTexture});

    CommandHistory history;
    auto command = std::make_unique<SetImporterSettingCommand>(assets, assetId, "generateMipmaps",
                                                               PropertyValue{true});
    CNA_EDITOR_EXPECT(command->isValid());
    history.execute(std::move(command));

    CNA_EDITOR_EXPECT(assets.find(assetId)->importerSettings["generateMipmaps"].asBoolean(false));

    // Written through, not merely held in memory: a setting that lived only in the session would
    // be lost on the next scan, and the user could not tell that from it having no effect.
    AssetDatabase reopened;
    reopened.setProjectRoot(directory.generic_string());
    reopened.scan("Textures");
    CNA_EDITOR_EXPECT(reopened.find(assetId) != nullptr);
    CNA_EDITOR_EXPECT(reopened.find(assetId)->importerSettings["generateMipmaps"].asBoolean(false));

    // Undo takes the field back out rather than writing a default in its place: a sidecar that
    // accumulated every field anyone glanced at would make its every diff noise.
    CNA_EDITOR_EXPECT(history.undo());
    CNA_EDITOR_EXPECT(assets.find(assetId)->importerSettings["generateMipmaps"].isNull());

    AssetDatabase afterUndo;
    afterUndo.setProjectRoot(directory.generic_string());
    afterUndo.scan("Textures");
    CNA_EDITOR_EXPECT(afterUndo.find(assetId)->importerSettings["generateMipmaps"].isNull());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(ImporterSettingEditsMergeIntoOneUndoEntry)
{
    const std::filesystem::path directory = makeScratchDirectory("importmerge");
    writeFile(directory / "Audio" / "Jump.wav", "not really a wav");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Audio");

    const Uuid assetId = assets.getAll().front()->id;
    CommandHistory history;

    for (const float volume : {0.9f, 0.8f, 0.7f, 0.6f})
    {
        auto command = std::make_unique<SetImporterSettingCommand>(assets, assetId, "importVolume",
                                                                   PropertyValue{volume});
        history.execute(std::move(command), MergePolicy::MergeWithPrevious);
    }

    // One entry for the drag, undoing to where it started -- the same policy the scene's own
    // property fields use.
    CNA_EDITOR_EXPECT_EQ(history.getCount(), std::size_t{1});
    CNA_EDITOR_EXPECT(history.undo());
    CNA_EDITOR_EXPECT(assets.find(assetId)->importerSettings["importVolume"].isNull());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(JsonRemoveTakesAFieldBackOut)
{
    JsonValue object = JsonValue::makeObject();
    object.set("kept", JsonValue{1});
    object.set("dropped", JsonValue{2});

    CNA_EDITOR_EXPECT(object.remove("dropped"));
    CNA_EDITOR_EXPECT(object["dropped"].isNull());
    CNA_EDITOR_EXPECT_EQ(object["kept"].asInt(0), 1);

    // Removing what is not there is not an error, and removing from a non-object is not a crash.
    CNA_EDITOR_EXPECT(!object.remove("dropped"));
    JsonValue scalar{5};
    CNA_EDITOR_EXPECT(!scalar.remove("anything"));
}

CNA_EDITOR_TEST(ImageSizeIsReadFromThePngHeader)
{
    const std::filesystem::path directory = makeScratchDirectory("imagesize");

    // A 3x2 PNG header: the 8-byte signature, then the IHDR length and tag, then the dimensions
    // big-endian. Only the header matters -- nothing here decodes the image.
    std::string png;
    const unsigned char signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
                                       0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R',
                                       0x00, 0x00, 0x00, 0x03,
                                       0x00, 0x00, 0x00, 0x02,
                                       0x08, 0x06, 0x00, 0x00, 0x00};
    png.assign(reinterpret_cast<const char*>(signature), sizeof(signature));
    writeFile(directory / "Tiny.png", png);

    const std::optional<ImageSize> size = readImageSize((directory / "Tiny.png").generic_string());
    CNA_EDITOR_EXPECT(size.has_value());
    CNA_EDITOR_EXPECT_EQ(size->width, 3);
    CNA_EDITOR_EXPECT_EQ(size->height, 2);

    // A format the editor cannot measure is unknown, not zero -- the caller has to be able to
    // tell "I could not read this" from "this image is empty".
    writeFile(directory / "Photo.jpg", "\xFF\xD8\xFF\xE0 not really a jpeg but long enough to read");
    CNA_EDITOR_EXPECT(!readImageSize((directory / "Photo.jpg").generic_string()).has_value());
    CNA_EDITOR_EXPECT(!readImageSize((directory / "Absent.png").generic_string()).has_value());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(ImporterFactsAreWrittenOnceAndNotRewrittenOnEveryScan)
{
    const std::filesystem::path directory = makeScratchDirectory("importerfacts");

    std::string png;
    const unsigned char signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
                                       0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R',
                                       0x00, 0x00, 0x00, 0x20,
                                       0x00, 0x00, 0x00, 0x10,
                                       0x08, 0x06, 0x00, 0x00, 0x00};
    png.assign(reinterpret_cast<const char*>(signature), sizeof(signature));
    writeFile(directory / "Textures" / "Hero.png", png);

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Textures");

    CNA_EDITOR_EXPECT_EQ(applyImporterFacts(assets), std::size_t{1});

    const Uuid assetId = assets.getAll().front()->id;
    const EditorVector2 size =
        PropertyValue::fromJson(assets.find(assetId)->importerSettings["pixelSize"], PropertyType::Vector2)
            .get<EditorVector2>();
    CNA_EDITOR_EXPECT_EQ(size.x, 32.0f);
    CNA_EDITOR_EXPECT_EQ(size.y, 16.0f);

    // Nothing changed, so nothing is rewritten. A scan that touched every sidecar on every open
    // would show up as a repository full of spurious diffs.
    CNA_EDITOR_EXPECT_EQ(applyImporterFacts(assets), std::size_t{0});

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(TheWatcherNoticesAnExternalEditExactlyOnce)
{
    const std::filesystem::path directory = makeScratchDirectory("watchedit");
    writeFile(directory / "Textures" / "Hero.png", "first contents");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Textures");
    const Uuid assetId = assets.getAll().front()->id;

    AssetWatcher watcher;
    watcher.setInterval(1.0);

    // Nothing has changed and no interval has elapsed, so nothing is polled and nothing reported.
    CNA_EDITOR_EXPECT(!watcher.poll(assets, 0.1).polled);
    CNA_EDITOR_EXPECT(!watcher.poll(assets, 2.0).hasChanges());

    writeFile(directory / "Textures" / "Hero.png", "second contents, a different length entirely");

    const AssetWatchResult first = watcher.poll(assets, 2.0);
    CNA_EDITOR_EXPECT_EQ(first.changed.size(), std::size_t{1});
    CNA_EDITOR_EXPECT_EQ(first.changed.front().toString(), assetId.toString());

    // Reported once. The stamp is updated as it is reported, so a console does not fill with the
    // same line twice a second until something else happens to fix it.
    CNA_EDITOR_EXPECT(!watcher.poll(assets, 2.0).hasChanges());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(TheWatcherReportsDisappearanceAndReturnSeparately)
{
    const std::filesystem::path directory = makeScratchDirectory("watchgone");
    writeFile(directory / "Textures" / "Hero.png", "contents");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Textures");
    const Uuid assetId = assets.getAll().front()->id;

    AssetWatcher watcher;
    watcher.setInterval(0.0);

    std::filesystem::remove(directory / "Textures" / "Hero.png");

    const AssetWatchResult gone = watcher.poll(assets, 0.0);
    CNA_EDITOR_EXPECT_EQ(gone.removed.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(gone.changed.empty());

    // Once, not on every poll for the rest of the session.
    CNA_EDITOR_EXPECT(!watcher.poll(assets, 0.0).hasChanges());

    writeFile(directory / "Textures" / "Hero.png", "contents are back");

    // A file returning is worth telling apart from one being edited: the first fixes a broken
    // reference, the second means reloading something already on screen.
    const AssetWatchResult back = watcher.poll(assets, 0.0);
    CNA_EDITOR_EXPECT_EQ(back.restored.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(back.changed.empty());
    CNA_EDITOR_EXPECT(!watcher.poll(assets, 0.0).hasChanges());

    std::filesystem::remove_all(directory);
}

CNA_EDITOR_TEST(TheWatcherHonoursItsInterval)
{
    const std::filesystem::path directory = makeScratchDirectory("watchinterval");
    writeFile(directory / "Textures" / "Hero.png", "contents");

    AssetDatabase assets;
    assets.setProjectRoot(directory.generic_string());
    assets.scan("Textures");

    AssetWatcher watcher;
    watcher.setInterval(10.0);

    writeFile(directory / "Textures" / "Hero.png", "much longer contents than before");

    // A change that has happened is not reported until a poll is due: the whole point of the
    // interval is that a frame does not cost one stat call per asset.
    for (int frame = 0; frame < 5; ++frame)
    {
        CNA_EDITOR_EXPECT(!watcher.poll(assets, 1.0).polled);
    }

    // Unless something asks for one now, which is what a manual "Refresh" would do.
    watcher.requestImmediatePoll();
    CNA_EDITOR_EXPECT_EQ(watcher.poll(assets, 0.0).changed.size(), std::size_t{1});

    std::filesystem::remove_all(directory);
}
