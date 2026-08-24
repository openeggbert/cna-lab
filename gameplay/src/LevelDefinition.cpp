#include "CopperBoots/LevelDefinition.hpp"

#include <charconv>
#include <cctype>
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

        [[nodiscard]] std::string ParseIdentifier(
            std::string_view& text, const SourceLine& line,
            const std::string_view sourceName)
        {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            const std::size_t end = text.find(' ');
            const std::string_view value = text.substr(0, end);
            if (value.empty())
                Fail(sourceName, line.Number, "expected an identifier");
            for (const unsigned char character : value) {
                if (!std::isalnum(character) && character != '-' &&
                    character != '_') {
                    Fail(sourceName, line.Number,
                         "identifier contains an invalid character");
                }
            }
            text.remove_prefix(end == std::string_view::npos
                ? text.size()
                : end);
            return std::string(value);
        }

        [[nodiscard]] Tile DecodeTile(const char glyph,
                                      const std::string_view sourceName,
                                      const std::size_t line)
        {
            switch (glyph) {
            case '.': return Tiles::Empty;
            case '#': return Tiles::Ruin;
            case '-': return Tiles::OneWay;
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
        constexpr std::size_t MinimumHeaderLineCount = 25;
        if (lines.size() < MinimumHeaderLineCount)
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

        std::string initialArea = "main";
        bool initialAreaSeen = false;
        std::vector<RouteEndpointDefinition> routeEndpoints;
        std::vector<RouteDefinition> routes;
        std::vector<std::size_t> endpointLines;
        std::vector<std::size_t> routeLines;
        std::size_t legendIndex = 6;
        while (legendIndex < lines.size() &&
               lines[legendIndex].Text != "legend") {
            const SourceLine& line = lines[legendIndex];
            if (line.Text.starts_with("initial-area ")) {
                if (initialAreaSeen)
                    Fail(sourceName, line.Number,
                         "initial-area may only be declared once");
                std::string_view value = ValueAfter(
                    line, "initial-area ", sourceName);
                initialArea = ParseIdentifier(value, line, sourceName);
                RequireEnd(value, line, sourceName);
                initialAreaSeen = true;
            }
            else if (line.Text.starts_with("endpoint ")) {
                std::string_view value = ValueAfter(
                    line, "endpoint ", sourceName);
                RouteEndpointDefinition endpoint;
                endpoint.Name = ParseIdentifier(value, line, sourceName);
                endpoint.Area = ParseIdentifier(value, line, sourceName);
                endpoint.Position.X = ParseNumber<int>(value, line, sourceName);
                endpoint.Position.Y = ParseNumber<int>(value, line, sourceName);
                RequireEnd(value, line, sourceName);
                if (endpoint.Position.X < 0 || endpoint.Position.X >= width ||
                    endpoint.Position.Y <= 0 || endpoint.Position.Y >= height) {
                    Fail(sourceName, line.Number,
                         "route endpoint coordinate is out of range");
                }
                for (const RouteEndpointDefinition& existing : routeEndpoints) {
                    if (existing.Name == endpoint.Name)
                        Fail(sourceName, line.Number,
                             "route endpoint name is duplicated");
                }
                routeEndpoints.push_back(std::move(endpoint));
                endpointLines.push_back(line.Number);
            }
            else if (line.Text.starts_with("route ")) {
                std::string_view value = ValueAfter(line, "route ", sourceName);
                RouteDefinition route;
                route.Source = ParseIdentifier(value, line, sourceName);
                route.Destination = ParseIdentifier(value, line, sourceName);
                RequireEnd(value, line, sourceName);
                if (route.Source == route.Destination)
                    Fail(sourceName, line.Number,
                         "self-linked routes are not allowed");
                for (const RouteDefinition& existing : routes) {
                    if (existing.Source == route.Source)
                        Fail(sourceName, line.Number,
                             "route source is duplicated");
                }
                routes.push_back(std::move(route));
                routeLines.push_back(line.Number);
            }
            else {
                Fail(sourceName, line.Number,
                     "unknown level metadata directive");
            }
            ++legendIndex;
        }
        if (legendIndex >= lines.size())
            Fail(sourceName, lines.back().Number, "missing level legend");

        constexpr std::array<std::string_view, 19> fixedLines{
            "legend", ". empty", "# solid", "- one-way", "B breakable", "! hazard",
            "E exit", "d decoration", "G cog", "? cog-block",
            "o empty-block", "P plated-block", "A plating",
            "R capacitor-block", "K capacitor",
            "H checkpoint", "C crawler", "c crawler-fall", "map"};
        for (std::size_t i = 0; i < fixedLines.size(); ++i) {
            const std::size_t lineIndex = legendIndex + i;
            if (lineIndex >= lines.size())
                Fail(sourceName, lines.back().Number,
                     "incomplete level legend");
            if (lines[lineIndex].Text != fixedLines[i])
                Fail(sourceName, lines[lineIndex].Number,
                     "expected '" + std::string(fixedLines[i]) + "'");
        }

        const std::size_t mapStart = legendIndex + fixedLines.size();
        if (lines.size() < mapStart + static_cast<std::size_t>(height))
            Fail(sourceName, lines.back().Number, "level map has too few rows");

        TileMap map(width, height);
        std::vector<TileCoordinate> cogs;
        std::vector<CrawlerDefinition> crawlers;
        std::vector<TileCoordinate> platingPickups;
        std::vector<TileCoordinate> capacitorPickups;
        std::vector<TileCoordinate> checkpoints;
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
                else if (glyph == 'H') {
                    checkpoints.push_back({x, y});
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

        const auto findEndpoint = [&](const std::string& endpointName) {
            for (const RouteEndpointDefinition& endpoint : routeEndpoints) {
                if (endpoint.Name == endpointName)
                    return &endpoint;
            }
            return static_cast<const RouteEndpointDefinition*>(nullptr);
        };
        for (std::size_t index = 0; index < routes.size(); ++index) {
            const RouteDefinition& route = routes[index];
            if (findEndpoint(route.Source) == nullptr ||
                findEndpoint(route.Destination) == nullptr) {
                Fail(sourceName, routeLines[index],
                     "route references an unknown endpoint");
            }
        }
        for (std::size_t index = 0; index < routeEndpoints.size(); ++index) {
            const RouteEndpointDefinition& endpoint = routeEndpoints[index];
            const int x = endpoint.Position.X;
            const int footY = endpoint.Position.Y;
            if (!map.IsSolid(x, footY) || map.IsSolid(x, footY - 1) ||
                map.IsSolid(x, footY - 2)) {
                Fail(sourceName, endpointLines[index],
                     "route endpoint must have clear standing space above solid ground");
            }
        }

        return {name, std::move(map), spawnX, spawnY,
                checkpointX, checkpointY, std::move(initialArea), parallax,
                std::move(cogs),
                std::move(crawlers), std::move(platingPickups),
                std::move(capacitorPickups), std::move(checkpoints),
                std::move(interactiveBlocks), std::move(routeEndpoints),
                std::move(routes)};
    }
}
