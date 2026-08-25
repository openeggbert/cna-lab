#include "JsonDataFileInternal.hpp"

#include "System/IO/File.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace IronGang
{
    using System::Text::Json::JsonDocument;
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    bool IsValidUtf8(const std::string& text)
    {
        std::size_t index = 0;
        while (index < text.size())
        {
            const auto byte = static_cast<unsigned char>(text[index]);
            std::size_t continuationBytes = 0;
            std::uint32_t codePoint = 0;
            if (byte < 0x80U)
            {
                ++index;
                continue;
            }
            if ((byte & 0xE0U) == 0xC0U)
            {
                continuationBytes = 1;
                codePoint = byte & 0x1FU;
            }
            else if ((byte & 0xF0U) == 0xE0U)
            {
                continuationBytes = 2;
                codePoint = byte & 0x0FU;
            }
            else if ((byte & 0xF8U) == 0xF0U)
            {
                continuationBytes = 3;
                codePoint = byte & 0x07U;
            }
            else
            {
                return false; // a continuation byte with no lead, or a 5/6-byte form
            }

            if (index + continuationBytes >= text.size())
            {
                return false; // truncated sequence
            }
            for (std::size_t offset = 1; offset <= continuationBytes; ++offset)
            {
                const auto continuation = static_cast<unsigned char>(text[index + offset]);
                if ((continuation & 0xC0U) != 0x80U)
                {
                    return false;
                }
                codePoint = (codePoint << 6) | (continuation & 0x3FU);
            }

            // Overlong encodings and surrogates are the two forms that let the same character be
            // written two ways -- which is how a validated identifier and a compared identifier
            // stop being the same string.
            static constexpr std::uint32_t kMinimum[] = {0U, 0x80U, 0x800U, 0x10000U};
            if (codePoint < kMinimum[continuationBytes] || codePoint > 0x10FFFFU ||
                (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            {
                return false;
            }
            index += continuationBytes + 1;
        }
        return true;
    }

    int MeasureJsonNestingDepth(const std::string& text)
    {
        int depth = 0;
        int deepest = 0;
        bool inString = false;
        bool escaped = false;
        for (const char character : text)
        {
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (character == '\\')
                {
                    escaped = true;
                }
                else if (character == '"')
                {
                    inString = false;
                }
                continue;
            }
            if (character == '"')
            {
                inString = true;
            }
            else if (character == '{' || character == '[')
            {
                ++depth;
                deepest = std::max(deepest, depth);
            }
            else if (character == '}' || character == ']')
            {
                --depth;
                if (depth < 0)
                {
                    return -1; // more closers than openers
                }
            }
        }
        if (inString || depth != 0)
        {
            return -1;
        }
        return deepest;
    }

    bool ReadBoundedJsonText(const std::string& path, std::string& out, std::string& errorMessage)
    {
        if (!System::IO::File::Exists(path))
        {
            errorMessage = "File not found: " + path;
            return false;
        }

        // Ask the filesystem how big it is before reading it: refusing a 900 MB file after
        // allocating 900 MB would be a strange kind of protection.
        std::error_code sizeError;
        const std::uintmax_t size = std::filesystem::file_size(std::filesystem::path(path), sizeError);
        if (sizeError)
        {
            errorMessage = "Could not measure " + path + ": " + sizeError.message();
            return false;
        }
        if (size > kMaxJsonDataFileBytes)
        {
            errorMessage = "File is larger than the " + std::to_string(kMaxJsonDataFileBytes) +
                           "-byte limit for data files (" + std::to_string(size) + " bytes): " + path;
            return false;
        }

        try
        {
            std::string text = System::IO::File::ReadAllText(path);
            if (!IsValidUtf8(text))
            {
                errorMessage = "File is not valid UTF-8: " + path;
                return false;
            }
            const int depth = MeasureJsonNestingDepth(text);
            if (depth < 0)
            {
                errorMessage = "File has unbalanced brackets or an unterminated string: " + path;
                return false;
            }
            if (depth > kMaxJsonDataFileDepth)
            {
                errorMessage = "File nests " + std::to_string(depth) + " levels deep, past the limit of " +
                               std::to_string(kMaxJsonDataFileDepth) + ": " + path;
                return false;
            }

            out = std::move(text);
            return true;
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }
    }

    bool LoadJsonDataFile(const std::string& path, JsonDataFile& out, std::string& errorMessage)
    {
        std::string text;
        if (!ReadBoundedJsonText(path, text, errorMessage))
        {
            return false;
        }

        try
        {
            JsonDataFile loaded;
            loaded.document = JsonDocument::Parse(text);
            loaded.root = loaded.document->getRootElementProperty();
            if (loaded.root.getValueKindProperty() != JsonValueKind::Object)
            {
                errorMessage = "File's root must be a JSON object: " + path;
                return false;
            }
            out = std::move(loaded);
            return true;
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string(exception.what()) + " (" + path + ")";
            return false;
        }
    }
}
