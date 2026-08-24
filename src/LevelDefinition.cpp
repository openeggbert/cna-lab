#include "LevelDefinition.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace WolfCna
{
    namespace
    {
        [[nodiscard]] std::runtime_error LevelError(
            std::string_view sourceName,
            std::string_view message)
        {
            return std::runtime_error(std::string(sourceName) + ": " + std::string(message));
        }
    }

    LevelDefinition LevelDefinition::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        if (!input)
            throw LevelError(path.string(), "could not open level file");

        std::ostringstream text;
        text << input.rdbuf();
        if (input.bad())
            throw LevelError(path.string(), "could not read level file");

        return Parse(text.str(), path.string());
    }

    LevelDefinition LevelDefinition::Parse(std::string_view text, std::string_view sourceName)
    {
        LevelDefinition level;
        std::istringstream input{std::string(text)};
        std::string row;
        std::size_t lineNumber = 0;
        std::size_t width = 0;

        while (std::getline(input, row))
        {
            ++lineNumber;
            if (!row.empty() && row.back() == '\r')
                row.pop_back();

            if (row.empty())
            {
                throw LevelError(
                    sourceName,
                    "line " + std::to_string(lineNumber) + " must not be empty");
            }

            if (level.rows_.empty())
                width = row.size();
            else if (row.size() != width)
            {
                throw LevelError(
                    sourceName,
                    "line " + std::to_string(lineNumber) + " has a different width");
            }

            for (std::size_t x = 0; x < row.size(); ++x)
            {
                switch (row[x])
                {
                case '#':
                case '.':
                case 'D':
                case 'G':
                case 'K':
                case 'g':
                case 'k':
                case 'M':
                case 'O':
                case 'Q':
                case 'q':
                case 'S':
                case 'C':
                case 'c':
                case 'H':
                case 'h':
                case 'A':
                case 'a':
                case 'T':
                case 'J':
                case 'N':
                case 'p':
                case 'r':
                case 'E':
                case 'X':
                case 'R':
                case 'B':
                case 'I':
                case 'L':
                case 'W':
                case 'V':
                case 'F':
                case 'U':
                case 'f':
                case 'u':
                case 'Z':
                case 'Y':
                case '^':
                case '>':
                case 'v':
                case '<':
                    break;
                case 'P':
                    if (level.playerStartX_ >= 0)
                    {
                        throw LevelError(
                            sourceName,
                            "contains more than one player spawn");
                    }
                    level.playerStartX_ = static_cast<int>(x);
                    level.playerStartZ_ = static_cast<int>(level.rows_.size());
                    break;
                default:
                    throw LevelError(
                        sourceName,
                        "line " + std::to_string(lineNumber) +
                            " contains unknown symbol '" + row[x] + "'");
                }
            }

            level.rows_.push_back(std::move(row));
        }

        if (level.rows_.empty())
            throw LevelError(sourceName, "level contains no rows");
        if (level.playerStartX_ < 0)
            throw LevelError(sourceName, "level contains no player spawn");

        const auto isWall = [&level](int x, int z)
        {
            return z >= 0 && z < static_cast<int>(level.rows_.size()) &&
                x >= 0 && x < static_cast<int>(level.rows_[static_cast<std::size_t>(z)].size()) &&
                level.rows_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] == '#';
        };
        for (int z = 0; z < static_cast<int>(level.rows_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(level.rows_[static_cast<std::size_t>(z)].size()); ++x)
            {
                const char symbol = level.rows_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                if ((symbol == 'R' || symbol == 'B') &&
                    !isWall(x, z - 1) && !isWall(x, z + 1) &&
                    !isWall(x - 1, z) && !isWall(x + 1, z))
                {
                    throw LevelError(
                        sourceName,
                        "line " + std::to_string(z + 1) +
                            " has a wall decoration without an adjacent wall");
                }

                if (symbol != '^' && symbol != '>' && symbol != 'v' && symbol != '<')
                    continue;
                const int directionX = symbol == '>' ? 1 : symbol == '<' ? -1 : 0;
                const int directionZ = symbol == 'v' ? 1 : symbol == '^' ? -1 : 0;
                const int destinationX = x + directionX;
                const int destinationZ = z + directionZ;
                const bool destinationInBounds = destinationZ >= 0 &&
                    destinationZ < static_cast<int>(level.rows_.size()) &&
                    destinationX >= 0 && destinationX <
                        static_cast<int>(level.rows_[static_cast<std::size_t>(destinationZ)].size());
                const char destination = destinationInBounds
                    ? level.rows_[static_cast<std::size_t>(destinationZ)]
                        [static_cast<std::size_t>(destinationX)]
                    : '#';
                if (!destinationInBounds || destination == '#' || destination == 'Y' ||
                    destination == 'D' || destination == 'Q' || destination == 'S' ||
                    destination == 'q' ||
                    destination == 'E' || destination == 'X')
                {
                    throw LevelError(
                        sourceName,
                        "line " + std::to_string(z + 1) +
                            " has a patrol marker pointing into a blocked cell");
                }
            }
        }

        return level;
    }

    const std::vector<std::string>& LevelDefinition::Rows() const
    {
        return rows_;
    }

    int LevelDefinition::PlayerStartX() const
    {
        return playerStartX_;
    }

    int LevelDefinition::PlayerStartZ() const
    {
        return playerStartZ_;
    }
}
