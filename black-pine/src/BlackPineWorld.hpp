#pragma once

#include "explore2d/Renderer.hpp"
#include "explore2d/World.hpp"

namespace black_pine {

[[nodiscard]] explore2d::WorldDefinition buildWorld();
[[nodiscard]] explore2d::RendererTheme buildTheme();

} // namespace black_pine
