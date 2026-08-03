// SPDX-License-Identifier: MS-PL
/**
 * @file PlayerTests.cpp
 * @brief Tests for the runtime bridge: the transport, the player's state machine, and a real
 *        editor-spawns-player round trip over a real socket.
 *
 * The last of those is the important one. Everything else here could pass while play mode remained
 * broken; `EditorLaunchesARealPlayerProcessAndTalksToIt` starts the actual `cna-player` binary,
 * connects to it over loopback TCP, exchanges real messages and shuts it down.
 */

#include "TestHarness.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "CNA/Editor/Player/PlayerHost.hpp"
#include "CNA/Editor/RuntimeBridge/MessageChannel.hpp"
#include "CNA/Editor/RuntimeBridge/PlayerProcess.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"

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

    /** @brief Writes a minimal but complete project with one scene and one entity. */
    struct ScratchProject
    {
        std::filesystem::path directory;
        std::string projectPath;
        Uuid entityId;

        explicit ScratchProject(const std::string& name)
        {
            directory = makeScratchDirectory(name);

            Project project = Project::createDefault("BridgeGame", directory.generic_string());
            project.setStartupScene("Scenes/Level01.cnascene");
            std::string errorMessage;
            const bool projectSaved = project.saveToFile({}, &errorMessage);
            (void)projectSaved;
            projectPath = project.getFilePath();

            ComponentRegistry registry;
            registerBuiltinComponents(registry);

            SceneDocument scene;
            scene.setName("Level01");
            EditorEntity player{Uuid::generate(), "Player"};
            EditorComponent transform{BuiltinComponentIds::kTransform};
            transform.applyDefaults(*registry.find(BuiltinComponentIds::kTransform));
            player.addComponent(std::move(transform));
            entityId = scene.addEntity(std::move(player));
            const bool sceneSaved =
                scene.saveToFile((directory / "Scenes" / "Level01.cnascene").generic_string(), &errorMessage);
            (void)sceneSaved;
        }

        ~ScratchProject()
        {
            std::error_code errorCode;
            std::filesystem::remove_all(directory, errorCode);
        }
    };

    /**
     * @brief Returns the directory holding cna-player.
     *
     * Supplied by CMake as a generator expression over the real target, rather than guessed from
     * the test binary's own location -- the two live in different directories, and a guess would
     * silently turn the end-to-end bridge test into a no-op that always passes.
     */
    std::filesystem::path playerDirectory()
    {
        return std::filesystem::path{CNA_EDITOR_TEST_PLAYER_DIR};
    }
}

CNA_EDITOR_TEST(MessageChannelBindsAnEphemeralPort)
{
    MessageChannel channel;
    CNA_EDITOR_EXPECT(channel.listen(0));

    // Port 0 asks the OS to choose. Reading it back matters: that value is what reaches the
    // player's command line, and a fixed port would collide with whatever else is running.
    CNA_EDITOR_EXPECT(channel.getPort() != 0);
    CNA_EDITOR_EXPECT(channel.getState() == ChannelState::Listening);

    channel.close();
    CNA_EDITOR_EXPECT(channel.getState() == ChannelState::Closed);
}

