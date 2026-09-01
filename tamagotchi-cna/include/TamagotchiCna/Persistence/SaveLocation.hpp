#pragma once

#include <filesystem>

namespace TamagotchiCna::Persistence {

// Keeps the choice of save path outside the CNA adapter. An existing relative
// slot is deliberately preferred, so updating the game never relocates a pet.
class SaveLocation final {
public:
    [[nodiscard]] static std::filesystem::path legacySlot(
        const std::filesystem::path& workingDirectory);
    [[nodiscard]] static std::filesystem::path platformDataDirectory();
    [[nodiscard]] static std::filesystem::path resolveSlot(
        const std::filesystem::path& workingDirectory,
        const std::filesystem::path& platformDataDirectory);
};

} // namespace TamagotchiCna::Persistence
