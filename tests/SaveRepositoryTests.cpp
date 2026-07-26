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

Persistence::SaveData p1Save()
{
    Persistence::SaveData data{};
    data.lastSavedUnixSeconds = 1'725'000'000;
    data.seed = 42;
    data.pet.characterId = "marutchi";
    data.pet.stage = Domain::ProgramStage::Child;
    data.pet.minutesSinceClockSet = 70;
    data.pet.minutesSinceHatch = 65;
    data.pet.age = 1;
    data.pet.weight = 12;
    data.pet.hungerHearts = 3;
    data.pet.happinessHearts = 2;
    data.pet.disciplineBars = 1;
    data.pet.medicineDosesRemaining = 0;
    data.pet.clockMinutesOfDay = 20 * 60;
    data.pet.wasteCount = 1;
    data.pet.careMistakes = 2;
    data.pet.disciplineMistakes = 1;
    data.pet.teenLineage = Domain::ProgramTeenLineage::TypeA;
    data.pet.teenStartedWithNoDiscipline = true;
    data.pet.attentionDeadlineMinutes = 85;
    data.pet.nextAttentionEligibleMinutes = 100;
    data.pet.asleep = true;
    data.pet.lightOff = true;
    data.pet.attentionReason = Domain::ProgramAttentionReason::SleepLight;
    return data;
}

void testP1RoundTripAndBackup()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "slot-1.json";
    std::error_code error;
    std::filesystem::remove_all(directory, error);

    Persistence::SaveData first = p1Save();
    Persistence::SaveRepository repository;
    expect(repository.save(path, first).success, "first P1 save must succeed");

    Persistence::SaveData second = first;
    second.pet.hungerHearts = 1;
    expect(repository.save(path, second).success, "second P1 save must succeed");
    expect(std::filesystem::exists(path.string() + ".bak"),
        "saving an existing P1 slot must create a backup");

    const Persistence::LoadResult loaded = repository.load(path);
    expect(loaded.success(), "saved P1 slot must load");
    expect(!loaded.isLegacyPrototype(), "current P1 save must not be classified as legacy");
    if (loaded.data) {
        expect(loaded.data->formatVersion == Persistence::SaveData::CurrentFormatVersion,
            "the current P1 format version must survive a round trip");
        expect(loaded.data->programId == "international-p1-1997",
            "P1 programme identifier must survive a round trip");
        expect(loaded.data->pet.characterId == "marutchi"
                && loaded.data->pet.stage == Domain::ProgramStage::Child,
            "P1 character identity and stage must survive a round trip");
        expect(loaded.data->pet.hungerHearts == 1 && loaded.data->pet.asleep,
            "P1 visible heart and boolean state must survive a round trip");
        expect(loaded.data->pet.minutesSinceClockSet == 70
                && loaded.data->pet.minutesSinceHatch == 65,
            "P1 lifecycle time must survive a round trip");
        expect(loaded.data->pet.wasteCount == 1 && loaded.data->pet.careMistakes == 2
                && loaded.data->pet.disciplineMistakes == 1
                && loaded.data->pet.teenLineage == Domain::ProgramTeenLineage::TypeA
                && loaded.data->pet.teenStartedWithNoDiscipline
                && loaded.data->pet.lightOff
                && loaded.data->pet.attentionReason == Domain::ProgramAttentionReason::SleepLight,
            "P1 care-event, evolution-history, and light state must survive a round trip");
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
    expect(!loaded.success() && !loaded.isLegacyPrototype(),
        "unsupported or incomplete JSON must be rejected as invalid, not legacy");

    std::filesystem::remove_all(directory, error);
}

