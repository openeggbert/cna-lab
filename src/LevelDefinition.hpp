#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace WolfCna
{
    class LevelDefinition final
    {
    public:
        [[nodiscard]] static LevelDefinition LoadFromFile(const std::filesystem::path& path);
        [[nodiscard]] static LevelDefinition Parse(
            std::string_view text,
            std::string_view sourceName);

        [[nodiscard]] const std::vector<std::string>& Rows() const;
        [[nodiscard]] int PlayerStartX() const;
        [[nodiscard]] int PlayerStartZ() const;

    private:
        std::vector<std::string> rows_;
        int playerStartX_ = -1;
        int playerStartZ_ = -1;
    };
}
