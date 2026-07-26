#pragma once

#include "CnaTamagotchi/Domain/PetState.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace CnaTamagotchi::Persistence {

struct SaveData final {
    static constexpr int CurrentFormatVersion = 1;

    int formatVersion{CurrentFormatVersion};
    std::int64_t lastSavedUnixSeconds{0};
    std::uint64_t seed{0};
    Domain::PetState pet{};
};

struct SaveResult final {
    bool success{false};
    std::string error;
};

struct LoadResult final {
    std::optional<SaveData> data;
    std::string error;

    [[nodiscard]] bool success() const noexcept { return data.has_value(); }
};

// Versioned JSON save storage. The repository has no CNA dependency and
// never changes the in-memory pet if parsing or validation fails.
class SaveRepository final {
public:
    [[nodiscard]] SaveResult save(const std::filesystem::path& path,
                                  const SaveData& data) const;
    [[nodiscard]] LoadResult load(const std::filesystem::path& path) const;
    [[nodiscard]] SaveResult restoreBackup(const std::filesystem::path& path) const;
    [[nodiscard]] SaveResult archiveCorruptSave(const std::filesystem::path& path) const;
};

} // namespace CnaTamagotchi::Persistence
