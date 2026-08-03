// SPDX-License-Identifier: MS-PL
/**
 * @file ProjectAndAssetTests.cpp
 * @brief Tests for the project format, the backend table, the asset database and the wire protocol.
 */

#include "TestHarness.hpp"

#include <filesystem>
#include <fstream>

#include "CNA/Editor/Assets/AssetDatabase.hpp"
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
