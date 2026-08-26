#pragma once

#include <string>
#include <vector>

namespace IronGang
{
    // plan_25 IG-25-003: a real subtitle, not a HUD line.
    //
    // Dialogue used to be drawn as a single unwrapped string at the top-left corner, which the
    // first screenshots caught running off the right edge mid-word ("...before the river shift
    // change" with the rest of the line simply gone). A subtitle has to wrap, sit where subtitles
    // sit, and stay readable over whatever the camera happens to be pointing at.
    //
    // The layout is computed here, as arithmetic over a fixed-width font, so it can be tested
    // without a graphics device -- the same split PrototypeRenderer and ScreenshotSummary use.

    // Wraps @p text to at most @p maxCharactersPerLine characters per line, breaking on spaces.
    // A word longer than a whole line is hard-split rather than allowed to overflow: losing a
    // hyphen is better than losing the end of a sentence off the edge of the screen. Runs of
    // spaces collapse. Never returns an empty line, and never drops a character.
    [[nodiscard]] std::vector<std::string> WrapSubtitleText(const std::string& text,
                                                            std::size_t maxCharactersPerLine);

    // Everything the renderer needs to draw one subtitle, in pixels.
    struct SubtitleLayout
    {
        // The speaker's name, and the wrapped body lines beneath it.
        std::string speaker;
        std::vector<std::string> lines;
        // The dimmed panel behind the text, so a light-coloured wall behind the camera cannot make
        // the subtitle unreadable.
        float panelX{0.0F};
        float panelY{0.0F};
        float panelWidth{0.0F};
        float panelHeight{0.0F};
        // Where the first line of text starts. Successive lines are lineHeight apart.
        float textX{0.0F};
        float textY{0.0F};
        float lineHeight{0.0F};
        // Scale to pass to DrawString; the 8x8 bitmap font is unreadable at 1:1 on a 720p screen.
        float scale{1.0F};

        [[nodiscard]] bool IsEmpty() const noexcept { return lines.empty(); }
    };

    // Fraction of the screen width a subtitle may occupy. Wider than this and the eye has to track
    // across the whole frame; narrower and a long line becomes a paragraph.
    inline constexpr float kSubtitleWidthFraction = 0.72F;
    // Distance from the bottom of the screen to the bottom of the panel, as a fraction of height.
    inline constexpr float kSubtitleBottomMarginFraction = 0.06F;

    // @p glyphWidth / @p glyphHeight are the font's unscaled cell size (8x8 for the built-in
    // bitmap font). @p scale is chosen from the screen height so the text stays the same apparent
    // size at any resolution, and is at least 1.
    [[nodiscard]] SubtitleLayout ComputeSubtitleLayout(const std::string& speaker,
                                                       const std::string& text,
                                                       float screenWidth,
                                                       float screenHeight,
                                                       float glyphWidth,
                                                       float glyphHeight);
}
