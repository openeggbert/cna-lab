#pragma once

#include <string_view>

#include "People/Objects/ObjectModel.hpp"

namespace People::Content
{
    /** @brief Original native content proving the first object schema and renderer. */
    class DemoFurniture final
    {
    public:
        static constexpr std::string_view BedId = "people.bed.cedar_nest";
        static constexpr std::string_view ChairId = "people.chair.sunny_dining";
        static constexpr std::string_view TableId = "people.table.roundleaf";
        static constexpr std::string_view RefrigeratorId = "people.fridge.mintbox";
        static constexpr std::string_view ToiletId = "people.toilet.cloudline";

        /** @brief Registers five definitions and places one instance of each. */
        static void Populate(Objects::ObjectWorld& world);
    };
}
