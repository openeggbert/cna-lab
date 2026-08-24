#pragma once

#include "CopperBoots/GameSettings.hpp"

namespace CopperBoots
{
    class CnaSettingsStore
    {
    public:
        [[nodiscard]] static SettingsLoadResult Load();
        static void Save(const GameSettings& settings);
    };
}
