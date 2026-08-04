// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/RuntimeBridge/EditorProtocol.hpp
 * @brief The message protocol between the editor and a `cna-player` process.
 *
 * Play mode runs the game in a **separate process**, never inside the editor (ANALYSIS.md
 * decision D-09). For CNA specifically this is not merely defensive -- it is forced. CNA picks its
 * graphics backend at *compile* time (finding F-01), so the editor binary and the game binary are
 * linked against different CNA builds and cannot share an address space at all. What would be a
 * robustness nicety in another engine is a structural requirement here.
 *
 * The wire format is one JSON object per line over a stream socket. Line-delimited JSON is chosen
 * over a binary format because the traffic is a handful of messages per second, and because being
 * able to watch the session with `nc` is worth far more during bring-up than the bytes saved.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Editor/Core/Json.hpp"
#include "CNA/Editor/Core/PropertyValue.hpp"
#include "CNA/Editor/Core/Uuid.hpp"

namespace CNA::Editor
{
    /** @brief Message kinds. Editor-to-player and player-to-editor share one enumeration. */
    enum class EditorMessageType
    {
        Unknown,

        // Editor -> player.
        /** @brief Handshake. Carries the protocol version and the project root. */
        Hello,
        /** @brief Load a scene file by project-relative path. */
        LoadScene,
        /** @brief Re-read one asset by id, without restarting the game. */
        ReloadAsset,
        /** @brief Apply one property change to a live entity. */
        SetProperty,
        Pause,
        Resume,
        /** @brief Advance exactly one frame while paused. */
        StepFrame,
        /** @brief Highlight an entity in the running game. */
        SelectEntity,
        /** @brief Capture the current frame and write it to the given path. */
        Screenshot,
        /** @brief Ask the player to shut down cleanly. */
        Quit,

        // Player -> editor.
        /** @brief Handshake reply. Carries the player's CNA backend and capabilities. */
        Ready,
        /** @brief An exception escaped the game loop. Carries the message and, if available, a trace. */
        ReportException,
        /** @brief A line from the game's logger. */
        ReportLog,
        /** @brief Periodic frame timing, for the profiler panel. */
        ReportFrameStats,
        /** @brief A requested screenshot is written and ready to read. */
        ScreenshotReady
    };

    /** @brief Returns the wire name of @p type, e.g. "loadScene". */
    const char* toString(EditorMessageType type);

    /** @brief Parses the wire name produced by toString(). */
    EditorMessageType parseEditorMessageType(std::string_view text);

    /**
     * @brief One protocol message.
     *
     * @c payload carries the type-specific fields; the protocol deliberately does not model them
     * as a C++ variant, because both ends must tolerate a peer built from a different revision
     * that sends fields they do not know. An unknown field is ignored, never fatal.
     */
    struct EditorMessage
    {
        EditorMessageType type = EditorMessageType::Unknown;

        /**
         * @brief Correlates a reply with its request. Zero means "unsolicited".
         *
         * Needed because the player answers out of order: a Screenshot taken during a paused
         * frame completes long after a SetProperty sent afterwards.
         */
        std::uint64_t requestId = 0;

        JsonValue payload;

        /** @brief Serialises to the single-line JSON form sent on the wire. */
        [[nodiscard]] std::string encode() const;

        /**
         * @brief Parses one line received from the wire.
         * @return The message, or std::nullopt when @p line is not valid JSON or has no type.
         */
        static std::optional<EditorMessage> decode(std::string_view line);

        /** @brief Builds a Hello message. */
        static EditorMessage makeHello(const std::string& projectRoot);

        /** @brief Builds a LoadScene message for a project-relative @p scenePath. */
        static EditorMessage makeLoadScene(const std::string& scenePath);

        /**
         * @brief Builds a ReloadAsset message for one asset id.
         *
         * By id rather than by path, like everything else on this wire and everywhere else in the
         * editor (D-08). The player resolves it through the same database the editor scanned, so
         * there is one answer to "what is this asset" on both sides -- and a reload survives the
         * file being renamed between the change and the message.
         */
        static EditorMessage makeReloadAsset(const Uuid& assetId);

        /** @brief Builds a SetProperty message targeting one property of one live entity. */
        static EditorMessage makeSetProperty(const Uuid& entityId,
                                             const std::string& componentTypeId,
                                             const std::string& propertyName,
                                             const PropertyValue& value);

        /**
         * @brief Builds a Screenshot message asking the player to capture a frame to @p path.
         *
         * The path is the *player's* filesystem, which is the same machine today and may not be
         * once ED-512's remote preview exists; the reply says whether it was written rather than
         * leaving the asking side to look for the file, since a stale file from an earlier run
         * answers that question wrongly.
         */
        static EditorMessage makeScreenshot(const std::string& path);

        /** @brief Builds a ReportLog message. */
        static EditorMessage makeReportLog(const std::string& severity, const std::string& text);
    };

    /** @brief The protocol version this build speaks; sent in Hello and checked in Ready. */
    inline constexpr int kEditorProtocolVersion = 1;

    /**
     * @brief Splits an incoming byte stream into complete newline-delimited messages.
     *
     * A stream socket delivers arbitrary chunks, so a reader that assumes one recv() equals one
     * message works right up until a message straddles a packet boundary and then fails in a way
     * that is very hard to reproduce. Feeding everything through this class removes that class of
     * bug entirely.
     */
    class MessageStreamDecoder
    {
    public:
        /**
         * @brief Appends @p bytes and returns every message that is now complete.
         *
         * A trailing partial line is retained for the next call.
         */
        std::vector<EditorMessage> feed(std::string_view bytes);

        /** @brief Returns the number of lines dropped for being unparseable. */
        [[nodiscard]] std::uint64_t getDroppedCount() const { return droppedCount_; }

        /** @brief Discards any buffered partial line. Called when a connection resets. */
        void reset();

    private:
        std::string buffer_;
        std::uint64_t droppedCount_ = 0;
    };
}
