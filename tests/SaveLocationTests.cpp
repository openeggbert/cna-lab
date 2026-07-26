#include "CnaTamagotchi/Persistence/SaveLocation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using CnaTamagotchi::Persistence::SaveLocation;

namespace {

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

std::filesystem::path testDirectory()
{
    return std::filesystem::temp_directory_path() / "cna-tamagotchi-save-location-tests";
}

void testNewSlotUsesPlatformDataDirectory()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path workingDirectory = directory / "working";
    const std::filesystem::path dataDirectory = directory / "data";
    std::error_code error;
    std::filesystem::remove_all(directory, error);

    const std::filesystem::path resolved =
        SaveLocation::resolveSlot(workingDirectory, dataDirectory);
    expect(resolved == dataDirectory / "cna-tamagotchi" / "saves" / "slot-1.json",
        "a new slot must use the supplied per-user data directory");

    std::filesystem::remove_all(directory, error);
}

void testLegacySaveAndBackupTakePrecedence()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path workingDirectory = directory / "working";
    const std::filesystem::path dataDirectory = directory / "data";
    const std::filesystem::path legacy = SaveLocation::legacySlot(workingDirectory);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(legacy.parent_path(), error);

    {
        std::ofstream stream(legacy);
        stream << "old slot";
    }
    expect(SaveLocation::resolveSlot(workingDirectory, dataDirectory) == legacy,
        "an existing relative save must not be moved to the per-user directory");

    std::filesystem::remove(legacy, error);
    {
        std::ofstream stream(legacy.string() + ".bak");
        stream << "old backup";
    }
    expect(SaveLocation::resolveSlot(workingDirectory, dataDirectory) == legacy,
        "an existing relative backup must keep its recovery flow in place");

    std::filesystem::remove_all(directory, error);
}

} // namespace

int main()
{
    testNewSlotUsesPlatformDataDirectory();
    testLegacySaveAndBackupTakePrecedence();

    if (failures == 0) {
        std::cout << "SaveLocationTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
