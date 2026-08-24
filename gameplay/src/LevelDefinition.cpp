#include "CopperBoots/LevelDefinition.hpp"

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace CopperBoots
{
    namespace
    {
        struct SourceLine
        {
            std::string_view Text;
            std::size_t Number;
        };

        [[noreturn]] void Fail(const std::string_view sourceName,
                               const std::size_t line,
                               const std::string_view message)
        {
            throw std::runtime_error(std::string(sourceName) + ':' +
                                     std::to_string(line) + ": " +
                                     std::string(message));
        }

        [[nodiscard]] std::vector<SourceLine> SplitLines(std::string_view text)
        {
            std::vector<SourceLine> lines;
            std::size_t number = 1;
            while (!text.empty()) {
                const std::size_t end = text.find('\n');
                std::string_view line = text.substr(0, end);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                lines.push_back({line, number++});
                if (end == std::string_view::npos)
                    break;
                text.remove_prefix(end + 1);
            }
            return lines;
        }

        [[nodiscard]] std::string_view ValueAfter(
            const SourceLine& line,
            const std::string_view prefix,
            const std::string_view sourceName)
        {
            if (!line.Text.starts_with(prefix))
                Fail(sourceName, line.Number,
                     "expected directive '" + std::string(prefix) + "'");
            const std::string_view value = line.Text.substr(prefix.size());
            if (value.empty())
                Fail(sourceName, line.Number, "directive value cannot be empty");
            return value;
        }

        template <typename Value>
        [[nodiscard]] Value ParseNumber(std::string_view& text,
                                        const SourceLine& line,
                                        const std::string_view sourceName)
        {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            Value value{};
            const auto result = std::from_chars(text.data(),
                                                text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr == text.data())
                Fail(sourceName, line.Number, "expected a number");
            text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
            return value;
        }

        void RequireEnd(std::string_view text,
                        const SourceLine& line,
                        const std::string_view sourceName)
        {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            if (!text.empty())
                Fail(sourceName, line.Number, "unexpected trailing value");
        }

        [[nodiscard]] Tile DecodeTile(const char glyph,
                                      const std::string_view sourceName,
                                      const std::size_t line)
        {
            switch (glyph) {
            case '.': return Tiles::Empty;
            case '#': return Tiles::Ruin;
            case 'B': return Tiles::Breakable;
            case '!': return Tiles::Hazard;
            case 'E': return Tiles::Exit;
            case 'd': return Tiles::Decoration;
            default:
                Fail(sourceName, line,
                     "unknown map glyph '" + std::string(1, glyph) + "'");
            }
        }
    }

    LevelDefinition LevelDefinition::Parse(const std::string_view text,
                                           const std::string_view sourceName)
    {
        const std::vector<SourceLine> lines = SplitLines(text);
        constexpr std::size_t HeaderLineCount = 23;
        if (lines.size() < HeaderLineCount)
            Fail(sourceName, lines.empty() ? 1 : lines.back().Number,
                 "incomplete level header");

        if (lines[0].Text != "copper-boots-level 1")
            Fail(sourceName, lines[0].Number,
                 "expected 'copper-boots-level 1'");
        const std::string name(ValueAfter(lines[1], "name ", sourceName));

        std::string_view sizeText = ValueAfter(lines[2], "size ", sourceName);
        const int width = ParseNumber<int>(sizeText, lines[2], sourceName);
        const int height = ParseNumber<int>(sizeText, lines[2], sourceName);
        RequireEnd(sizeText, lines[2], sourceName);
        if (width <= 0 || height <= 0 || width > 2'048 || height > 256)
            Fail(sourceName, lines[2].Number, "level dimensions are out of range");

        std::string_view spawnText = ValueAfter(lines[3], "spawn ", sourceName);
        const int spawnX = ParseNumber<int>(spawnText, lines[3], sourceName);
        const int spawnY = ParseNumber<int>(spawnText, lines[3], sourceName);
        RequireEnd(spawnText, lines[3], sourceName);
        if (spawnX < 0 || spawnX >= width || spawnY <= 0 || spawnY > height)
            Fail(sourceName, lines[3].Number, "spawn coordinate is out of range");

        std::string_view checkpointText = ValueAfter(
            lines[4], "checkpoint ", sourceName);
        const int checkpointX = ParseNumber<int>(
            checkpointText, lines[4], sourceName);
        const int checkpointY = ParseNumber<int>(
            checkpointText, lines[4], sourceName);
        RequireEnd(checkpointText, lines[4], sourceName);
        if (checkpointX < 0 || checkpointX >= width || checkpointY <= 0 ||
            checkpointY > height)
            Fail(sourceName, lines[4].Number,
                 "checkpoint coordinate is out of range");

        std::string_view parallaxText = ValueAfter(
            lines[5], "parallax ", sourceName);
        std::array<float, 3> parallax{};
        for (float& factor : parallax)
            factor = ParseNumber<float>(parallaxText, lines[5], sourceName);
        RequireEnd(parallaxText, lines[5], sourceName);
        if (!(parallax[0] >= 0.0F && parallax[0] <= parallax[1] &&
              parallax[1] <= parallax[2] && parallax[2] <= 1.5F))
            Fail(sourceName, lines[5].Number,
                 "parallax factors must be ascending values from 0 to 1.5");

        constexpr std::array<std::string_view, 17> fixedLines{
            "legend", ". empty", "# solid", "B breakable", "! hazard",
            "E exit", "d decoration", "G cog", "? cog-block",
            "o empty-block", "P plated-block", "A plating",
            "R capacitor-block", "K capacitor",
            "C crawler", "c crawler-fall", "map"};
        for (std::size_t i = 0; i < fixedLines.size(); ++i) {
            const std::size_t lineIndex = 6 + i;
            if (lines[lineIndex].Text != fixedLines[i])
                Fail(sourceName, lines[lineIndex].Number,
                     "expected '" + std::string(fixedLines[i]) + "'");
        }

        const std::size_t mapStart = HeaderLineCount;
        if (lines.size() < mapStart + static_cast<std::size_t>(height))
            Fail(sourceName, lines.back().Number, "level map has too few rows");

        TileMap map(width, height);
        std::vector<TileCoordinate> cogs;
        std::vector<CrawlerDefinition> crawlers;
        std::vector<TileCoordinate> platingPickups;
        std::vector<TileCoordinate> capacitorPickups;
        std::vector<InteractiveBlockDefinition> interactiveBlocks;
        for (int y = 0; y < height; ++y) {
            const SourceLine& line = lines[mapStart + static_cast<std::size_t>(y)];
            if (line.Text.size() != static_cast<std::size_t>(width))
                Fail(sourceName, line.Number,
                     "map row width does not match size directive");
            for (int x = 0; x < width; ++x) {
                const char glyph = line.Text[static_cast<std::size_t>(x)];
                if (glyph == 'G') {
                    cogs.push_back({x, y});
                    map.Set(x, y, Tiles::Empty);
                }
                else if (glyph == 'C' || glyph == 'c') {
                    crawlers.push_back({{x, y}, glyph == 'c'});
                    map.Set(x, y, Tiles::Empty);
                }
                else if (glyph == 'A') {
                    platingPickups.push_back({x, y});
                    map.Set(x, y, Tiles::Empty);
                }
                else if (glyph == 'K') {
                    capacitorPickups.push_back({x, y});
                    map.Set(x, y, Tiles::Empty);
                }
                else if (glyph == '?' || glyph == 'o' || glyph == 'P' ||
                         glyph == 'R') {
                    BlockContent content = BlockContent::None;
                    if (glyph == '?')
                        content = BlockContent::Cog;
                    else if (glyph == 'P')
                        content = BlockContent::Plating;
                    else if (glyph == 'R')
                        content = BlockContent::Capacitor;
                    interactiveBlocks.push_back({{x, y}, content});
                    map.Set(x, y, Tiles::Interactive);
                }
                else {
                    map.Set(x, y, DecodeTile(glyph, sourceName, line.Number));
                }
            }
        }

        for (std::size_t i = mapStart + static_cast<std::size_t>(height);
             i < lines.size(); ++i) {
            if (!lines[i].Text.empty())
                Fail(sourceName, lines[i].Number, "unexpected content after map");
        }

        return {name, std::move(map), spawnX, spawnY,
                checkpointX, checkpointY, parallax, std::move(cogs),
                std::move(crawlers), std::move(platingPickups),
                std::move(capacitorPickups),
                std::move(interactiveBlocks)};
    }
}
