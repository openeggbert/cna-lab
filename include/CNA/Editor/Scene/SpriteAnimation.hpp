// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/SpriteAnimation.hpp
 * @brief A sprite animation clip, and the playback state a preview needs.
 *
 * plan.md ED-303. A frame is an **index into a sheet**, not a rectangle: a sheet is a uniform grid
 * in every case anyone authors by hand, an index is far smaller to author, and it is the same
 * arithmetic the tilemap already does. A rectangle-per-frame form is a strictly larger feature and
 * is worth adding when a real sheet needs it -- not before.
 *
 * Playback is deliberately **not** part of the document. `AnimationPlayback` is a plain value the
 * previewing panel owns and throws away; a scene that recorded which frame an artist happened to be
 * looking at would carry that into every save and every diff (ANALYSIS.md decision D-07).
 */

#include <cstdint>
#include <vector>

#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/PropertyValue.hpp"

namespace CNA::Editor
{
    class EditorComponent;

    /** @brief Property names on `CNA.SpriteAnimation`. */
    namespace SpriteAnimationKeys
    {
        inline constexpr const char* kSheet = "sheet";
        inline constexpr const char* kFrameWidth = "frameWidth";
        inline constexpr const char* kFrameHeight = "frameHeight";
        inline constexpr const char* kSheetColumns = "sheetColumns";
        inline constexpr const char* kFrames = "frames";
        inline constexpr const char* kFramesPerSecond = "framesPerSecond";
        inline constexpr const char* kLoop = "loop";
    }

    /** @brief One animation clip, read out of its component. */
    struct SpriteAnimationClip
    {
        /** @brief Frame indices into the sheet, in playback order. */
        std::vector<std::int64_t> frames;

        int frameWidth = 0;
        int frameHeight = 0;
        int sheetColumns = 1;
        float framesPerSecond = 12.0f;
        bool loop = true;

        [[nodiscard]] std::size_t getFrameCount() const { return frames.size(); }
        [[nodiscard]] bool isEmpty() const { return frames.empty() || frameWidth <= 0 || frameHeight <= 0; }

        /** @brief Returns the clip's length in seconds, or zero when it cannot play. */
        [[nodiscard]] float getDuration() const;

        /**
         * @brief Returns the source rectangle for the frame at @p position in the list.
         *
         * An out-of-range position answers an empty rectangle rather than clamping: a caller that
         * has lost track of where it is should draw nothing, not silently draw frame zero.
         */
        [[nodiscard]] EditorRectangle getFrameRectangle(std::size_t position) const;
    };

    /** @brief Reads the clip from @p component. */
    [[nodiscard]] SpriteAnimationClip readSpriteAnimationClip(const EditorComponent& component,
                                                              const ComponentDescriptor* descriptor);

    /**
     * @brief Where a preview has got to. Editor state, never serialised.
     *
     * Held by whichever panel is previewing and discarded with it. Putting this in the document
     * would mean every save recorded the frame an artist happened to be paused on.
     */
    struct AnimationPlayback
    {
        /** @brief Position in the clip's frame list, not the frame's index into the sheet. */
        std::size_t position = 0;

        /** @brief Seconds accumulated toward the next frame. */
        float elapsed = 0.0f;

        bool playing = false;

        /**
         * @brief Advances by @p deltaSeconds, honouring @p clip's rate and loop flag.
         *
         * The clock is passed in rather than read, so a test steps it exactly and never sleeps --
         * the same rule the asset watcher follows.
         *
         * @return True when the displayed frame changed, so a caller can avoid redrawing.
         */
        bool advance(const SpriteAnimationClip& clip, float deltaSeconds);

        /** @brief Moves @p delta frames, wrapping within the clip. Stops playback. */
        void step(const SpriteAnimationClip& clip, int delta);

        /** @brief Clamps the position into @p clip, which a shortened frame list can invalidate. */
        void clampTo(const SpriteAnimationClip& clip);
    };
}
