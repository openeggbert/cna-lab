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
                case 'M':
                case 'Q':
                case 'C':
                case 'H':
                case 'A':
                case 'T':
                case 'E':
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
