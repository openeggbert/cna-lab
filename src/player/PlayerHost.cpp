// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Player/PlayerHost.hpp"

#include "CNA/Editor/Scene/BuiltinComponents.hpp"

namespace CNA::Editor
{
    const char* toString(PlayState state)
    {
        switch (state)
        {
            case PlayState::Running: return "running";
            case PlayState::Paused: return "paused";
            case PlayState::Stopping: return "stopping";
        }
        return "running";
    }

    PlayerHost::PlayerHost()
    {
        registerBuiltinComponents(components_);
    }

    PlayerHost::~PlayerHost() = default;

    bool PlayerHost::openProject(const std::string& projectPath)
    {
        const ProjectLoadResult loaded = project_.loadFromFile(projectPath);
        if (!loaded.succeeded)
        {
            error_ = loaded.errorMessage;
            return false;
        }

        assets_.setProjectRoot(project_.getRootPath());
        assets_.scan(project_.getAssetDirectory());

        // The startup scene is loaded eagerly so that a player launched without an explicit
        // LoadScene still shows the game rather than an empty window.
        if (project_.getKind() == ProjectKind::CnaNative && !project_.getStartupScene().empty())
        {
            const std::string path = project_.resolvePath(project_.getStartupScene());
            if (scene_.loadFromFile(path, components_).succeeded) { scenePath_ = path; }
        }
        return true;
    }

    EditorMessage PlayerHost::makeReady(const std::string& backendName) const
    {
        EditorMessage message;
        message.type = EditorMessageType::Ready;
        message.payload = JsonValue::makeObject();
        message.payload.set("protocolVersion", JsonValue{kEditorProtocolVersion});
        // The backend is what the editor most needs to know: it is fixed at compile time, so this
        // is the only way the editor learns which build it actually got hold of.
        message.payload.set("backend", JsonValue{backendName});
        message.payload.set("project", JsonValue{project_.getName()});
        message.payload.set("scene", JsonValue{scene_.getName()});
        message.payload.set("entityCount", JsonValue{static_cast<std::int64_t>(scene_.getEntityCount())});
        return message;
    }

    void PlayerHost::handle(const EditorMessage& message, Outbox& outbox)
    {
        switch (message.type)
        {
            case EditorMessageType::Hello: {
                const int peerVersion = message.payload["protocolVersion"].asInt(0);
                if (peerVersion != kEditorProtocolVersion)
                {
                    // A version mismatch is reported and the session continues in a degraded
                    // state rather than being torn down: the editor can then tell the user which
                    // build to rebuild, which it could not do from a silently dropped connection.
                    outbox.push_back(EditorMessage::makeReportLog(
                        "error", "protocol version mismatch: editor speaks " + std::to_string(peerVersion)
                                     + ", this player speaks " + std::to_string(kEditorProtocolVersion)));
                }
                else
                {
                    handshakeComplete_ = true;
                }
                break;
            }

            case EditorMessageType::LoadScene:
                handleLoadScene(message, outbox);
                break;

            case EditorMessageType::SetProperty:
                handleSetProperty(message, outbox);
                break;

            case EditorMessageType::ReloadAsset: {
                const Uuid id = Uuid::parse(message.payload["assetId"].asString());
                assets_.scan(project_.getAssetDirectory());
                outbox.push_back(EditorMessage::makeReportLog(
                    "info", "reloaded asset " + (id.isValid() ? id.toString() : std::string{"<all>"})));
                break;
            }

            case EditorMessageType::Pause:
                playState_ = PlayState::Paused;
                pendingSteps_ = 0;
                outbox.push_back(EditorMessage::makeReportLog("info", "paused"));
                break;

            case EditorMessageType::Resume:
                playState_ = PlayState::Running;
                pendingSteps_ = 0;
                outbox.push_back(EditorMessage::makeReportLog("info", "resumed"));
                break;

            case EditorMessageType::StepFrame:
                // Stepping while already running is meaningless, and honouring it would make the
                // game jump a frame ahead of where the user is looking.
                if (playState_ == PlayState::Paused) { ++pendingSteps_; }
                break;

            case EditorMessageType::SelectEntity:
                highlightedEntity_ = Uuid::parse(message.payload["entityId"].asString());
                break;

            case EditorMessageType::Quit:
                playState_ = PlayState::Stopping;
                break;

            case EditorMessageType::Screenshot: {
                // The capture itself belongs to the CNA-linked main loop, which owns the device.
                // Acknowledging here keeps the request/reply correlation honest even when the
                // player is built without graphics.
                EditorMessage reply;
                reply.type = EditorMessageType::ScreenshotReady;
                reply.requestId = message.requestId;
                reply.payload = JsonValue::makeObject();
                reply.payload.set("path", message.payload["path"]);
                outbox.push_back(std::move(reply));
                break;
            }

            // Player-to-editor messages: a peer sending one of these is confused, but saying so
            // is more useful than silently ignoring it.
            case EditorMessageType::Ready:
            case EditorMessageType::ReportException:
            case EditorMessageType::ReportLog:
            case EditorMessageType::ReportFrameStats:
            case EditorMessageType::ScreenshotReady:
            case EditorMessageType::Unknown:
                outbox.push_back(EditorMessage::makeReportLog(
                    "warn", std::string{"ignoring unexpected message '"} + toString(message.type) + "'"));
                break;
        }
    }

