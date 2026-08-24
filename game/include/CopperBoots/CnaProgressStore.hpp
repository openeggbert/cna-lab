#pragma once

#include "CopperBoots/ProgressSave.hpp"

namespace CopperBoots
{
    class CnaProgressStore
    {
    public:
        [[nodiscard]] static ProgressLoadResult Load();
        static ProgressLoadResult Save(const ProgressData& progress);
    };
}
