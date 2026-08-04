// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/RuntimeBridge/EditorProtocol.hpp"

#include <array>

namespace CNA::Editor
{
    namespace
    {
        struct MessageTypeName
        {
            EditorMessageType type;
            const char* name;
        };

        constexpr std::array<MessageTypeName, 16> kMessageTypeNames{{
            {EditorMessageType::Unknown, "unknown"},
            {EditorMessageType::Hello, "hello"},
            {EditorMessageType::LoadScene, "loadScene"},
            {EditorMessageType::ReloadAsset, "reloadAsset"},
            {EditorMessageType::SetProperty, "setProperty"},
            {EditorMessageType::Pause, "pause"},
            {EditorMessageType::Resume, "resume"},
            {EditorMessageType::StepFrame, "stepFrame"},
            {EditorMessageType::SelectEntity, "selectEntity"},
            {EditorMessageType::Screenshot, "screenshot"},
            {EditorMessageType::Quit, "quit"},
            {EditorMessageType::Ready, "ready"},
            {EditorMessageType::ReportException, "reportException"},
            {EditorMessageType::ReportLog, "reportLog"},
            {EditorMessageType::ReportFrameStats, "reportFrameStats"},
            {EditorMessageType::ScreenshotReady, "screenshotReady"},
        }};
    }

    const char* toString(EditorMessageType type)
    {
        for (const auto& entry : kMessageTypeNames)
        {
            if (entry.type == type) { return entry.name; }
        }
        return "unknown";
    }

    EditorMessageType parseEditorMessageType(std::string_view text)
    {
        for (const auto& entry : kMessageTypeNames)
        {
            if (text == entry.name) { return entry.type; }
        }
        return EditorMessageType::Unknown;
    }

    std::string EditorMessage::encode() const
    {
        JsonValue json = JsonValue::makeObject();
        json.set("type", JsonValue{toString(type)});
        if (requestId != 0) { json.set("requestId", JsonValue{static_cast<std::int64_t>(requestId)}); }
        if (!payload.isNull()) { json.set("payload", payload); }

        // Compact, single-line: the framing is the newline, so the body must not contain one.
        return Json::write(json, false) + "\n";
    }

    std::optional<EditorMessage> EditorMessage::decode(std::string_view line)
    {
        const JsonParseResult parsed = Json::parse(line);
        if (!parsed.succeeded || !parsed.value.isObject()) { return std::nullopt; }

        EditorMessage message;
        message.type = parseEditorMessageType(parsed.value["type"].asString());
        if (message.type == EditorMessageType::Unknown) { return std::nullopt; }

        message.requestId = static_cast<std::uint64_t>(parsed.value["requestId"].asNumber(0.0));
        message.payload = parsed.value["payload"];
        return message;
    }

    EditorMessage EditorMessage::makeHello(const std::string& projectRoot)
    {
        EditorMessage message;
        message.type = EditorMessageType::Hello;
        message.payload = JsonValue::makeObject();
        message.payload.set("protocolVersion", JsonValue{kEditorProtocolVersion});
        message.payload.set("projectRoot", JsonValue{projectRoot});
        return message;
    }

    EditorMessage EditorMessage::makeLoadScene(const std::string& scenePath)
    {
        EditorMessage message;
        message.type = EditorMessageType::LoadScene;
        message.payload = JsonValue::makeObject();
        message.payload.set("scenePath", JsonValue{scenePath});
        return message;
    }

    EditorMessage EditorMessage::makeReloadAsset(const Uuid& assetId)
    {
        EditorMessage message;
        message.type = EditorMessageType::ReloadAsset;
        message.payload = JsonValue::makeObject();
        message.payload.set("assetId", JsonValue{assetId.toString()});
        return message;
    }

    EditorMessage EditorMessage::makeScreenshot(const std::string& path)
    {
        EditorMessage message;
        message.type = EditorMessageType::Screenshot;
        message.payload = JsonValue::makeObject();
        message.payload.set("path", JsonValue{path});
        return message;
    }

    EditorMessage EditorMessage::makeSetProperty(const Uuid& entityId,
                                                 const std::string& componentTypeId,
                                                 const std::string& propertyName,
                                                 const PropertyValue& value)
    {
        EditorMessage message;
        message.type = EditorMessageType::SetProperty;
        message.payload = JsonValue::makeObject();
        message.payload.set("entityId", JsonValue{entityId.toString()});
        message.payload.set("component", JsonValue{componentTypeId});
        message.payload.set("property", JsonValue{propertyName});
        // The type travels with the value here, unlike in a scene file: the player resolves the
        // component's schema from its own registry, which may not match the editor's after a
        // plugin reload, so the wire must be self-describing.
        message.payload.set("valueType", JsonValue{toString(value.getType())});
        message.payload.set("value", value.toJson());
        return message;
    }

    EditorMessage EditorMessage::makeReportLog(const std::string& severity, const std::string& text)
    {
        EditorMessage message;
        message.type = EditorMessageType::ReportLog;
        message.payload = JsonValue::makeObject();
        message.payload.set("severity", JsonValue{severity});
        message.payload.set("text", JsonValue{text});
        return message;
    }

    std::vector<EditorMessage> MessageStreamDecoder::feed(std::string_view bytes)
    {
        buffer_.append(bytes);

        std::vector<EditorMessage> messages;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t newline = buffer_.find('\n', start);
            if (newline == std::string::npos) { break; }

            std::string_view line{buffer_.data() + start, newline - start};
            if (!line.empty() && line.back() == '\r') { line.remove_suffix(1); }

            if (!line.empty())
            {
                if (std::optional<EditorMessage> message = EditorMessage::decode(line))
                {
                    messages.push_back(std::move(*message));
                }
                else
                {
                    // A malformed line is counted and skipped rather than closing the connection:
                    // a peer from a newer revision sending something unrecognised must not be able
                    // to kill a play session.
                    ++droppedCount_;
                }
            }
            start = newline + 1;
        }

        buffer_.erase(0, start);
        return messages;
    }

    void MessageStreamDecoder::reset()
    {
        buffer_.clear();
    }
}
