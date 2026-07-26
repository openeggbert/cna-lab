#pragma once

#include "CnaTamagotchi/Domain/ProgramSimulation.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace CnaTamagotchi::Persistence {

struct SaveData final {
    static constexpr int CurrentFormatVersion = 2;

    int formatVersion{CurrentFormatVersion};
    std::string programId{"international-p1-1997"};
    std::int64_t lastSavedUnixSeconds{0};
    std::uint64_t seed{0};
    Domain::ProgramPetState pet{};
};

struct SaveResult final {
    bool success{false};
    std::string error;
};

struct LoadResult final {
    std::optional<SaveData> data;
    bool legacyPrototype{false};
    std::string error;

    [[nodiscard]] bool success() const noexcept { return data.has_value(); }
    [[nodiscard]] bool isLegacyPrototype() const noexcept { return legacyPrototype; }
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
    [[nodiscard]] SaveResult archiveResetSave(const std::filesystem::path& path) const;
    [[nodiscard]] SaveResult archiveLegacySave(const std::filesystem::path& path) const;
};

} // namespace CnaTamagotchi::Persistence
