#pragma once

#include "TamagotchiCna/Domain/ProgramDefinition.hpp"

namespace TamagotchiCna::Domain::Programs {

// English-language international Generation 1 (1997), never a P1/P2 hybrid.
[[nodiscard]] const ProgramDefinition& internationalP1() noexcept;

} // namespace TamagotchiCna::Domain::Programs