void testVersionTwoP1SaveKeepsAConservativeEvolutionDefault()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "version-two.json";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);

    {
        std::ofstream stream(path);
        stream << R"({
  "formatVersion": 2,
  "programId": "international-p1-1997",
  "lastSavedUnixSeconds": 1725000000,
  "seed": 42,
  "pet": {
    "characterId": "marutchi",
    "stage": 2,
    "minutesSinceClockSet": 70,
    "minutesSinceHatch": 65,
    "age": 1,
    "weight": 10,
    "hungerHearts": 3,
    "happinessHearts": 2,
    "disciplineBars": 1,
    "medicineDosesRemaining": 0,
    "clockMinutesOfDay": 1200,
    "wasteCount": 0,
    "careMistakes": 2,
    "attentionDeadlineMinutes": -1,
    "nextAttentionEligibleMinutes": 0,
    "asleep": 0,
    "lightOff": 0,
    "sick": 0,
    "attentionReason": 0
  }
})";
    }

    Persistence::SaveRepository repository;
    const Persistence::LoadResult loaded = repository.load(path);
    expect(loaded.success(), "version-two P1 saves must remain readable after evolution-history storage");
    if (loaded.data) {
        expect(loaded.data->formatVersion == 2
                && loaded.data->pet.disciplineMistakes == 0
                && loaded.data->pet.teenLineage == Domain::ProgramTeenLineage::None,
            "a version-two P1 save must receive conservative unknown evolution-history defaults");
    }

    std::filesystem::remove_all(directory, error);
}

void testBackupRestorationAndArchives()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "recoverable.json";
    std::error_code error;
    std::filesystem::remove_all(directory, error);

    Persistence::SaveRepository repository;
    Persistence::SaveData backupData = p1Save();
    backupData.pet.happinessHearts = 2;
    expect(repository.save(path, backupData).success, "first recovery save must succeed");

    Persistence::SaveData currentData = backupData;
    currentData.pet.happinessHearts = 4;
    expect(repository.save(path, currentData).success, "second recovery save must succeed");

    {
        std::ofstream stream(path, std::ios::trunc);
        stream << "this is not a save";
    }
    expect(!repository.load(path).success(), "the deliberately damaged current save must be rejected");

    expect(repository.restoreBackup(path).success, "a valid P1 backup must restore over damage");
    const Persistence::LoadResult restored = repository.load(path);
    expect(restored.success(), "the restored main save must load");
    if (restored.data) {
        expect(restored.data->pet.happinessHearts == backupData.pet.happinessHearts,
            "restoration must recover the P1 state stored in the backup");
    }
    expect(repository.load(path.string() + ".bak").success(),
        "restoration must retain the valid backup file");

    {
        std::ofstream stream(path, std::ios::trunc);
        stream << "damaged again";
    }
    expect(repository.archiveCorruptSave(path).success, "damaged save must be archivable");
    expect(!std::filesystem::exists(path), "archiving must remove the damaged file from its active path");
    expect(std::filesystem::exists(path.string() + ".corrupt"),
        "the first damaged save must be retained in a recovery archive");

    expect(repository.save(path, currentData).success, "a reset test save must succeed");
    expect(repository.archiveResetSave(path).success, "an explicit reset must archive its active save");
    expect(std::filesystem::exists(path.string() + ".reset"),
        "an explicit reset must retain the prior generation separately from corrupt saves");

    std::filesystem::remove_all(directory, error);
}

void testLegacyPrototypeIsNeverConvertedToP1()
{
    const std::filesystem::path directory = testDirectory();
    const std::filesystem::path path = directory / "legacy.json";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);

    {
        std::ofstream stream(path);
        stream << R"({
  "formatVersion": 1,
  "lastSavedUnixSeconds": 1725000000,
  "seed": 42,
  "pet": { "species": 0 }
})";
    }

    Persistence::SaveRepository repository;
    const Persistence::LoadResult loaded = repository.load(path);
    expect(!loaded.success() && loaded.isLegacyPrototype(),
        "the retired prototype save must be recognised but never converted into P1 state");
    expect(repository.archiveLegacySave(path).success,
        "an incompatible prototype save must be recoverably archived");
    expect(std::filesystem::exists(path.string() + ".legacy"),
        "the incompatible prototype save must remain available under the legacy suffix");

    std::filesystem::remove_all(directory, error);
}

} // namespace

int main()
{
    testP1RoundTripAndBackup();
    testInvalidDataIsRejected();
    testVersionTwoP1SaveKeepsAConservativeEvolutionDefault();
    testBackupRestorationAndArchives();
    testLegacyPrototypeIsNeverConvertedToP1();

    if (failures == 0) {
        std::cout << "SaveRepositoryTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