CNA_EDITOR_TEST(MessageChannelCarriesMessagesBothWays)
{
    MessageChannel server;
    CNA_EDITOR_EXPECT(server.listen(0));

    MessageChannel client;
    CNA_EDITOR_EXPECT(client.connect(server.getPort()));

    // Both ends are non-blocking, so the handshake completes over several polls rather than in
    // one call. That is the same loop the editor runs once per frame.
    bool connected = false;
    for (int attempt = 0; attempt < 200 && !connected; ++attempt)
    {
        server.poll();
        client.poll();
        connected = server.isConnected() && client.isConnected();
        if (!connected) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    }
    CNA_EDITOR_EXPECT(connected);

    CNA_EDITOR_EXPECT(server.send(EditorMessage::makeLoadScene("Scenes/Level01.cnascene")));

    std::vector<EditorMessage> received;
    for (int attempt = 0; attempt < 200 && received.empty(); ++attempt)
    {
        const std::vector<EditorMessage> batch = client.poll();
        received.insert(received.end(), batch.begin(), batch.end());
        if (received.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    }

    CNA_EDITOR_EXPECT_EQ(received.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(received.front().type == EditorMessageType::LoadScene);
    CNA_EDITOR_EXPECT_EQ(received.front().payload["scenePath"].asString(),
                         std::string{"Scenes/Level01.cnascene"});
    CNA_EDITOR_EXPECT_EQ(client.getDroppedCount(), std::uint64_t{0});
}

CNA_EDITOR_TEST(MessageChannelReportsAFailedConnect)
{
    MessageChannel client;
    // Port 1 on loopback: privileged and nothing is listening, so this cannot succeed.
    client.connect(1);

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        client.poll();
        if (client.getState() == ChannelState::Failed) { break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CNA_EDITOR_EXPECT(!client.isConnected());
}

CNA_EDITOR_TEST(PlayerHostOpensAProjectAndItsStartupScene)
{
    const ScratchProject project{"playerhost"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));
    CNA_EDITOR_EXPECT_EQ(host.getScene().getName(), std::string{"Level01"});
    CNA_EDITOR_EXPECT(host.getScene().findEntity(project.entityId) != nullptr);
    CNA_EDITOR_EXPECT(host.getPlayState() == PlayState::Running);

    const EditorMessage ready = host.makeReady("SOFTWARE");
    CNA_EDITOR_EXPECT(ready.type == EditorMessageType::Ready);
    // The backend is the one thing the editor cannot infer: it is fixed at compile time, so the
    // Ready message is how the editor learns which build it actually got.
    CNA_EDITOR_EXPECT_EQ(ready.payload["backend"].asString(), std::string{"SOFTWARE"});
    CNA_EDITOR_EXPECT_EQ(ready.payload["entityCount"].asInt(), 1);
}

CNA_EDITOR_TEST(PlayerHostHonoursPauseStepAndResume)
{
    const ScratchProject project{"playerstep"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));

    PlayerHost::Outbox outbox;
    CNA_EDITOR_EXPECT(host.tick());
    CNA_EDITOR_EXPECT_EQ(host.getFrameCount(), std::uint64_t{1});

    EditorMessage pause;
    pause.type = EditorMessageType::Pause;
    host.handle(pause, outbox);
    CNA_EDITOR_EXPECT(host.getPlayState() == PlayState::Paused);

    // Paused means paused: ticking must not advance, or the game would drift ahead of what the
    // user is looking at in the inspector.
    CNA_EDITOR_EXPECT(!host.tick());
    CNA_EDITOR_EXPECT_EQ(host.getFrameCount(), std::uint64_t{1});

    EditorMessage step;
    step.type = EditorMessageType::StepFrame;
    host.handle(step, outbox);
    CNA_EDITOR_EXPECT(host.tick());
    CNA_EDITOR_EXPECT_EQ(host.getFrameCount(), std::uint64_t{2});
    // One step, one frame -- not a resume.
    CNA_EDITOR_EXPECT(!host.tick());

    EditorMessage resume;
    resume.type = EditorMessageType::Resume;
    host.handle(resume, outbox);
    CNA_EDITOR_EXPECT(host.tick());
    CNA_EDITOR_EXPECT_EQ(host.getFrameCount(), std::uint64_t{3});
}

CNA_EDITOR_TEST(PlayerHostIgnoresStepWhileRunning)
{
    const ScratchProject project{"playernostep"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));

    PlayerHost::Outbox outbox;
    EditorMessage step;
    step.type = EditorMessageType::StepFrame;
    host.handle(step, outbox);
    host.handle(step, outbox);

    // Stepping while running is meaningless; honouring it would make the game jump ahead.
    host.tick();
    CNA_EDITOR_EXPECT_EQ(host.getFrameCount(), std::uint64_t{1});
}

CNA_EDITOR_TEST(PlayerHostAppliesLiveSetProperty)
{
    const ScratchProject project{"playerlive"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));

    PlayerHost::Outbox outbox;
    host.handle(EditorMessage::makeSetProperty(project.entityId, BuiltinComponentIds::kTransform,
                                               "position", PropertyValue{EditorVector3{7.0f, 8.0f, 9.0f}}),
                outbox);

    const EditorComponent* transform =
        host.getScene().findEntity(project.entityId)->findComponent(BuiltinComponentIds::kTransform);
    CNA_EDITOR_EXPECT_EQ(transform->getProperty("position").get<EditorVector3>().x, 7.0f);
    CNA_EDITOR_EXPECT_EQ(transform->getProperty("position").get<EditorVector3>().z, 9.0f);
}

CNA_EDITOR_TEST(PlayerHostReportsRatherThanCrashingOnBadRequests)
{
    const ScratchProject project{"playerbad"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));

    PlayerHost::Outbox outbox;
    host.handle(EditorMessage::makeSetProperty(Uuid::generate(), BuiltinComponentIds::kTransform,
                                               "position", PropertyValue{EditorVector3{}}),
                outbox);
    host.handle(EditorMessage::makeSetProperty(project.entityId, "Nope.Missing",
                                               "x", PropertyValue{1.0f}),
                outbox);
    host.handle(EditorMessage::makeLoadScene(""), outbox);

    CNA_EDITOR_EXPECT_EQ(outbox.size(), std::size_t{3});
    for (const EditorMessage& reply : outbox)
    {
        CNA_EDITOR_EXPECT(reply.type == EditorMessageType::ReportLog);
    }
}

