#pragma once

// Internal to iron_gang_core: this header names sharp-runtime's JSON types, which are a **private**
// dependency of the library. Keeping it out of include/ is what stops every consumer of
// IronGang::Core from needing Text.Json on its include path. The public, JSON-free half of this
// facility is include/IronGang/Core/JsonDataFile.hpp.

#include "IronGang/Core/JsonDataFile.hpp"

#include "System/Text/Json/JsonDocument.hpp"

#include <memory>
#include <string>

namespace IronGang
{
    // A parsed JSON document plus its root. The document owns the parsed tree, so it has to be
    // kept alive for as long as the root (or anything read out of it) is used -- which is exactly
    // why this is one struct rather than two return values.
    struct JsonDataFile
    {
        std::shared_ptr<System::Text::Json::JsonDocument> document;
        System::Text::Json::JsonElement root;
    };

    // ReadBoundedJsonText followed by a parse, requiring a JSON object at the root -- which every
    // data file in this game has. @p out is only written on success.
    [[nodiscard]] bool LoadJsonDataFile(const std::string& path, JsonDataFile& out, std::string& errorMessage);
}
