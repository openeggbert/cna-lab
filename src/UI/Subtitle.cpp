#include "IronGang/UI/Subtitle.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    namespace
    {
        // Padding inside the panel, in scaled pixels.
        constexpr float kPaddingCells = 1.0F;
        // Gap between the speaker line and the body, as a fraction of a line.
        constexpr float kSpeakerGap = 0.35F;
        // The screen height the font's 1:1 size was designed to look right at; scale grows from
        // there so a 1080p screen does not get a smaller-looking subtitle than a 720p one.
        constexpr float kReferenceScreenHeight = 360.0F;
    }

    std::vector<std::string> WrapSubtitleText(const std::string& text, std::size_t maxCharactersPerLine)
    {
        std::vector<std::string> lines;
        if (maxCharactersPerLine == 0)
        {
            return lines;
        }

        std::string current;
        std::size_t index = 0;
        while (index <= text.size())
        {
            // Collect one word (a run of non-space characters).
            while (index < text.size() && text[index] == ' ')
            {
                ++index;
            }
            const std::size_t wordStart = index;
            while (index < text.size() && text[index] != ' ')
            {
                ++index;
            }
            if (wordStart == index)
            {
                break; // trailing spaces only
            }
            std::string word = text.substr(wordStart, index - wordStart);

            // A word longer than a whole line is hard-split: an unbreakable token must not be
            // allowed to run off the edge, which is the failure this whole function exists for.
            while (word.size() > maxCharactersPerLine)
            {
                if (!current.empty())
                {
                    lines.push_back(current);
                    current.clear();
                }
                lines.push_back(word.substr(0, maxCharactersPerLine));
                word = word.substr(maxCharactersPerLine);
            }

            if (current.empty())
            {
                current = word;
            }
            else if (current.size() + 1 + word.size() <= maxCharactersPerLine)
            {
                current += ' ';
                current += word;
            }
            else
            {
                lines.push_back(current);
                current = word;
            }
        }
        if (!current.empty())
        {
            lines.push_back(current);
        }
        return lines;
    }

    SubtitleLayout ComputeSubtitleLayout(const std::string& speaker,
                                         const std::string& text,
                                         float screenWidth,
                                         float screenHeight,
                                         float glyphWidth,
                                         float glyphHeight)
    {
        SubtitleLayout layout;
        if (screenWidth <= 0.0F || screenHeight <= 0.0F || glyphWidth <= 0.0F || glyphHeight <= 0.0F ||
            text.empty())
        {
            return layout;
        }

        layout.scale = std::max(1.0F, std::floor(screenHeight / kReferenceScreenHeight));
        const float cellWidth = glyphWidth * layout.scale;
        const float cellHeight = glyphHeight * layout.scale;

        const float usableWidth = screenWidth * kSubtitleWidthFraction;
        const auto maxCharacters =
            static_cast<std::size_t>(std::max(1.0F, std::floor(usableWidth / cellWidth)));

        layout.speaker = speaker;
        layout.lines = WrapSubtitleText(text, maxCharacters);
        if (layout.lines.empty())
        {
            return layout;
        }

        layout.lineHeight = cellHeight * 1.25F;
        const float speakerHeight = speaker.empty() ? 0.0F : layout.lineHeight * (1.0F + kSpeakerGap);
        const float padding = cellWidth * kPaddingCells;

        // The panel is only as wide as the longest line actually needs, so a short line does not
        // sit in the middle of a full-width bar.
        std::size_t longest = speaker.size();
        for (const std::string& line : layout.lines)
        {
            longest = std::max(longest, line.size());
        }
        layout.panelWidth = static_cast<float>(longest) * cellWidth + padding * 2.0F;
        layout.panelHeight = speakerHeight + static_cast<float>(layout.lines.size()) * layout.lineHeight +
                             padding * 2.0F;
        layout.panelX = std::floor((screenWidth - layout.panelWidth) * 0.5F);
        layout.panelY = std::floor(screenHeight * (1.0F - kSubtitleBottomMarginFraction) -
                                   layout.panelHeight);
        layout.textX = layout.panelX + padding;
        layout.textY = layout.panelY + padding + speakerHeight;
        return layout;
    }
}