CNA_EDITOR_TEST(PlayerHostReportsAProtocolVersionMismatch)
{
    const ScratchProject project{"playerversion"};

    PlayerHost host;
    CNA_EDITOR_EXPECT(host.openProject(project.projectPath));

    PlayerHost::Outbox outbox;
    EditorMessage hello = EditorMessage::makeHello(project.directory.generic_string());
    hello.payload.set("protocolVersion", JsonValue{kEditorProtocolVersion + 99});
    host.handle(hello, outbox);

    // Reported, not fatal: the editor can then tell the user which build to rebuild, which it
    // could not do from a silently dropped connection.
    CNA_EDITOR_EXPECT_EQ(outbox.size(), std::size_t{1});
    CNA_EDITOR_EXPECT(!host.isHandshakeComplete());

    outbox.clear();
    host.handle(EditorMessage::makeHello(project.directory.generic_string()), outbox);
    CNA_EDITOR_EXPECT(host.isHandshakeComplete());
}

CNA_EDITOR_TEST(PlayerBuildDiscoveryFindsTheInstalledBinaries)
{
    // Discovery is a real feature, not a detail: because CNA fixes its backend at compile time,
    // "preview on Software" means "launch cna-player-software", and the editor must offer only
    // the backends whose binaries actually exist.
    const std::vector<PlayerBuild> builds = discoverPlayerBuilds(playerDirectory().generic_string());

    // At least one player must be discoverable, and every entry must name a real file. The
    // *backend* deliberately is not asserted: without CNA the binary is the un-suffixed
    // `cna-player` (reported as "default"), and with CNA it is `cna-player-<backend>`. Pinning
    // either would make this test pass in one configuration and fail in the other for no reason.
    CNA_EDITOR_EXPECT(!builds.empty());
    for (const PlayerBuild& build : builds)
    {
        CNA_EDITOR_EXPECT(!build.backend.empty());
        CNA_EDITOR_EXPECT(!build.executablePath.empty());
        CNA_EDITOR_EXPECT(std::filesystem::exists(build.executablePath));
    }

    CNA_EDITOR_EXPECT_EQ(discoverPlayerBuilds("/definitely/not/a/directory").size(), std::size_t{0});
}

CNA_EDITOR_TEST(EditorLaunchesARealPlayerProcessAndTalksToIt)
{
    // The end-to-end case: a real process, a real socket, real messages. Everything else in this
    // file could pass while play mode remained broken.
    const ScratchProject project{"bridge"};

    const std::vector<PlayerBuild> builds = discoverPlayerBuilds(playerDirectory().generic_string());
    CNA_EDITOR_EXPECT(!builds.empty());
    if (builds.empty()) { return; }

    PlayerProcess player;
    CNA_EDITOR_EXPECT(player.start(builds.front(), project.projectPath));
    if (!player.isRunning() && player.getExitReason() == PlayerExitReason::FailedToStart)
    {
        ::CnaEditorTest::reportFailure(__FILE__, __LINE__, "cannot start player: " + player.getError());
        return;
    }

    bool sawReady = false;
    bool sawSceneLog = false;
    bool sentLoad = false;

    for (int attempt = 0; attempt < 400 && !(sawReady && sawSceneLog); ++attempt)
    {
        for (const EditorMessage& message : player.poll())
        {
            if (message.type == EditorMessageType::Ready) { sawReady = true; }
            if (message.type == EditorMessageType::ReportLog
                && message.payload["text"].asString().find("loaded scene") != std::string::npos)
            {
                sawSceneLog = true;
            }
        }

        if (sawReady && !sentLoad)
        {
            CNA_EDITOR_EXPECT(player.send(EditorMessage::makeLoadScene("Scenes/Level01.cnascene")));
            sentLoad = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CNA_EDITOR_EXPECT(sawReady);

    // The player reports the backend it was compiled with -- "NONE" without CNA, the real backend
    // name with it. What matters is that *something* was reported: that is the mechanism the
    // editor uses to detect a mismatched launch, and an empty value would break it silently.
    CNA_EDITOR_EXPECT(!player.getReportedBackend().empty());
    CNA_EDITOR_EXPECT(sawSceneLog);

    player.stop();
    CNA_EDITOR_EXPECT(!player.isRunning());
    CNA_EDITOR_EXPECT(player.getExitReason() != PlayerExitReason::FailedToStart);
}

CNA_EDITOR_TEST(PlayerProcessReportsAMissingBinaryRatherThanHanging)
{
    const ScratchProject project{"bridgemissing"};

    PlayerBuild missing;
    missing.backend = "nonexistent";
    missing.executablePath = (project.directory / "cna-player-nonexistent").generic_string();

    PlayerProcess player;
    player.start(missing, project.projectPath);

    // On POSIX the fork succeeds and the exec fails in the child, so the failure surfaces as the
    // process exiting rather than as a spawn error. Either way the editor must notice and must
    // not sit waiting for a connection that will never arrive.
    for (int attempt = 0; attempt < 200 && player.isRunning(); ++attempt)
    {
        player.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CNA_EDITOR_EXPECT(!player.isRunning());
}