    void PlayerHost::handleLoadScene(const EditorMessage& message, Outbox& outbox)
    {
        const std::string relativePath = message.payload["scenePath"].asString();
        if (relativePath.empty())
        {
            outbox.push_back(EditorMessage::makeReportLog("error", "loadScene without a scenePath"));
            return;
        }

        const std::string path = project_.resolvePath(relativePath);
        const SceneLoadResult loaded = scene_.loadFromFile(path, components_);
        if (!loaded.succeeded)
        {
            outbox.push_back(EditorMessage::makeReportLog("error", "cannot load scene: " + loaded.errorMessage));
            return;
        }

        scenePath_ = path;
        highlightedEntity_ = Uuid{};
        for (const std::string& warning : loaded.warnings)
        {
            outbox.push_back(EditorMessage::makeReportLog("warn", warning));
        }
        outbox.push_back(EditorMessage::makeReportLog(
            "info", "loaded scene '" + scene_.getName() + "' with "
                        + std::to_string(scene_.getEntityCount()) + " entities"));
    }

    void PlayerHost::handleSetProperty(const EditorMessage& message, Outbox& outbox)
    {
        const Uuid entityId = Uuid::parse(message.payload["entityId"].asString());
        const std::string componentTypeId = message.payload["component"].asString();
        const std::string propertyName = message.payload["property"].asString();

        EditorEntity* entity = scene_.findEntity(entityId);
        if (entity == nullptr)
        {
            outbox.push_back(EditorMessage::makeReportLog(
                "warn", "setProperty for unknown entity " + entityId.toString()));
            return;
        }

        EditorComponent* component = entity->findComponent(componentTypeId);
        if (component == nullptr)
        {
            outbox.push_back(EditorMessage::makeReportLog(
                "warn", "setProperty for missing component '" + componentTypeId + "' on '"
                            + entity->getName() + "'"));
            return;
        }

        // The wire carries the value's type, unlike a scene file, precisely because the player's
        // registry may not match the editor's after a plugin reload. Trusting the wire here is
        // what makes live editing survive that mismatch.
        const PropertyType type = parsePropertyType(message.payload["valueType"].asString());
        component->setProperty(propertyName, PropertyValue::fromJson(message.payload["value"], type));
    }

    bool PlayerHost::tick()
    {
        if (playState_ == PlayState::Stopping) { return false; }

        if (playState_ == PlayState::Paused)
        {
            if (pendingSteps_ <= 0) { return false; }
            --pendingSteps_;
        }

        ++frameCount_;
        return true;
    }
}
