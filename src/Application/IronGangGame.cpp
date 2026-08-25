// Iron Gang application entry point.
#include "IronGang/Application/IronGangGame.hpp"

#include "IronGang/Core/Log.hpp"
#include "IronGang/Persistence/SaveGame.hpp"
#include "IronGang/UI/BitmapFont.hpp"
#include "IronGang/UI/DistrictMap.hpp"

#include "CNA/Platform/IPlatform.hpp"
#include "CNA/Platform/NativeWindowHandle.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "System/TimeSpan.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace IronGang
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Input;

    namespace
    {
        // What the HUD and window title say about the mission: its current objective, or -- once a
        // state with "outcome": "failed" has been reached -- that state's own explanation
        // (plan_24 IG-24-009).
        std::string MissionStatusLine(const PrototypeMission& mission)
        {
            if (!mission.IsFailed())
            {
                return "Objective: " + mission.GetObjectiveText();
            }
            const std::string reason = mission.GetFailureReason();
            const std::string retryHint = " | R: retry";
            return reason.empty() ? "Mission failed" + retryHint : "Mission failed: " + reason + retryHint;
        }

#ifndef IRON_GANG_GRAPHICS_BACKEND
#define IRON_GANG_GRAPHICS_BACKEND "unknown"
#endif
#ifndef IRON_GANG_BUILD_CONFIGURATION
#define IRON_GANG_BUILD_CONFIGURATION "unknown"
#endif

        constexpr int kBackBufferWidth = 1280;
        constexpr int kBackBufferHeight = 720;
        // Gate M9: how far ahead (and how far sideways from dead-ahead) something must be to
        // count as an obstacle a TrafficVehicle should brake for -- a simplified stand-in for a
        // real forward collision sensor, matching this system's kinematic-mover scope.
        constexpr float kTrafficLaneHalfWidth = 2.0F;
        constexpr float kNoObstacleAhead = 1000.0F;
        // How close the player's vehicle must get to a pedestrian before it flees.
        constexpr float kPedestrianThreatRadius = 6.0F;
        // Where a dispatched patrol car first appears: off to the side of the player's own
        // vehicle spawn point, clear of the road and sidewalks.
        const Vector3 kPoliceSpawnOffset{10.0F, 0.0F, -6.0F};

        const char* DistrictName(DistrictId district) noexcept
        {
            switch (district)
            {
            case DistrictId::WarehouseBlock:
                return "warehouse_block";
            case DistrictId::Countryside:
                return "countryside";
            }
            return "unknown";
        }

        float DistanceAheadIfInLane(const Vector3& fromPosition, float fromYaw, const Vector3& obstaclePosition)
        {
            const Vector3 forward = ForwardFromYaw(fromYaw);
            Vector3 toObstacle = obstaclePosition - fromPosition;
            toObstacle.Y = 0.0F;
            const float forwardDistance = Vector3::Dot(forward, toObstacle);
            if (forwardDistance <= 0.0F)
            {
                return kNoObstacleAhead;
            }
            const Vector3 lateral = toObstacle - forward * forwardDistance;
            if (lateral.Length() > kTrafficLaneHalfWidth)
            {
                return kNoObstacleAhead;
            }
            return forwardDistance;
        }

        Color DistrictMapBoxColor(const WorldBox& box)
        {
            if (box.name.find("road") != std::string::npos)
            {
                return Color(70, 75, 82, 255);
            }
            if (box.name.find("sidewalk") != std::string::npos)
            {
                return Color(150, 145, 135, 255);
            }
            if (box.name.find("lane_marking") != std::string::npos)
            {
                return Color(225, 211, 173, 255);
            }
            if (box.name.find("target") != std::string::npos)
            {
                return Color(63, 190, 95, 255);
            }
            if (box.name.find("exit_marker") != std::string::npos)
            {
                return Color(120, 170, 230, 255);
            }
            return box.collidable
                ? box.color
                : Color(static_cast<int>(box.color.getRProperty()),
                        static_cast<int>(box.color.getGProperty()),
                        static_cast<int>(box.color.getBProperty()),
                        150);
        }

        void DrawMapLine(Graphics::SpriteBatch& spriteBatch,
                         const Graphics::Texture2D& pixel,
                         const Vector2& start,
                         const Vector2& end,
                         const Color& color,
                         float width)
        {
            const Vector2 delta = end - start;
            const float length = delta.Length();
            if (length < 0.5F)
            {
                return;
            }
            spriteBatch.Draw(pixel,
                             start,
                             std::nullopt,
                             color,
                             std::atan2(delta.Y, delta.X),
                             Vector2(0.0F, 0.5F),
                             Vector2(length, width),
                             Graphics::SpriteEffects::None,
                             0.0F);
        }
    }

    IronGangGame::IronGangGame(std::string assetRoot)
        : graphicsDeviceManager_(std::make_unique<GraphicsDeviceManager>(this)),
          assetRoot_(std::move(assetRoot))
    {
        graphicsDeviceManager_->setPreferredBackBufferWidthProperty(kBackBufferWidth);
        graphicsDeviceManager_->setPreferredBackBufferHeightProperty(kBackBufferHeight);
        graphicsDeviceManager_->setSynchronizeWithVerticalRetraceProperty(true);
        setTargetElapsedTimeProperty(System::TimeSpan::FromMilliseconds(1000.0 / 60.0));
        getWindowProperty().setTitleProperty("Iron Gang - starting");
        setIsMouseVisibleProperty(true);
    }

    const std::string& IronGangGame::GetTypeName() const
    {
        static const std::string typeName = "IronGang.IronGangGame";
        return typeName;
    }

    void IronGangGame::SetVerticalSync(bool enabled)
    {
        graphicsDeviceManager_->setSynchronizeWithVerticalRetraceProperty(enabled);
    }

    void IronGangGame::EnablePerformanceProfile(std::string reportPath)
    {
        performanceReportPath_ = std::move(reportPath);
        const bool enabled = !performanceReportPath_.empty();
        performanceProfiler_.SetEnabled(enabled);
        physics_.SetProfilingEnabled(enabled);
    }

    bool IronGangGame::WritePerformanceReport(std::string& error) const
    {
        if (performanceReportPath_.empty())
        {
            error.clear();
            return true;
        }
        if (performanceScenario_ == PerformanceScenario::Mission && !mission_.IsCompleted())
        {
            error = "Mission performance scenario did not complete; increase --smoke frames";
            return false;
        }

        PerformanceReportContext context;
        context.backend = IRON_GANG_GRAPHICS_BACKEND;
        context.buildConfiguration = IRON_GANG_BUILD_CONFIGURATION;
        context.scenario = PerformanceScenarioName(performanceScenario_);
        context.width = kBackBufferWidth;
        context.height = kBackBufferHeight;
        context.verticalSyncRequested = graphicsDeviceManager_->getSynchronizeWithVerticalRetraceProperty();
        context.requestedSwapInterval = context.verticalSyncRequested ? 1 : 0;
        const CNA::Platform::NativeWindowHandle nativeWindow =
            getWindowProperty().GetNativeWindowHandleEXT();
        context.nativeWindowSystem = CNA::Platform::ToString(nativeWindow.system);
        context.nativeWindowAvailable = CNA::Platform::HasNativeWindow(nativeWindow);
        context.graphicsRuntimeIdentityKnown = graphicsRuntimeIdentityKnown_;
        context.graphicsRuntimeVendor = graphicsRuntimeVendor_;
        context.graphicsRuntimeRenderer = graphicsRuntimeRenderer_;
        context.graphicsRuntimeVersion = graphicsRuntimeVersion_;
        context.graphicsRuntimeUnavailableReason = graphicsRuntimeUnavailableReason_;
        context.swapIntervalApplyResultKnown = swapIntervalApplyResultKnown_;
        context.swapIntervalApplySucceeded = swapIntervalApplySucceeded_;
        context.appliedSwapInterval = appliedSwapInterval_;
        context.swapIntervalUnavailableReason = swapIntervalUnavailableReason_;
        context.fixedTimeStep = getIsFixedTimeStepProperty();
        context.targetFrameMilliseconds = getTargetElapsedTimeProperty().getTotalMillisecondsProperty();
        context.peakResidentBytes = PerformanceProfiler::ReadPeakResidentBytes();
        const RendererVideoMemoryBreakdown videoMemory = renderer_.GetTrackedVideoMemory();
        const std::uint64_t hudAtlasBytes = static_cast<std::uint64_t>(kFont8x8AtlasWidth) *
            static_cast<std::uint64_t>(kFont8x8AtlasHeight) * sizeof(Color);
        context.trackedGameOwnedVideoMemoryBytes = videoMemory.gameOwnedBytes + hudAtlasBytes + sizeof(Color);
        context.trackedImportedModelBufferBytes = videoMemory.importedModels.bufferBytes;
        context.trackedImportedModelTextureBytes = videoMemory.importedModels.textureBytes;
        context.trackedVideoMemoryBytes = context.trackedGameOwnedVideoMemoryBytes +
            context.trackedImportedModelBufferBytes + context.trackedImportedModelTextureBytes;
        context.videoMemoryTrackingComplete = false;
        context.gpuTimerSupported = gpuFrameTimer_ && gpuFrameTimer_->IsSupported();
        context.gpuTimerUnsupportedReason = context.gpuTimerSupported
            ? std::string{}
            : (gpuFrameTimer_ ? gpuFrameTimer_->GetUnsupportedReason() : "GPU timer was not initialized");
        context.gpuTimerDiscardedSamples = gpuFrameTimer_ ? gpuFrameTimer_->GetDiscardedSampleCount() : 0;
        context.physicsBodyCount = peakPhysicsBodyCount_;
        context.trafficVehicleCount = peakTrafficVehicleCount_;
        context.pedestrianCount = peakPedestrianCount_;
        context.policeVehicleCount = peakPoliceVehicleCount_;
        return performanceProfiler_.WriteJsonReport(performanceReportPath_, context, error);
    }

    void IronGangGame::CaptureSwapIntervalAcceptance()
    {
        swapIntervalApplyResultKnown_ = false;
        swapIntervalApplySucceeded_ = false;
        appliedSwapInterval_.reset();
        swapIntervalUnavailableReason_.clear();

#if defined(__EMSCRIPTEN__)
        swapIntervalUnavailableReason_ =
            "browser presentation is compositor-controlled; WebGL has no qualifying swap-interval acknowledgement";
#elif defined(CNA_RENDERER_EASYGL)
        CNA::Platform::IPlatformGlContext* glContext = GetPlatformEXT().GetGlContext();
        if (glContext == nullptr)
        {
            swapIntervalUnavailableReason_ = "the active platform exposes no OpenGL context service";
            return;
        }

        const int requestedInterval =
            graphicsDeviceManager_->getSynchronizeWithVerticalRetraceProperty() ? 1 : 0;
        swapIntervalApplyResultKnown_ = true;
        swapIntervalApplySucceeded_ = glContext->SetSwapInterval(requestedInterval);
        if (swapIntervalApplySucceeded_)
        {
            appliedSwapInterval_ = requestedInterval;
        }
        else
        {
            swapIntervalUnavailableReason_ = "the platform declined the requested swap interval";
        }
#else
        swapIntervalUnavailableReason_ = "the active graphics backend does not use the OpenGL swap-interval seam";
#endif
    }

    void IronGangGame::CaptureGraphicsRuntimeIdentity()
    {
        graphicsRuntimeIdentityKnown_ = false;
        graphicsRuntimeVendor_.clear();
        graphicsRuntimeRenderer_.clear();
        graphicsRuntimeVersion_.clear();
        graphicsRuntimeUnavailableReason_.clear();

#if defined(CNA_RENDERER_EASYGL)
        CNA::Platform::IPlatformGlContext* glContext = GetPlatformEXT().GetGlContext();
        if (glContext == nullptr)
        {
            graphicsRuntimeUnavailableReason_ =
                "the active platform exposes no OpenGL context service";
            return;
        }
#if defined(_WIN32)
        using GlGetString = const unsigned char* (__stdcall*)(unsigned int);
#else
        using GlGetString = const unsigned char* (*)(unsigned int);
#endif
        const auto glGetString = reinterpret_cast<GlGetString>(glContext->GetProcAddress("glGetString"));
        if (glGetString == nullptr)
        {
            graphicsRuntimeUnavailableReason_ = "the current OpenGL context exposes no glGetString";
            return;
        }

        constexpr unsigned int kGlVendor = 0x1F00;
        constexpr unsigned int kGlRenderer = 0x1F01;
        constexpr unsigned int kGlVersion = 0x1F02;
        const unsigned char* vendor = glGetString(kGlVendor);
        const unsigned char* renderer = glGetString(kGlRenderer);
        const unsigned char* version = glGetString(kGlVersion);
        if (vendor == nullptr || renderer == nullptr || version == nullptr)
        {
            graphicsRuntimeUnavailableReason_ =
                "the current OpenGL context returned an incomplete identity";
            return;
        }

        graphicsRuntimeVendor_ = reinterpret_cast<const char*>(vendor);
        graphicsRuntimeRenderer_ = reinterpret_cast<const char*>(renderer);
        graphicsRuntimeVersion_ = reinterpret_cast<const char*>(version);
        if (graphicsRuntimeVendor_.empty() || graphicsRuntimeRenderer_.empty() ||
            graphicsRuntimeVersion_.empty())
        {
            graphicsRuntimeVendor_.clear();
            graphicsRuntimeRenderer_.clear();
            graphicsRuntimeVersion_.clear();
            graphicsRuntimeUnavailableReason_ =
                "the current OpenGL context returned an empty identity field";
            return;
        }
        graphicsRuntimeIdentityKnown_ = true;
#else
        graphicsRuntimeUnavailableReason_ =
            "the active graphics backend does not expose OpenGL context identity strings";
#endif
    }

    void IronGangGame::RecordRenderWorkload()
    {
        if (!performanceProfiler_.IsEnabled())
        {
            return;
        }
        const RenderWorkload& workload = renderer_.GetFrameWorkload();
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::DrawCalls, workload.drawCalls);
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::StateChanges, workload.stateChanges);
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::Vertices, workload.vertices);
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::Triangles, workload.triangles);
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::Instances, workload.instances);
        performanceProfiler_.RecordRenderWorkload(RenderWorkloadMetric::VisibleObjects, workload.visibleObjects);
    }

    void IronGangGame::RecordPhysicsWorkload()
    {
        if (!performanceProfiler_.IsEnabled())
        {
            return;
        }

        const Physics::PhysicsProfileSnapshot workload = physics_.CaptureProfileSnapshot();
        performanceProfiler_.RecordPhysicsWorkload(PhysicsWorkloadMetric::Bodies, workload.bodyCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::ActiveRigidBodies, workload.activeRigidBodyCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::RigidBodyContactManifolds, workload.rigidBodyContactManifoldCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::CharacterContacts, workload.characterContactCount);
        performanceProfiler_.RecordPhysicsWorkload(PhysicsWorkloadMetric::FixedSteps, workload.fixedStepCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::PublicRaycasts, workload.publicRaycastCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::CharacterCollisionUpdates, workload.characterCollisionUpdateCount);
        performanceProfiler_.RecordPhysicsWorkload(
            PhysicsWorkloadMetric::VehicleWheelRaycasts, workload.vehicleWheelRaycastCount);
    }

    std::uint64_t IronGangGame::GetTrackedRendererVideoMemoryBytes() const
    {
        const RendererVideoMemoryBreakdown memory = renderer_.GetTrackedVideoMemory();
        return static_cast<std::uint64_t>(memory.gameOwnedBytes) +
            static_cast<std::uint64_t>(memory.importedModels.bufferBytes) +
            static_cast<std::uint64_t>(memory.importedModels.textureBytes);
    }

    void IronGangGame::RecordDistrictLoadSample(const char* reason,
                                                DistrictId sourceDistrict,
                                                double worldPhysicsMilliseconds,
                                                double rendererUploadMilliseconds,
                                                std::uint64_t residentBytesBefore,
                                                std::uint64_t trackedVideoMemoryBytesBefore)
    {
        if (!performanceProfiler_.IsEnabled())
        {
            return;
        }

        const PrototypeWorld& world = districtManager_.GetWorld();
        DistrictLoadSample sample;
        sample.reason = reason;
        sample.sourceDistrict = DistrictName(sourceDistrict);
        sample.targetDistrict = DistrictName(world.GetId());
        sample.worldPhysicsMilliseconds = worldPhysicsMilliseconds;
        sample.rendererUploadMilliseconds = rendererUploadMilliseconds;
        sample.proceduralWorldObjectCount = world.GetBoxes().size();
        sample.staticPhysicsBodyCount = world.GetSolidColliders().size() + 1U; // one ground body
        sample.residentBytesBefore = residentBytesBefore;
        sample.residentBytesAfter = PerformanceProfiler::ReadCurrentResidentBytes();
        sample.trackedVideoMemoryBytesBefore = trackedVideoMemoryBytesBefore;
        sample.trackedVideoMemoryBytesAfter = GetTrackedRendererVideoMemoryBytes();
        performanceProfiler_.RecordDistrictLoad(std::move(sample));
    }

    void IronGangGame::Initialize()
    {
        ScopedPerformanceSample startupSample(performanceProfiler_, PerformanceMetric::StartupCpu);
        Game::Initialize();
        if (performanceProfiler_.IsEnabled())
        {
            CaptureSwapIntervalAcceptance();
            CaptureGraphicsRuntimeIdentity();
            gpuFrameTimer_ = std::make_unique<GpuFrameTimer>(getGraphicsDeviceProperty());
        }
        districtManager_.Initialize(physics_);
        player_.Reset(districtManager_.GetWorld().GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(districtManager_.GetWorld().GetVehicleSpawn(),
                       districtManager_.GetWorld().GetVehicleSpawnYaw(), physics_);

        // plan_04 IG-04-001: tunables come from data, but a missing or partly unusable file only
        // costs the tuning -- every field falls back to the default the game already runs on.
        std::vector<std::string> configWarnings;
        std::string configError;
        if (!LoadGameConfig(assetRoot_ + "/config/game.json", config_, configError, &configWarnings))
        {
            Log::Error(LogCategory::Config, configError + " -- using built-in configuration defaults.");
        }
        for (const std::string& warning : configWarnings)
        {
            Log::Warning(LogCategory::Config, warning);
        }
        autosave_.Configure(config_.autosaveIntervalSeconds, config_.autosaveMinimumSpacingSeconds);
        // An explicit --log-level wins: the run is being debugged right now, and the file is not.
        Log::SetMinimumSeverity(logSeverityOverride_.value_or(config_.logSeverity));

        // Gate M7 (plan_24-mission-framework-and-scripting.md IG-24-001/004): the mission's
        // states/objectives/transitions now live in data; PrototypeMission ships with a
        // hardcoded default reproducing the same flow, so a missing/corrupt file degrades to
        // that rather than breaking the mission.
        std::string missionError;
        if (!mission_.LoadMission(assetRoot_ + "/missions/prologue.mission.json", missionError))
        {
            Log::Warning(LogCategory::Mission, missionError + " -- using built-in fallback mission.");
        }
        mission_.Reset();

        std::string dialogueError;
        if (!dialogue_.LoadFromFile(assetRoot_ + "/dialogues/prologue.dialogue.txt", dialogueError))
        {
            Log::Warning(LogCategory::Dialogue, dialogueError + " -- using built-in fallback.");
            dialogue_.LoadFallbackPrologue();
        }
        dialogue_.Start();
        if (const DialogueLine* line = dialogue_.GetCurrentLine())
        {
            std::cout << line->speaker << ": " << line->text << '\n';
        }

        // Gate M8 (plan_26-cutscenes-and-cinematic-sequencing.md IG-26-001/003): a short,
        // skippable camera-only sequence panning from an establishing shot of the warehouse
        // delivery target to the exact framing of the normal gameplay follow-camera at the
        // player's spawn point (computed by hand to match Draw()'s own camera formula exactly,
        // so the cut back to gameplay has no visible pop). Falls back to an identical hardcoded
        // sequence (same convention as dialogue/mission) if the file fails to load.
        CutsceneSequence introSequence;
        std::string cutsceneError;
        if (!LoadCutsceneSequence(assetRoot_ + "/cutscenes/prologue_intro.cutscene.json", introSequence,
                                  cutsceneError))
        {
            Log::Warning(LogCategory::Cutscene, cutsceneError + " -- using built-in fallback cutscene.");
            introSequence = CutsceneSequence{
                "prologue_intro_fallback",
                1,
                2.5F,
                {
                    {0.0F, Vector3(25.0F, 12.0F, -34.0F), Vector3(0.0F, 2.0F, -34.0F)},
                    {2.5F, Vector3(0.0F, 4.65F, 27.5F), Vector3(0.0F, 1.25F, 20.0F)},
                }};
        }
        cutscene_.Start(std::move(introSequence));

        // Load the warehouse and sedan as generated CNJ models (MC3 -> glTF -> CNJ) if they have
        // been built via scripts/build-assets.sh; otherwise fall back to procedural geometry, so
        // a fresh checkout that has not run the asset pipeline still runs.
        getContentProperty().setRootDirectoryProperty(assetRoot_ + "/generated/models/cnj");

        std::optional<Graphics::Model> warehouseModel;
        try
        {
            warehouseModel = getContentProperty().Load<Graphics::Model>("warehouse");
            Log::Info(LogCategory::Assets, "Loaded generated warehouse.cnj");
        }
        catch (const std::exception& contentError)
        {
            Log::Warning(LogCategory::Assets,
                         std::string(contentError.what()) +
                             " -- using procedural warehouse box. Run scripts/build-assets.sh"
                             " assets/source/mc3/warehouse.mc3.xml assets/generated/models to"
                             " generate it.");
        }

        // The sedan is authored as four single-object MC3 files (body/cabin/windshield/wheel)
        // instead of one multi-object scene: the current MC3 -> glTF -> CNJ pipeline does not
        // bake per-object node transforms into vertex data, so a multi-part scene loaded as one
        // CNJ Model would lose each part's relative position (see PrototypeRenderer.hpp's
        // VehicleModelSet comment). Iron Gang composes the four parts itself instead. All four
        // must load for the sedan to use CNJ content; otherwise it stays fully procedural rather
        // than mixing generated and procedural parts.
        std::optional<VehicleModelSet> vehicleModels;
        try
        {
            VehicleModelSet models{
                getContentProperty().Load<Graphics::Model>("vehicle_body"),
                getContentProperty().Load<Graphics::Model>("vehicle_cabin"),
                getContentProperty().Load<Graphics::Model>("vehicle_windshield"),
                getContentProperty().Load<Graphics::Model>("vehicle_wheel")};
            vehicleModels = std::move(models);
            Log::Info(LogCategory::Assets, "Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj");
        }
        catch (const std::exception& contentError)
        {
            Log::Warning(LogCategory::Assets,
                         std::string(contentError.what()) +
                             " -- using procedural sedan. Run scripts/build-assets.sh"
                             " assets/source/mc3/vehicle_<part>.mc3.xml assets/generated/models"
                             " for body/cabin/windshield/wheel to generate it.");
        }

        // Gate M6: a hand-authored (not MC3 -- Mesh Craft has no rigging/skinning authoring
        // support) skinned test character, replacing the procedural on-foot player box when
        // available. See assets/source/gltf/test_character.gltf's own provenance note.
        std::optional<Graphics::Model> characterModel;
        try
        {
            characterModel = getContentProperty().Load<Graphics::Model>("test_character");
            Log::Info(LogCategory::Assets, "Loaded generated test_character.cnj");
        }
        catch (const std::exception& contentError)
        {
            Log::Warning(LogCategory::Assets,
                         std::string(contentError.what()) +
                             " -- using procedural player box. Run"
                             " cna_tool_gltf_to_cnj assets/source/gltf/test_character.gltf"
                             " assets/generated/models/cnj test_character 1.0 to generate it.");
        }

        renderer_.Initialize(getGraphicsDeviceProperty(), districtManager_.GetWorld(),
                             std::move(warehouseModel), std::move(vehicleModels), std::move(characterModel));
        RespawnTrafficAndPedestrians();

        // Gate M10: a real on-screen HUD (see BitmapFont.hpp for why this is a hand-built bitmap
        // font rather than a loaded asset). Always constructed -- there is no "missing asset"
        // fallback case here, unlike the CNJ models above.
        spriteBatch_.emplace(getGraphicsDeviceProperty());
        hudFont_.emplace(BuildBitmapFont8x8(getGraphicsDeviceProperty()));
        mapPixel_.emplace(getGraphicsDeviceProperty(), 1, 1);
        const Color whitePixel(255, 255, 255, 255);
        mapPixel_->SetData(&whitePixel, 1);

        // Gate M10 audio (plan_27): real CC0 sound assets (assets/licenses/asset-registry.csv),
        // each optional -- a missing file, or no audio hardware at all (NoAudioHardwareException,
        // a real risk in this sandboxed environment), degrades to silence, matching the same
        // try/catch-with-fallback convention used for every other optional asset above.
        try
        {
            engineSound_.emplace(assetRoot_ + "/audio/engine_loop.wav");
            engineSoundInstance_.emplace(engineSound_->CreateInstance());
            engineSoundInstance_->setIsLoopedProperty(true);
            Log::Info(LogCategory::Audio, "Loaded engine_loop.wav");
        }
        catch (const std::exception& audioError)
        {
            Log::Warning(LogCategory::Audio, std::string(audioError.what()) + " -- no engine sound.");
        }
        try
        {
            footstepSound_.emplace(assetRoot_ + "/audio/footstep.wav");
            Log::Info(LogCategory::Audio, "Loaded footstep.wav");
        }
        catch (const std::exception& audioError)
        {
            Log::Warning(LogCategory::Audio, std::string(audioError.what()) + " -- no footstep sound.");
        }
        try
        {
            hornSound_.emplace(assetRoot_ + "/audio/horn.wav");
            Log::Info(LogCategory::Audio, "Loaded horn.wav");
        }
        catch (const std::exception& audioError)
        {
            Log::Warning(LogCategory::Audio, std::string(audioError.what()) + " -- no horn sound.");
        }

        // Gameplay profiling workloads must reach control without synthetic keyboard events.
        // Ordinary smoke/play and the explicit intro scenario keep the real opening sequence;
        // idle/walk/drive/mixed take this deterministic shortcut through dialogue and cutscene.
        // Mission keeps and advances the real opening sequence as part of its end-to-end route.
        const bool skipsOpening = performanceScenario_ == PerformanceScenario::Idle ||
            performanceScenario_ == PerformanceScenario::Walk ||
            performanceScenario_ == PerformanceScenario::Drive ||
            performanceScenario_ == PerformanceScenario::Mixed;
        if (skipsOpening)
        {
            while (dialogue_.IsActive())
            {
                dialogue_.Advance();
            }
            cutscene_.Skip();
        }
        if (performanceScenario_ == PerformanceScenario::Drive)
        {
            playerDriving_ = true;
            player_.SetPosition(vehicle_.GetPosition(), physics_);
            player_.SetYaw(vehicle_.GetYaw(), physics_);
        }

        UpdateWindowTitle(10.0F);
    }

    void IronGangGame::RespawnTrafficAndPedestrians()
    {
        const PrototypeWorld& world = districtManager_.GetWorld();

        trafficVehicles_.clear();
        const WaypointPath& loop = world.GetTrafficLoop();
        if (!loop.Empty())
        {
            constexpr int kTrafficVehicleCount = 2;
            constexpr float kCruiseSpeed = 6.0F;
            for (int i = 0; i < kTrafficVehicleCount; ++i)
            {
                const std::size_t startIndex =
                    (loop.points.size() * static_cast<std::size_t>(i)) / static_cast<std::size_t>(kTrafficVehicleCount);
                TrafficVehicle vehicle;
                vehicle.Reset(loop, startIndex, kCruiseSpeed);
                trafficVehicles_.push_back(vehicle);
            }
        }

        pedestrians_.clear();
        constexpr float kWalkSpeed = 1.6F;
        for (const WaypointPath& sidewalk : world.GetSidewalkPaths())
        {
            Pedestrian pedestrian;
            pedestrian.Reset(sidewalk, 0, kWalkSpeed);
            pedestrians_.push_back(pedestrian);
        }

        police_.Reset();
        peakPhysicsBodyCount_ = std::max(peakPhysicsBodyCount_, physics_.GetBodyCount());
        peakTrafficVehicleCount_ = std::max(peakTrafficVehicleCount_, trafficVehicles_.size());
        peakPedestrianCount_ = std::max(peakPedestrianCount_, pedestrians_.size());
    }

    bool IronGangGame::WasPressed(const KeyboardState& current, Keys key) const
    {
        return current.IsKeyDown(key) && previousKeyboard_.IsKeyUp(key);
    }

    void IronGangGame::HandleInteraction()
    {
        // Gate M6: ignore a new interaction while an enter/exit clip is already playing, rather
        // than starting a second transition mid-animation.
        if (vehicleTransitionState_ != VehicleTransitionState::None)
        {
            return;
        }

        if (!playerDriving_)
        {
            if (DistanceSquaredXZ(player_.GetPosition(), vehicle_.GetPosition()) <= 9.0F)
            {
                // Stay visible and on-foot for kVehicleTransitionSeconds while "EnterVehicle"
                // plays; playerDriving_ only flips (hiding the character) once it finishes -- see
                // the vehicle-transition tick in Update().
                vehicleTransitionState_ = VehicleTransitionState::Entering;
                vehicleTransitionSecondsRemaining_ = kVehicleTransitionSeconds;
                player_.SetPosition(vehicle_.GetPosition(), physics_);
                transientStatus_ = "Entering sedan";
                transientStatusSeconds_ = 2.0F;
            }
            else
            {
                transientStatus_ = "Move closer to the sedan";
                transientStatusSeconds_ = 2.0F;
            }
            return;
        }

        const Vector3 right = RightFromYaw(vehicle_.GetYaw());
        Vector3 exitPosition = vehicle_.GetPosition() + right * 2.2F;
        exitPosition.Y = 1.70F;
        if (!districtManager_.GetWorld().CanOccupy(exitPosition, 0.35F))
        {
            exitPosition = vehicle_.GetPosition() - right * 2.2F;
            exitPosition.Y = 1.70F;
        }
        if (districtManager_.GetWorld().CanOccupy(exitPosition, 0.35F))
        {
            // Become visible immediately (playerDriving_ = false) so "ExitVehicle" is seen right
            // where the car is, then play it for kVehicleTransitionSeconds before normal on-foot
            // Walk/Idle selection resumes.
            playerDriving_ = false;
            player_.SetPosition(exitPosition, physics_);
            player_.SetYaw(vehicle_.GetYaw(), physics_);
            vehicleTransitionState_ = VehicleTransitionState::Exiting;
            vehicleTransitionSecondsRemaining_ = kVehicleTransitionSeconds;
            transientStatus_ = "Exited sedan";
        }
        else
        {
            transientStatus_ = "No safe space to exit";
        }
        transientStatusSeconds_ = 2.0F;
    }

    void IronGangGame::CheckDistrictExit()
    {
        if (districtManager_.IsTransitioning())
        {
            return;
        }
        const DistrictExit& exit = districtManager_.GetWorld().GetDistrictExit();
        const Vector3& checkPosition = playerDriving_ ? vehicle_.GetPosition() : player_.GetPosition();
        if (exit.trigger.bounds.ContainsXZ(checkPosition))
        {
            BeginDistrictTransition();
            transientStatus_.clear();
        }
    }

    void IronGangGame::BeginDistrictTransition()
    {
        pendingDistrictSource_ = districtManager_.GetWorld().GetId();
        pendingDistrictResidentBytesBefore_ = performanceProfiler_.IsEnabled()
            ? PerformanceProfiler::ReadCurrentResidentBytes()
            : 0;
        pendingDistrictVideoMemoryBytesBefore_ = performanceProfiler_.IsEnabled()
            ? GetTrackedRendererVideoMemoryBytes()
            : 0;
        const PerformanceProfiler::Clock::time_point loadStart = PerformanceProfiler::Clock::now();
        districtManager_.RequestTransition(physics_);
        pendingDistrictWorldPhysicsMilliseconds_ = std::chrono::duration<double, std::milli>(
                                                       PerformanceProfiler::Clock::now() - loadStart)
                                                       .count();
    }

    void IronGangGame::HandleDistrictArrival()
    {
        if (!districtManager_.ConsumeArrival())
        {
            return;
        }

        const PerformanceProfiler::Clock::time_point arrivalActivationStart = PerformanceProfiler::Clock::now();
        const PrototypeWorld& world = districtManager_.GetWorld();
        player_.Reset(world.GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(world.GetVehicleSpawn(), world.GetVehicleSpawnYaw(), physics_);
        if (playerDriving_)
        {
            // Carry the player's vehicle across (IG-13-018): if they were driving, keep them in
            // the car at its freshly-spawned position/yaw in the new district instead of leaving
            // them on foot at the pedestrian spawn point.
            player_.SetPosition(vehicle_.GetPosition(), physics_);
            player_.SetYaw(vehicle_.GetYaw(), physics_);
        }
        pendingDistrictWorldPhysicsMilliseconds_ += std::chrono::duration<double, std::milli>(
                                                        PerformanceProfiler::Clock::now() - arrivalActivationStart)
                                                        .count();

        const PerformanceProfiler::Clock::time_point renderLoadStart = PerformanceProfiler::Clock::now();
        renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), world);
        const double rendererUploadMilliseconds = std::chrono::duration<double, std::milli>(
                                                      PerformanceProfiler::Clock::now() - renderLoadStart)
                                                      .count();
        RecordDistrictLoadSample("exit_transition",
                                 pendingDistrictSource_,
                                 pendingDistrictWorldPhysicsMilliseconds_,
                                 rendererUploadMilliseconds,
                                 pendingDistrictResidentBytesBefore_,
                                 pendingDistrictVideoMemoryBytesBefore_);
        pendingDistrictWorldPhysicsMilliseconds_ = 0.0;
        pendingDistrictResidentBytesBefore_ = 0;
        pendingDistrictVideoMemoryBytesBefore_ = 0;
        RespawnTrafficAndPedestrians();
        autosave_.Request(AutosaveTrigger::DistrictArrival);
        transientStatus_ = "Arrived";
        transientStatusSeconds_ = 2.0F;
    }

    WorldStateSnapshot IronGangGame::CaptureWorldState() const
    {
        WorldStateSnapshot world;
        world.playerPosition = player_.GetPosition();
        world.playerYaw = player_.GetYaw();
        world.vehiclePosition = vehicle_.GetPosition();
        world.vehicleYaw = vehicle_.GetYaw();
        world.vehicleSpeed = vehicle_.GetSpeed();
        world.playerDriving = playerDriving_;
        world.districtId = districtManager_.GetWorld().GetId();
        return world;
    }

    SaveSnapshot IronGangGame::CaptureSnapshot() const
    {
        SaveSnapshot snapshot;
        static_cast<WorldStateSnapshot&>(snapshot) = CaptureWorldState();
        snapshot.missionStateId = mission_.GetStateId();
        snapshot.missionVariables = mission_.CaptureVariables();
        snapshot.missionCheckpoint = mission_.GetCheckpoint();
        snapshot.missionCheckpointWorld = missionCheckpointWorld_;
        return snapshot;
    }

    SaveBlockReason IronGangGame::CurrentSaveBlockReason() const
    {
        SaveConditions conditions;
        conditions.cutsceneActive = cutscene_.IsActive();
        conditions.dialogueActive = dialogue_.IsActive();
        conditions.districtTransitioning = districtManager_.IsTransitioning();
        conditions.vehicleTransitionActive = vehicleTransitionState_ != VehicleTransitionState::None;
        return FindSaveBlockReason(conditions);
    }

    void IronGangGame::SavePrototype()
    {
        // plan_29 IG-29-011: refusing with a reason beats writing a save that would come back
        // wrong, and beats silently doing nothing.
        const SaveBlockReason blocked = CurrentSaveBlockReason();
        if (blocked != SaveBlockReason::None)
        {
            transientStatus_ = std::string("Can't save: ") + DescribeSaveBlockReason(blocked);
            transientStatusSeconds_ = 3.0F;
            return;
        }

        std::string error;
        if (SaveGame::Write(SavePath(), CaptureSnapshot(), error))
        {
            transientStatus_ = "Saved prototype state";
            // A manual save resets the periodic timer: the player just did what it exists to do.
            autosave_.Reset();
        }
        else
        {
            transientStatus_ = "Save failed: " + error;
        }
        transientStatusSeconds_ = 3.0F;
    }

    void IronGangGame::WriteAutosave(AutosaveTrigger trigger)
    {
        std::string error;
        if (SaveGame::Write(AutosavePath(), CaptureSnapshot(), error))
        {
            transientStatus_ = std::string("Autosaved (") + DescribeAutosaveTrigger(trigger) + ")";
            transientStatusSeconds_ = 2.0F;
            return;
        }
        // An autosave the player did not ask for must not steal the status line on failure any
        // more than it does on success, but it must not fail silently either.
        Log::Error(LogCategory::Save, "autosave failed: " + error);
    }

    void IronGangGame::LoadPrototype()
    {
        std::string error;
        SaveReadDiagnostics saveDiagnostics;
        // "Load" means "resume", so the newest save wins whether the player wrote it or the
        // autosave did (plan_29 IG-29-010). SaveGame::Read still falls back to that file's own
        // backup if it turns out to be damaged.
        const std::string chosen = SaveGame::ChooseMostRecent({SavePath(), AutosavePath()});
        const std::string loadPath = chosen.empty() ? SavePath() : chosen;
        const std::optional<SaveSnapshot> snapshot = SaveGame::Read(loadPath, error, &saveDiagnostics);
        if (!snapshot)
        {
            transientStatus_ = "Load failed: " + error;
            transientStatusSeconds_ = 3.0F;
            return;
        }
        // plan_29 IG-29-003/004: recovering from a damaged save is not a silent event -- the
        // player has lost whatever happened between the backup and the save that failed.
        if (saveDiagnostics.usedBackup)
        {
            Log::Warning(LogCategory::Save, "the save file could not be read (" +
                                                saveDiagnostics.primaryError + "); loaded the backup instead.");
        }
        if (saveDiagnostics.formatVersion < kCurrentSaveFormatVersion)
        {
            Log::Info(LogCategory::Save, "migrated a format v" +
                                             std::to_string(saveDiagnostics.formatVersion) +
                                             " save; the next save will be written as v" +
                                             std::to_string(kCurrentSaveFormatVersion) + ".");
        }

        if (snapshot->districtId != districtManager_.GetWorld().GetId())
        {
            const DistrictId sourceDistrict = districtManager_.GetWorld().GetId();
            const std::uint64_t residentBytesBefore = performanceProfiler_.IsEnabled()
                ? PerformanceProfiler::ReadCurrentResidentBytes()
                : 0;
            const std::uint64_t videoMemoryBytesBefore = performanceProfiler_.IsEnabled()
                ? GetTrackedRendererVideoMemoryBytes()
                : 0;
            const PerformanceProfiler::Clock::time_point worldPhysicsStart = PerformanceProfiler::Clock::now();
            districtManager_.LoadDistrict(snapshot->districtId, physics_);
            const double worldPhysicsMilliseconds = std::chrono::duration<double, std::milli>(
                                                        PerformanceProfiler::Clock::now() - worldPhysicsStart)
                                                        .count();
            const PerformanceProfiler::Clock::time_point rendererUploadStart = PerformanceProfiler::Clock::now();
            renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), districtManager_.GetWorld());
            const double rendererUploadMilliseconds = std::chrono::duration<double, std::milli>(
                                                          PerformanceProfiler::Clock::now() - rendererUploadStart)
                                                          .count();
            RecordDistrictLoadSample("save_load",
                                     sourceDistrict,
                                     worldPhysicsMilliseconds,
                                     rendererUploadMilliseconds,
                                     residentBytesBefore,
                                     videoMemoryBytesBefore);
        }

        if (!mission_.SetStateId(snapshot->missionStateId))
        {
            // The save names a state the currently loaded mission file does not define. Resuming
            // there would strand the mission with no objective and no way out, so keep the state
            // the mission already has and say so (plan_24 IG-24-019).
            Log::Warning(LogCategory::Save, "save file mission state \"" + snapshot->missionStateId +
                                                "\" is not defined by the loaded mission; keeping \"" +
                                                mission_.GetStateId() + "\".");
        }
        // plan_24 IG-24-029: variables are restored, but entry actions deliberately are not
        // re-run (SetState's own comment). A variable the current mission file no longer declares
        // is reported and skipped rather than failing the load (IG-24-019).
        std::vector<std::string> missionVariableWarnings;
        mission_.ApplyVariables(snapshot->missionVariables, &missionVariableWarnings);
        mission_.ApplyCheckpoint(snapshot->missionCheckpoint, &missionVariableWarnings);
        // plan_29 IG-29-029: both halves of a checkpoint round-trip, so R works straight after a
        // load. A save from before the world half existed restores the mission half only, and a
        // retry then falls back to restarting the mission.
        missionCheckpointWorld_ = snapshot->missionCheckpointWorld;
        missionCheckpointWorldStateId_ =
            missionCheckpointWorld_.has_value() ? snapshot->missionCheckpoint.stateId : std::string();
        for (const std::string& warning : missionVariableWarnings)
        {
            Log::Warning(LogCategory::Save, "save file mission variable ignored: " + warning);
        }
        ApplyWorldSnapshot(*snapshot);
        const bool loadedAutosave = loadPath == AutosavePath();
        transientStatus_ = saveDiagnostics.usedBackup
                               ? "Loaded backup save"
                               : (loadedAutosave ? "Loaded autosave" : "Loaded prototype state");
        // The loaded state is now the newest thing worth keeping; start the interval afresh so an
        // autosave does not fire seconds later over a game the player has not played yet.
        autosave_.Reset();
        transientStatusSeconds_ = 3.0F;
    }

    void IronGangGame::CaptureMissionCheckpointWorld()
    {
        const MissionCheckpointSnapshot& checkpoint = mission_.GetCheckpoint();
        if (checkpoint.stateId.empty() || checkpoint.stateId == missionCheckpointWorldStateId_)
        {
            return;
        }

        // The same world state SavePrototype() records; the mission's own half was recorded by
        // PrototypeMission when it entered the checkpoint state.
        WorldStateSnapshot world;
        world.playerPosition = player_.GetPosition();
        world.playerYaw = player_.GetYaw();
        world.vehiclePosition = vehicle_.GetPosition();
        world.vehicleYaw = vehicle_.GetYaw();
        world.vehicleSpeed = vehicle_.GetSpeed();
        world.playerDriving = playerDriving_;
        world.districtId = districtManager_.GetWorld().GetId();
        missionCheckpointWorld_ = world;
        missionCheckpointWorldStateId_ = checkpoint.stateId;
        // A checkpoint the player never gets to reload is only half a checkpoint.
        autosave_.Request(AutosaveTrigger::Checkpoint);
    }

    void IronGangGame::ApplyWorldSnapshot(const WorldStateSnapshot& snapshot)
    {
        player_.Reset(snapshot.playerPosition, snapshot.playerYaw, physics_);
        vehicle_.Restore(snapshot.vehiclePosition, snapshot.vehicleYaw, snapshot.vehicleSpeed, physics_);
        playerDriving_ = snapshot.playerDriving;
        // A save is never taken mid-cutscene by design (nothing writes one there), but force the
        // cutscene to its terminal state anyway so restoring can never leave the game with an
        // active cutscene camera fighting a restored, unrelated player/vehicle position.
        cutscene_.Skip();
        vehicleTransitionState_ = VehicleTransitionState::None; // no mid-clip state is ever restored
        RespawnTrafficAndPedestrians(); // ambient traffic/pedestrian/police state is never saved
    }

    void IronGangGame::RetryMission()
    {
        if (!mission_.HasCheckpoint() || !missionCheckpointWorld_.has_value())
        {
            // Nothing to return to: a full restart is the only honest option.
            ResetPrototype();
            return;
        }

        mission_.Retry();

        if (missionCheckpointWorld_->districtId != districtManager_.GetWorld().GetId())
        {
            districtManager_.LoadDistrict(missionCheckpointWorld_->districtId, physics_);
            renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), districtManager_.GetWorld());
        }
        // Also resets police_, which matters here: the chase that caused the failure is still
        // running, and leaving it would re-trigger the same failure within a frame or two.
        ApplyWorldSnapshot(*missionCheckpointWorld_);
        transientStatus_ = "Retrying from checkpoint";
        transientStatusSeconds_ = 3.0F;
    }

    void IronGangGame::ResetPrototype()
    {
        missionCheckpointWorld_.reset();
        missionCheckpointWorldStateId_.clear();
        autosave_.Reset();
        const DistrictId sourceDistrict = districtManager_.GetWorld().GetId();
        const std::uint64_t residentBytesBefore = performanceProfiler_.IsEnabled()
            ? PerformanceProfiler::ReadCurrentResidentBytes()
            : 0;
        const std::uint64_t videoMemoryBytesBefore = performanceProfiler_.IsEnabled()
            ? GetTrackedRendererVideoMemoryBytes()
            : 0;
        const PerformanceProfiler::Clock::time_point worldPhysicsStart = PerformanceProfiler::Clock::now();
        districtManager_.LoadDistrict(DistrictId::WarehouseBlock, physics_);
        const double worldPhysicsMilliseconds = std::chrono::duration<double, std::milli>(
                                                    PerformanceProfiler::Clock::now() - worldPhysicsStart)
                                                    .count();
        const PerformanceProfiler::Clock::time_point rendererUploadStart = PerformanceProfiler::Clock::now();
        renderer_.RebuildStaticGeometry(getGraphicsDeviceProperty(), districtManager_.GetWorld());
        const double rendererUploadMilliseconds = std::chrono::duration<double, std::milli>(
                                                      PerformanceProfiler::Clock::now() - rendererUploadStart)
                                                      .count();
        RecordDistrictLoadSample("prototype_reset",
                                 sourceDistrict,
                                 worldPhysicsMilliseconds,
                                 rendererUploadMilliseconds,
                                 residentBytesBefore,
                                 videoMemoryBytesBefore);

        player_.Reset(districtManager_.GetWorld().GetPlayerSpawn(), 0.0F, physics_);
        vehicle_.Reset(districtManager_.GetWorld().GetVehicleSpawn(),
                       districtManager_.GetWorld().GetVehicleSpawnYaw(), physics_);
        mission_.Reset();
        dialogue_.Start();
        // The intro cutscene deliberately does not replay on reset (unlike dialogue) -- "R" is a
        // developer/QA shortcut back to a clean gameplay-ready state, not a full replay of the
        // opening cinematic; skipping straight to its terminal state keeps that state consistent.
        cutscene_.Skip();
        playerDriving_ = false;
        vehicleTransitionState_ = VehicleTransitionState::None;
        RespawnTrafficAndPedestrians();
        transientStatus_ = "Prototype reset";
        transientStatusSeconds_ = 2.0F;
    }

    std::string IronGangGame::SavePath() const
    {
        return "runtime/iron_gang_prototype.save";
    }

    std::string IronGangGame::AutosavePath() const
    {
        return "runtime/iron_gang_prototype.autosave";
    }

    void IronGangGame::Update(GameTime& gameTime)
    {
        ScopedPerformanceSample updateSample(performanceProfiler_, PerformanceMetric::UpdateCpu);
        double audioCpuMilliseconds = 0.0;
        bool physicsSampleRecorded = false;
        bool aiSampleRecorded = false;
        AiWorkloadSample aiWorkload;
        AudioWorkloadSample audioWorkload;
        const auto measureAudio = [&](auto&& operation)
        {
            if (!performanceProfiler_.IsEnabled())
            {
                operation();
                return;
            }
            const PerformanceProfiler::Clock::time_point start = PerformanceProfiler::Clock::now();
            operation();
            audioCpuMilliseconds += std::chrono::duration<double, std::milli>(
                                        PerformanceProfiler::Clock::now() - start)
                                        .count();
        };

        Game::Update(gameTime);
        const float deltaSeconds = static_cast<float>(
            gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        const KeyboardState keyboard = Keyboard::GetState();

        if (keyboard.IsKeyDown(Keys::Escape))
        {
            Exit();
            return;
        }

        if (WasPressed(keyboard, Keys::Tab))
        {
            mapVisible_ = !mapVisible_;
        }

        districtManager_.Update(deltaSeconds);
        HandleDistrictArrival();
        bool transitioning = districtManager_.IsTransitioning();

        // Representative M12 workload at a deterministic 60 Hz fixed update: two seconds of
        // walking, then driving, then one real district swap after eight seconds. It exercises
        // character/vehicle physics, footsteps/engine control, ambient AI, the loading screen,
        // static-body replacement, and renderer geometry rebuild in one bounded capture.
        if (performanceScenario_ == PerformanceScenario::Mixed && !transitioning)
        {
            if (performanceScenarioUpdate_ == 120)
            {
                playerDriving_ = true;
                player_.SetPosition(vehicle_.GetPosition(), physics_);
                player_.SetYaw(vehicle_.GetYaw(), physics_);
            }
            else if (performanceScenarioUpdate_ == 480)
            {
                BeginDistrictTransition();
                transitioning = true;
            }
        }

        // The mission workload retains the real dialogue/cutscene instead of skipping them. It
        // advances one dialogue line per simulated second, lets the 2.5-second camera track finish
        // naturally, then uses the same HandleInteraction/physics/mission paths as player input.
        const bool missionAdvancesDialogue =
            performanceScenario_ == PerformanceScenario::Mission &&
            dialogue_.IsActive() &&
            performanceScenarioUpdate_ > 0 &&
            performanceScenarioUpdate_ % 60 == 0;

        // Gate M8: ticks independently of dialogue's own pace -- the cutscene has its own short,
        // fixed duration and naturally hands control back on its own, but pressing Enter while
        // dialogue has already finished (a player racing through the opening) skips straight to
        // its terminal camera state instead (IG-26-004).
        if (!transitioning)
        {
            cutscene_.Update(deltaSeconds);
        }

        if (missionAdvancesDialogue || WasPressed(keyboard, Keys::Enter))
        {
            if (dialogue_.IsActive())
            {
                dialogue_.Advance();
                if (const DialogueLine* line = dialogue_.GetCurrentLine())
                {
                    std::cout << line->speaker << ": " << line->text << '\n';
                }
            }
            else if (cutscene_.IsActive())
            {
                cutscene_.Skip();
            }
        }

        const bool missionEntersVehicle =
            performanceScenario_ == PerformanceScenario::Mission &&
            mission_.IsInState("enter_vehicle") &&
            vehicleTransitionState_ == VehicleTransitionState::None &&
            !playerDriving_;
        if ((missionEntersVehicle || WasPressed(keyboard, Keys::E)) &&
            !dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning)
        {
            HandleInteraction();
        }
        if (WasPressed(keyboard, Keys::F5))
        {
            SavePrototype();
        }
        if (WasPressed(keyboard, Keys::F9))
        {
            LoadPrototype();
        }
        if (WasPressed(keyboard, Keys::R))
        {
            // A failed mission is the one case where "restart" should mean the mission's own last
            // checkpoint rather than the whole prototype (plan_24 IG-24-009).
            if (mission_.IsFailed())
            {
                RetryMission();
            }
            else
            {
                ResetPrototype();
            }
        }
        if (WasPressed(keyboard, Keys::H) && playerDriving_ && hornSound_)
        {
            measureAudio([&]()
            {
                ++audioWorkload.oneShotPlayRequests;
                if (hornSound_->Play())
                {
                    ++audioWorkload.oneShotPlaySuccesses;
                }
            });
        }

        if (!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning)
        {
            if (playerDriving_)
            {
                VehicleInput input;
                input.throttle = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.steering = (keyboard.IsKeyDown(Keys::D) || keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                                 (keyboard.IsKeyDown(Keys::A) || keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.handbrake = keyboard.IsKeyDown(Keys::Space);
                if (performanceScenario_ == PerformanceScenario::Drive ||
                    performanceScenario_ == PerformanceScenario::Mixed ||
                    performanceScenario_ == PerformanceScenario::Mission)
                {
                    input.throttle = mission_.IsCompleted() ? 0.0F : 0.65F;
                    input.steering = 0.0F;
                    input.handbrake = mission_.IsCompleted();
                }
                {
                    ScopedPerformanceSample physicsSample(performanceProfiler_, PerformanceMetric::PhysicsCpu);
                    vehicle_.Update(deltaSeconds, input, physics_);
                }
                physicsSampleRecorded = true;
                player_.SetPosition(vehicle_.GetPosition(), physics_);
                player_.SetYaw(vehicle_.GetYaw(), physics_);
            }
            else if (vehicleTransitionState_ == VehicleTransitionState::None)
            {
                // Suppressed while an enter/exit clip is playing (see the vehicle-transition tick
                // below) -- the character briefly stops responding to on-foot input during the
                // clip, matching how dialogue already freezes movement.
                OnFootInput input;
                input.forward = (keyboard.IsKeyDown(Keys::W) || keyboard.IsKeyDown(Keys::Up) ? 1.0F : 0.0F) -
                                (keyboard.IsKeyDown(Keys::S) || keyboard.IsKeyDown(Keys::Down) ? 1.0F : 0.0F);
                input.strafe = (keyboard.IsKeyDown(Keys::D) ? 1.0F : 0.0F) -
                               (keyboard.IsKeyDown(Keys::A) ? 1.0F : 0.0F);
                input.turn = (keyboard.IsKeyDown(Keys::Right) ? 1.0F : 0.0F) -
                             (keyboard.IsKeyDown(Keys::Left) ? 1.0F : 0.0F);
                input.sprint = keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
                if (performanceScenario_ == PerformanceScenario::Walk ||
                    performanceScenario_ == PerformanceScenario::Mixed ||
                    performanceScenario_ == PerformanceScenario::Mission)
                {
                    input.forward = 1.0F;
                    input.strafe = 0.0F;
                    input.turn = 0.0F;
                    input.sprint = false;
                }
                {
                    ScopedPerformanceSample physicsSample(performanceProfiler_, PerformanceMetric::PhysicsCpu);
                    player_.Update(deltaSeconds, input, physics_);
                }
                physicsSampleRecorded = true;

                // Gate M6: locomotion clip switching, crossfaded by the renderer's small
                // AnimationPlayer-backed state. A no-op if the skinned test character failed to load.
                const bool playerIsMoving = input.forward != 0.0F || input.strafe != 0.0F;
                renderer_.UpdateCharacterAnimation(deltaSeconds, playerIsMoving ? "Walk" : "Idle");

                // Gate M10 audio: a one-shot footstep SFX every kFootstepIntervalSeconds while
                // actually moving; the timer holds at the interval (not reset to 0) while
                // stationary so the next footstep plays immediately on resuming, not after a
                // fresh partial-interval wait.
                if (footstepSound_)
                {
                    measureAudio([&]()
                    {
                        if (playerIsMoving)
                        {
                            footstepTimer_ += deltaSeconds;
                            if (footstepTimer_ >= kFootstepIntervalSeconds)
                            {
                                footstepTimer_ -= kFootstepIntervalSeconds;
                                ++audioWorkload.oneShotPlayRequests;
                                if (footstepSound_->Play())
                                {
                                    ++audioWorkload.oneShotPlaySuccesses;
                                }
                            }
                        }
                        else
                        {
                            footstepTimer_ = kFootstepIntervalSeconds;
                        }
                    });
                }
            }
            if (vehicleTransitionState_ == VehicleTransitionState::None)
            {
                // Skipped mid-clip: position doesn't change during an enter/exit animation (input
                // is suppressed above), but avoid starting a second, unrelated transition on top
                // of an in-progress one regardless.
                CheckDistrictExit();
            }

            // Gate M10 audio: a looping engine sound tied to playerDriving_, with volume/pitch
            // scaling by speed for a bit of dynamism -- not real per-RPM engine modeling.
            if (engineSoundInstance_)
            {
                measureAudio([&]()
                {
                    if (playerDriving_)
                    {
                        if (engineSoundInstance_->getStateProperty() != Audio::SoundState::Playing)
                        {
                            ++audioWorkload.loopPlayCommands;
                            engineSoundInstance_->Play();
                        }
                        const float speedFactor = std::clamp(vehicle_.GetSpeedKph() / 80.0F, 0.0F, 1.0F);
                        engineSoundInstance_->setVolumeProperty(0.4F + 0.4F * speedFactor);
                        engineSoundInstance_->setPitchProperty(-0.15F + 0.3F * speedFactor);
                        audioWorkload.loopParameterUpdates += 2U;
                    }
                    else if (engineSoundInstance_->getStateProperty() == Audio::SoundState::Playing)
                    {
                        ++audioWorkload.loopStopCommands;
                        engineSoundInstance_->Stop();
                    }
                });
            }
        }

        // Gate M6 dialogue pose: the on-foot Walk/Idle call above only fires when dialogue is NOT
        // active (movement/physics are frozen during dialogue), so this covers the opposite case.
        // Not drawn while driving, so skipped there -- matches drawPlayer's own condition.
        if (!transitioning && dialogue_.IsActive() && !playerDriving_)
        {
            renderer_.UpdateCharacterAnimation(deltaSeconds, "Dialogue");
        }

        // Gate M6 vehicle entry/exit: play the one-shot clip for kVehicleTransitionSeconds, then
        // finalize (Entering hides the character by flipping playerDriving_ true; Exiting already
        // flipped playerDriving_ false in HandleInteraction(), so there is nothing left to flip).
        if (!transitioning && vehicleTransitionState_ != VehicleTransitionState::None)
        {
            const char* clipName =
                vehicleTransitionState_ == VehicleTransitionState::Entering ? "EnterVehicle" : "ExitVehicle";
            renderer_.UpdateCharacterAnimation(deltaSeconds, clipName);
            vehicleTransitionSecondsRemaining_ -= deltaSeconds;
            if (vehicleTransitionSecondsRemaining_ <= 0.0F)
            {
                if (vehicleTransitionState_ == VehicleTransitionState::Entering)
                {
                    playerDriving_ = true;
                }
                vehicleTransitionState_ = VehicleTransitionState::None;
            }
        }

        if (!transitioning)
        {
            // Keep mission progression outside the ambient-AI timer while retaining the same
            // update order and transition gate.
            {
                ScopedPerformanceSample aiSample(performanceProfiler_, PerformanceMetric::AiCpu);
                aiSampleRecorded = true;
                const std::size_t trafficCount = trafficVehicles_.size();
                const bool recordsAiWorkload = performanceProfiler_.IsEnabled();
                if (recordsAiWorkload)
                {
                    aiWorkload.trafficUpdates = trafficCount;
                    aiWorkload.trafficObstacleChecks =
                        trafficCount > 0
                            ? trafficCount * (trafficCount - 1U) + (playerDriving_ ? trafficCount : 0U)
                            : 0U;
                    aiWorkload.pedestrianUpdates = pedestrians_.size();
                    aiWorkload.pedestrianThreatChecks = playerDriving_ ? pedestrians_.size() : 0U;
                }
                // Gate M9: traffic/pedestrians/police keep ticking through dialogue and cutscenes
                // (ambient city life, matching Mafia 1's own feel) -- only a district transition
                // suspends them, same as everything else gated on `transitioning` in this function.
                for (std::size_t i = 0; i < trafficVehicles_.size(); ++i)
                {
                    const Vector3 myPosition = trafficVehicles_[i].GetPosition();
                    const float myYaw = trafficVehicles_[i].GetYaw();
                    float obstacleDistance = kNoObstacleAhead;
                    for (std::size_t j = 0; j < trafficVehicles_.size(); ++j)
                    {
                        if (i == j)
                        {
                            continue;
                        }
                        obstacleDistance = std::min(
                            obstacleDistance,
                            DistanceAheadIfInLane(myPosition, myYaw, trafficVehicles_[j].GetPosition()));
                    }
                    if (playerDriving_)
                    {
                        obstacleDistance =
                            std::min(obstacleDistance,
                                     DistanceAheadIfInLane(myPosition, myYaw, vehicle_.GetPosition()));
                    }
                    trafficVehicles_[i].Update(deltaSeconds, obstacleDistance);
                }

                std::vector<Vector3> witnessPositions;
                witnessPositions.reserve(trafficVehicles_.size() + pedestrians_.size());
                for (const TrafficVehicle& trafficVehicle : trafficVehicles_)
                {
                    witnessPositions.push_back(trafficVehicle.GetPosition());
                }
                for (const Pedestrian& pedestrian : pedestrians_)
                {
                    witnessPositions.push_back(pedestrian.GetPosition());
                }

                for (Pedestrian& pedestrian : pedestrians_)
                {
                    const bool hasThreat =
                        playerDriving_ &&
                        DistanceSquaredXZ(pedestrian.GetPosition(), vehicle_.GetPosition()) <=
                            kPedestrianThreatRadius * kPedestrianThreatRadius;
                    pedestrian.Update(deltaSeconds, hasThreat, vehicle_.GetPosition());
                }

                const PoliceUpdateWorkload policeWorkload = police_.Update(
                    deltaSeconds, playerDriving_, vehicle_.GetPosition(), vehicle_.GetSpeedKph(),
                    witnessPositions, districtManager_.GetWorld().GetVehicleSpawn() + kPoliceSpawnOffset);
                if (recordsAiWorkload)
                {
                    aiWorkload.policeWitnessChecks = policeWorkload.witnessChecks;
                    aiWorkload.policePatrolUpdates = policeWorkload.patrolUpdates;
                }
                peakPoliceVehicleCount_ = std::max(peakPoliceVehicleCount_, police_.GetActivePatrolCount());
            }

            // plan_24 IG-24-006: PoliceSystem is the game's, not the mission's, so its wanted
            // state is pushed in as facts rather than derived inside PrototypeMission::Update().
            const PoliceState policeState = police_.GetState();
            std::string factError;
            if (!mission_.SetFact("police_alerted", MissionValue::Bool(policeState != PoliceState::Clear),
                                  factError) ||
                !mission_.SetFact("police_chasing", MissionValue::Bool(policeState == PoliceState::Chasing),
                                  factError) ||
                !mission_.SetFact("police_chase_seconds", MissionValue::Float(police_.GetChaseSeconds()),
                                  factError))
            {
                Log::Error(LogCategory::Mission, "could not publish police mission facts: " + factError);
            }

            mission_.Update(dialogue_.IsFinished(), player_.GetPosition(), vehicle_.GetPosition(),
                            playerDriving_, districtManager_.GetWorld().GetWarehouseGoal());
            CaptureMissionCheckpointWorld();
            if (performanceScenario_ == PerformanceScenario::Mission && mission_.IsCompleted())
            {
                // End at the exact successful mission boundary. Continuing toward the fixed
                // --smoke limit would let the still-moving sedan reach the district exit and
                // contaminate this dedicated mission workload with a district transition.
                Exit();
            }
        }

        // plan_29 IG-29-010: one place decides when an autosave happens, and it holds the request
        // rather than dropping it while the game is somewhere a save would come back wrong.
        const AutosaveTrigger autosaveTrigger = autosave_.Update(deltaSeconds, CurrentSaveBlockReason());
        if (autosaveTrigger != AutosaveTrigger::None)
        {
            WriteAutosave(autosaveTrigger);
        }

        if (transientStatusSeconds_ > 0.0F)
        {
            transientStatusSeconds_ -= deltaSeconds;
        }
        UpdateWindowTitle(deltaSeconds);
        previousKeyboard_ = keyboard;
        if (!physicsSampleRecorded)
        {
            performanceProfiler_.Record(PerformanceMetric::PhysicsCpu, 0.0);
        }
        if (!aiSampleRecorded)
        {
            performanceProfiler_.Record(PerformanceMetric::AiCpu, 0.0);
        }
        performanceProfiler_.Record(PerformanceMetric::AudioCpu, audioCpuMilliseconds);
        if (performanceProfiler_.IsEnabled())
        {
            audioWorkload.loadedSoundAssets =
                static_cast<std::uint64_t>(engineSound_.has_value()) +
                static_cast<std::uint64_t>(footstepSound_.has_value()) +
                static_cast<std::uint64_t>(hornSound_.has_value());
            audioWorkload.trackedLoopInstances = static_cast<std::uint64_t>(engineSoundInstance_.has_value());
            audioWorkload.trackedPlayingLoopVoices = static_cast<std::uint64_t>(
                engineSoundInstance_ &&
                engineSoundInstance_->getStateProperty() == Audio::SoundState::Playing);
            performanceProfiler_.RecordAudioWorkload(audioWorkload);
        }
        RecordPhysicsWorkload();
        if (performanceProfiler_.IsEnabled())
        {
            aiWorkload.trafficVehicles = trafficVehicles_.size();
            aiWorkload.pedestrians = pedestrians_.size();
            aiWorkload.fleeingPedestrians = static_cast<std::uint64_t>(std::count_if(
                pedestrians_.begin(), pedestrians_.end(), [](const Pedestrian& pedestrian)
                {
                    return pedestrian.IsFleeing();
                }));
            aiWorkload.policePatrols = static_cast<std::uint64_t>(police_.GetActivePatrolCount());
            performanceProfiler_.RecordAiWorkload(aiWorkload);
        }
        if (performanceScenario_ != PerformanceScenario::InteractiveOrIntro)
        {
            ++performanceScenarioUpdate_;
        }
    }

    void IronGangGame::UpdateWindowTitle(float deltaSeconds)
    {
        titleRefreshTimer_ -= deltaSeconds;
        if (titleRefreshTimer_ > 0.0F)
        {
            return;
        }
        titleRefreshTimer_ = 0.20F;

        std::ostringstream title;
        title << config_.projectName << " | ";
        if (districtManager_.IsTransitioning())
        {
            title << "Loading...";
        }
        else if (const DialogueLine* line = dialogue_.GetCurrentLine())
        {
            title << line->speaker << ": " << line->text << " | Enter: continue";
        }
        else if (cutscene_.IsActive())
        {
            title << "..." << " | Enter: skip";
        }
        else
        {
            title << (playerDriving_ ? "Driving" : "On foot") << " | " << MissionStatusLine(mission_);
            if (playerDriving_)
            {
                title << " | " << static_cast<int>(std::round(vehicle_.GetSpeedKph())) << " km/h";
            }
            // Gate M9: surfaces the police state machine's own progression (Dispatched is the
            // brief "en route" delay before Chasing actually starts moving patrol cars).
            if (police_.GetState() == PoliceState::Dispatched)
            {
                title << " | Police dispatched...";
            }
            else if (police_.GetState() == PoliceState::Chasing)
            {
                title << " | WANTED";
            }
            if (transientStatusSeconds_ > 0.0F && !transientStatus_.empty())
            {
                title << " | " << transientStatus_;
            }
        }
        getWindowProperty().setTitleProperty(title.str());
    }

    void IronGangGame::DrawDistrictMap(Graphics::SpriteBatch& spriteBatch,
                                       Graphics::SpriteFont& font,
                                       Graphics::Texture2D& pixel,
                                       int viewportWidth,
                                       int viewportHeight) const
    {
        const int panelSize = std::clamp(std::min(viewportWidth - 80, viewportHeight - 80), 240, 640);
        const int panelX = (viewportWidth - panelSize) / 2;
        const int panelY = (viewportHeight - panelSize) / 2;
        const Rectangle panel(panelX, panelY, panelSize, panelSize);
        const int mapSize = panelSize - 104;
        const Rectangle mapBounds(panelX + (panelSize - mapSize) / 2, panelY + 48, mapSize, mapSize);

        spriteBatch.Draw(pixel, panel, Color(12, 18, 24, 238));
        spriteBatch.Draw(pixel, mapBounds, Color(40, 55, 43, 255));

        const PrototypeWorld& world = districtManager_.GetWorld();
        const DistrictMapProjection projection = BuildDistrictMapProjection(world.GetBoxes(), mapBounds);
        for (const WorldBox& box : world.GetBoxes())
        {
            spriteBatch.Draw(pixel, projection.ProjectBox(box), DistrictMapBoxColor(box));
        }

        const Vector3 activePosition = playerDriving_ ? vehicle_.GetPosition() : player_.GetPosition();
        const Vector2 activePoint = projection.ProjectPoint(activePosition);
        const Vector2 exitPoint = projection.ProjectPoint(world.GetDistrictExit().trigger.bounds.center);
        DrawMapLine(spriteBatch, pixel, activePoint, exitPoint, Color(100, 175, 245, 180), 2.0F);

        const auto drawMarker = [&](const Vector3& position, const Color& color, int size)
        {
            const Vector2 point = projection.ProjectPoint(position);
            spriteBatch.Draw(pixel,
                             Rectangle(static_cast<int>(std::lround(point.X)) - size / 2,
                                       static_cast<int>(std::lround(point.Y)) - size / 2,
                                       size,
                                       size),
                             color);
        };
        drawMarker(world.GetDistrictExit().trigger.bounds.center, Color(100, 175, 245, 255), 9);
        if (world.GetWarehouseGoal().id != "none")
        {
            drawMarker(world.GetWarehouseGoal().bounds.center, Color(75, 230, 115, 255), 9);
        }
        drawMarker(vehicle_.GetPosition(), Color(245, 175, 65, 255), 9);
        drawMarker(activePosition, Color(60, 225, 235, 255), 11);

        std::ostringstream title;
        title << config_.cityName << " " << config_.prototypeYear << " | " << DistrictName(world.GetId())
              << " | TAB: CLOSE";
        spriteBatch.DrawString(font, title.str(), Vector2(static_cast<float>(panelX + 16),
                                                          static_cast<float>(panelY + 14)),
                               Color(240, 240, 230, 255));
        spriteBatch.DrawString(font, "N", Vector2(static_cast<float>(mapBounds.X + mapBounds.Width - 12),
                                                   static_cast<float>(mapBounds.Y + 4)),
                               Color(240, 240, 230, 255));
        spriteBatch.DrawString(font, "CYAN YOU  AMBER CAR  GREEN TARGET  BLUE EXIT",
                               Vector2(static_cast<float>(panelX + 16),
                                       static_cast<float>(panelY + panelSize - 28)),
                               Color(205, 210, 205, 255));
    }

    std::string_view IronGangGame::CurrentPerformancePhase() const noexcept
    {
        if (districtManager_.IsTransitioning())
        {
            return "district_transition";
        }
        switch (performanceScenario_)
        {
        case PerformanceScenario::InteractiveOrIntro:
            return "interactive";
        case PerformanceScenario::Intro:
            return "intro";
        case PerformanceScenario::Idle:
            return "idle";
        case PerformanceScenario::Walk:
            return "walk";
        case PerformanceScenario::Drive:
            return "drive";
        case PerformanceScenario::Mixed:
            return playerDriving_ ? "mixed_drive" : "mixed_walk";
        case PerformanceScenario::Mission:
            if (dialogue_.IsActive())
            {
                return "mission_dialogue";
            }
            if (cutscene_.IsActive())
            {
                return "mission_cutscene";
            }
            if (vehicleTransitionState_ != VehicleTransitionState::None)
            {
                return "mission_vehicle_transition";
            }
            return playerDriving_ ? "mission_drive" : "mission_walk";
        }
        return "unknown";
    }

    void IronGangGame::Draw(const GameTime& gameTime)
    {
        const std::optional<std::uint64_t> scenarioUpdate =
            performanceScenario_ == PerformanceScenario::InteractiveOrIntro
                ? std::nullopt
                : std::optional<std::uint64_t>(
                      static_cast<std::uint64_t>(performanceScenarioUpdate_));
        performanceProfiler_.BeginFrame(CurrentPerformancePhase(), scenarioUpdate);
        if (performanceProfiler_.IsEnabled())
        {
            renderer_.BeginFrameWorkloadTracking();
        }
        ScopedPerformanceSample renderSample(performanceProfiler_, PerformanceMetric::RenderCpu);
        (void)gameTime;
        Graphics::GraphicsDevice& device = getGraphicsDeviceProperty();
        if (gpuFrameTimer_)
        {
            double gpuMilliseconds = 0.0;
            if (gpuFrameTimer_->Poll(gpuMilliseconds))
            {
                performanceProfiler_.Record(PerformanceMetric::GpuRender, gpuMilliseconds);
            }
            gpuFrameTimer_->Begin();
        }

        // IG-13-013: a solid-color loading screen for a district transition's minimum display
        // time. The 3D scene is deliberately not drawn here -- physics has already swapped to
        // the new district's static bodies by this point (DistrictManager::RequestTransition()
        // runs synchronously), so the old renderer geometry would be stale/mismatched.
        if (districtManager_.IsTransitioning())
        {
            device.Clear(Color(18, 18, 22, 255), 1.0F);
            if (smokeFramesRemaining_ > 0)
            {
                --smokeFramesRemaining_;
                if (smokeFramesRemaining_ == 0)
                {
                    Exit();
                }
            }
            if (gpuFrameTimer_)
            {
                gpuFrameTimer_->End();
            }
            RecordRenderWorkload();
            return;
        }

        device.Clear(Color(112, 145, 164, 255), 1.0F);
        device.SetDepthTestEnabled(true);

        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
            ? static_cast<float>(viewport.getWidthProperty()) /
              static_cast<float>(viewport.getHeightProperty())
            : 1.0F;

        Vector3 target;
        Vector3 camera;
        if (cutscene_.IsActive())
        {
            // Gate M8: the intro cutscene's own camera keyframes override the normal
            // player/vehicle follow camera entirely while it plays. Its final keyframe is
            // authored to exactly match the "else" branch below at the player's spawn position,
            // so the cut back to gameplay has no visible pop.
            camera = cutscene_.GetCameraPosition();
            target = cutscene_.GetCameraLookAt();
        }
        else if (playerDriving_)
        {
            target = vehicle_.GetPosition() + Vector3(0.0F, 1.0F, 0.0F);
            camera = target - vehicle_.GetForward() * 10.5F + Vector3(0.0F, 4.8F, 0.0F);
        }
        else
        {
            target = player_.GetPosition() + Vector3(0.0F, -0.45F, 0.0F);
            camera = target - player_.GetForward() * 7.5F + Vector3(0.0F, 3.4F, 0.0F);
        }

        const Matrix view = Matrix::CreateLookAt(camera, target, Vector3::Up);
        const Matrix projection = Matrix::CreatePerspectiveFieldOfView(
            std::numbers::pi_v<float> / 4.0F,
            aspect,
            0.1F,
            250.0F);

        renderer_.Draw(device,
                       view,
                       projection,
                       player_.GetPosition(),
                       player_.GetYaw(),
                       !playerDriving_,
                       vehicle_.GetPosition(),
                       vehicle_.GetYaw());

        // Gate M9: drawn even mid-cutscene/dialogue (ambient city life keeps moving, see the
        // matching tick comment in Update()) -- only suppressed during a district transition,
        // which this whole function already returns early for above.
        std::vector<ActorPose> trafficPoses;
        trafficPoses.reserve(trafficVehicles_.size());
        for (const TrafficVehicle& trafficVehicle : trafficVehicles_)
        {
            trafficPoses.push_back({trafficVehicle.GetPosition(), trafficVehicle.GetYaw()});
        }

        std::vector<ActorPose> pedestrianPoses;
        pedestrianPoses.reserve(pedestrians_.size());
        for (const Pedestrian& pedestrian : pedestrians_)
        {
            pedestrianPoses.push_back({pedestrian.GetPosition(), pedestrian.GetYaw()});
        }

        std::vector<ActorPose> policePoses;
        policePoses.reserve(static_cast<std::size_t>(police_.GetActivePatrolCount()));
        for (int i = 0; i < police_.GetActivePatrolCount(); ++i)
        {
            policePoses.push_back({police_.GetPatrolPosition(i), police_.GetPatrolYaw(i)});
        }

        renderer_.DrawTraffic(device, view, projection, trafficPoses, pedestrianPoses, policePoses);

        // Gate M10: a real on-screen HUD, replacing the window-title-only display (which stays,
        // for window-manager/taskbar visibility, but is no longer the only place this shows).
        if (spriteBatch_ && hudFont_)
        {
            spriteBatch_->Begin();
            float lineY = 10.0F;
            constexpr float kLineHeight = 12.0F;

            if (const DialogueLine* line = dialogue_.GetCurrentLine())
            {
                spriteBatch_->DrawString(*hudFont_, line->speaker + ": " + line->text, Vector2(10.0F, lineY),
                                        Color(255, 255, 255, 255));
                lineY += kLineHeight;
                spriteBatch_->DrawString(*hudFont_, "Enter: continue", Vector2(10.0F, lineY),
                                        Color(200, 200, 200, 255));
            }
            else if (cutscene_.IsActive())
            {
                spriteBatch_->DrawString(*hudFont_, "Enter: skip", Vector2(10.0F, lineY), Color(200, 200, 200, 255));
            }
            else
            {
                std::ostringstream objective;
                objective << (playerDriving_ ? "Driving" : "On foot") << " | " << MissionStatusLine(mission_);
                spriteBatch_->DrawString(*hudFont_, objective.str(), Vector2(10.0F, lineY), Color(255, 255, 255, 255));
                lineY += kLineHeight;

                if (playerDriving_)
                {
                    std::ostringstream speed;
                    speed << static_cast<int>(std::round(vehicle_.GetSpeedKph())) << " km/h";
                    spriteBatch_->DrawString(*hudFont_, speed.str(), Vector2(10.0F, lineY), Color(255, 255, 255, 255));
                    lineY += kLineHeight;
                }

                if (police_.GetState() == PoliceState::Dispatched)
                {
                    spriteBatch_->DrawString(*hudFont_, "Police dispatched...", Vector2(10.0F, lineY),
                                            Color(255, 200, 80, 255));
                    lineY += kLineHeight;
                }
                else if (police_.GetState() == PoliceState::Chasing)
                {
                    spriteBatch_->DrawString(*hudFont_, "WANTED", Vector2(10.0F, lineY), Color(230, 60, 60, 255));
                    lineY += kLineHeight;
                }

                if (transientStatusSeconds_ > 0.0F && !transientStatus_.empty())
                {
                    spriteBatch_->DrawString(*hudFont_, transientStatus_, Vector2(10.0F, lineY),
                                            Color(210, 210, 160, 255));
                }
            }
            if (mapVisible_ && mapPixel_)
            {
                DrawDistrictMap(*spriteBatch_, *hudFont_, *mapPixel_,
                                viewport.getWidthProperty(), viewport.getHeightProperty());
            }
            spriteBatch_->End();
        }

        if (smokeFramesRemaining_ > 0)
        {
            --smokeFramesRemaining_;
            if (smokeFramesRemaining_ == 0)
            {
                Exit();
            }
        }
        if (gpuFrameTimer_)
        {
            gpuFrameTimer_->End();
        }
        RecordRenderWorkload();
    }

    void IronGangGame::EndDraw()
    {
        // CNA's base EndDraw() routes to GraphicsDeviceManager::EndDraw() and then Present().
        // Keeping this separate from Draw() distinguishes CPU submission from swap/v-sync/GPU
        // back-pressure without changing the renderer or relying on backend-specific timers.
        ScopedPerformanceSample presentSample(performanceProfiler_, PerformanceMetric::PresentCpu);
        Game::EndDraw();
    }
}
