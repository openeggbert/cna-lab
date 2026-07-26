#include "CnaTamagotchi/Persistence/SaveRepository.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace CnaTamagotchi;

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
    return std::filesystem::temp_directory_path() / "cna-tamagotchi-save-repository-tests";
}

void testRoundTripAndBackup()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "slot-1.json";
    std::error_code error;
    std::filesystem::remove_all(directory, error);

    Persistence::SaveData first{};
    first.lastSavedUnixSeconds = 1'725'000'000;
    first.seed = 42;
    first.pet.species = Domain::PetSpecies::Cometling;
    first.pet.lifeStage = Domain::LifeStage::Teen;
    first.pet.ageMinutes = 1'440;
    first.pet.needs.hunger = 37;
    first.pet.asleep = true;

    Persistence::SaveRepository repository;
    expect(repository.save(path, first).success, "first save must succeed");

    Persistence::SaveData second = first;
    second.pet.needs.hunger = 81;
    expect(repository.save(path, second).success, "second save must succeed");
    expect(std::filesystem::exists(path.string() + ".bak"),
        "saving an existing slot must create a backup");

    const Persistence::LoadResult loaded = repository.load(path);
    expect(loaded.success(), "saved slot must load");
    if (loaded.data) {
        expect(loaded.data->seed == second.seed, "seed must survive a round trip");
        expect(loaded.data->pet.species == Domain::PetSpecies::Cometling,
            "species must survive a round trip");
        expect(loaded.data->pet.needs.hunger == 81,
            "latest need value must survive a round trip");
        expect(loaded.data->pet.asleep, "boolean state must survive a round trip");
    }

    std::filesystem::remove_all(directory, error);
}

void testInvalidDataIsRejected()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "invalid.json";
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    {
        std::ofstream stream(path);
        stream << "{\"formatVersion\":99}";
    }

    Persistence::SaveRepository repository;
    const Persistence::LoadResult loaded = repository.load(path);
    expect(!loaded.success(), "unsupported or incomplete JSON must be rejected");

    std::filesystem::remove_all(directory, error);
}

} // namespace

int main()
{
    testRoundTripAndBackup();
    testInvalidDataIsRejected();

    if (failures == 0) {
        std::cout << "SaveRepositoryTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
