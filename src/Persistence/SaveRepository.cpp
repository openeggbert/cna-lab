#include "CnaTamagotchi/Persistence/SaveRepository.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace CnaTamagotchi::Persistence {
namespace {

constexpr std::size_t MaximumSaveBytes = 64U * 1024U;

template <typename Integer>
std::optional<Integer> extractInteger(const std::string& json, const std::string_view key)
{
    const std::string quotedKey = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = json.find(quotedKey);
    if (keyPosition == std::string::npos
        || json.find(quotedKey, keyPosition + quotedKey.size()) != std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colon = json.find(':', keyPosition + quotedKey.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    std::size_t valueStart = colon + 1U;
    while (valueStart < json.size()
           && (json[valueStart] == ' ' || json[valueStart] == '\t'
               || json[valueStart] == '\r' || json[valueStart] == '\n')) {
        ++valueStart;
    }
    if (valueStart == json.size()) {
        return std::nullopt;
    }

    Integer value{};
    const char* const first = json.data() + valueStart;
    const char* const last = json.data() + json.size();
    const auto [parsedEnd, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || parsedEnd == first) {
        return std::nullopt;
    }
    return value;
}

bool containsKey(const std::string& json, const std::string_view key)
{
    return json.find("\"" + std::string(key) + "\"") != std::string::npos;
}

bool validNeed(const int value) noexcept
{
    return value >= 0 && value <= 100;
}

bool validSpecies(const int value) noexcept
{
    return value >= static_cast<int>(Domain::PetSpecies::Puffin)
        && value <= static_cast<int>(Domain::PetSpecies::Cometling);
}

bool validLifeStage(const int value) noexcept
{
    return value >= static_cast<int>(Domain::LifeStage::Egg)
        && value <= static_cast<int>(Domain::LifeStage::Farewell);
}

bool validAttentionReason(const int value) noexcept
{
    return value >= static_cast<int>(Domain::AttentionReason::None)
        && value <= static_cast<int>(Domain::AttentionReason::Discipline);
}

std::string serialise(const SaveData& data)
{
    const Domain::Needs& needs = data.pet.needs;
    return "{\n"
        "  \"formatVersion\": " + std::to_string(data.formatVersion) + ",\n"
        "  \"lastSavedUnixSeconds\": " + std::to_string(data.lastSavedUnixSeconds) + ",\n"
        "  \"seed\": " + std::to_string(data.seed) + ",\n"
        "  \"pet\": {\n"
        "    \"species\": " + std::to_string(static_cast<int>(data.pet.species)) + ",\n"
        "    \"lifeStage\": " + std::to_string(static_cast<int>(data.pet.lifeStage)) + ",\n"
        "    \"weight\": " + std::to_string(data.pet.weight) + ",\n"
        "    \"careMistakes\": " + std::to_string(data.pet.careMistakes) + ",\n"
        "    \"ageMinutes\": " + std::to_string(data.pet.ageMinutes) + ",\n"
        "    \"clockMinutesOfDay\": " + std::to_string(data.pet.clockMinutesOfDay) + ",\n"
        "    \"wasteCount\": " + std::to_string(data.pet.wasteCount) + ",\n"
        "    \"attentionDeadlineMinutes\": "
            + std::to_string(data.pet.attentionDeadlineMinutes) + ",\n"
        "    \"nextAttentionEligibleMinutes\": "
            + std::to_string(data.pet.nextAttentionEligibleMinutes) + ",\n"
        "    \"asleep\": " + std::to_string(data.pet.asleep ? 1 : 0) + ",\n"
        "    \"lightOff\": " + std::to_string(data.pet.lightOff ? 1 : 0) + ",\n"
        "    \"sick\": " + std::to_string(data.pet.sick ? 1 : 0) + ",\n"
        "    \"attentionReason\": "
            + std::to_string(static_cast<int>(data.pet.attentionReason)) + ",\n"
        "    \"needs\": {\n"
        "      \"hunger\": " + std::to_string(needs.hunger) + ",\n"
        "      \"happiness\": " + std::to_string(needs.happiness) + ",\n"
        "      \"energy\": " + std::to_string(needs.energy) + ",\n"
        "      \"hygiene\": " + std::to_string(needs.hygiene) + ",\n"
        "      \"health\": " + std::to_string(needs.health) + ",\n"
        "      \"affection\": " + std::to_string(needs.affection) + ",\n"
        "      \"discipline\": " + std::to_string(needs.discipline) + "\n"
        "    }\n"
        "  }\n"
        "}\n";
}

LoadResult invalid(const std::string_view reason)
{
    return LoadResult{.data = std::nullopt, .error = std::string(reason)};
}

} // namespace

SaveResult SaveRepository::save(const std::filesystem::path& path, const SaveData& data) const
{
    if (path.empty() || path.filename().empty()) {
        return SaveResult{.success = false, .error = "Save path must name a file."};
    }
    if (data.formatVersion != SaveData::CurrentFormatVersion) {
        return SaveResult{.success = false, .error = "Refusing to write an unsupported save version."};
    }

    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return SaveResult{.success = false, .error = "Could not create save directory: " + error.message()};
        }
    }

    const std::filesystem::path temporaryPath = path.string() + ".tmp";
    const std::filesystem::path backupPath = path.string() + ".bak";
    std::filesystem::remove(temporaryPath, error);
    error.clear();

    {
        std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return SaveResult{.success = false, .error = "Could not open temporary save file."};
        }
        stream << serialise(data);
        stream.flush();
        if (!stream) {
            return SaveResult{.success = false, .error = "Could not write temporary save file."};
        }
    }

    if (std::filesystem::exists(path, error) && !error) {
        std::filesystem::copy_file(path, backupPath,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            return SaveResult{.success = false, .error = "Could not create save backup: " + error.message()};
        }
    }
    if (error) {
        return SaveResult{.success = false, .error = "Could not inspect existing save: " + error.message()};
    }

    std::filesystem::rename(temporaryPath, path, error);
    if (error) {
        return SaveResult{.success = false,
            .error = "Could not replace save atomically; temporary save was retained: " + error.message()};
    }
    return SaveResult{.success = true, .error = {}};
}

LoadResult SaveRepository::load(const std::filesystem::path& path) const
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return invalid("Could not open save file.");
    }

    const std::string json((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (!stream.good() && !stream.eof()) {
        return invalid("Could not read save file.");
    }
    if (json.empty() || json.size() > MaximumSaveBytes) {
        return invalid("Save file is empty or exceeds the size limit.");
    }

    const auto formatVersion = extractInteger<int>(json, "formatVersion");
    const auto lastSaved = extractInteger<std::int64_t>(json, "lastSavedUnixSeconds");
    const auto seed = extractInteger<std::uint64_t>(json, "seed");
    const auto species = extractInteger<int>(json, "species");
    const auto lifeStage = extractInteger<int>(json, "lifeStage");
    const auto weight = extractInteger<int>(json, "weight");
    const auto careMistakes = extractInteger<int>(json, "careMistakes");
    const auto ageMinutes = extractInteger<int>(json, "ageMinutes");
    const auto clockMinutesOfDay = extractInteger<int>(json, "clockMinutesOfDay");
    const auto wasteCount = extractInteger<int>(json, "wasteCount");
    const auto attentionDeadlineMinutes = extractInteger<int>(json, "attentionDeadlineMinutes");
    const auto nextAttentionEligibleMinutes = extractInteger<int>(json, "nextAttentionEligibleMinutes");
    const auto asleep = extractInteger<int>(json, "asleep");
    const auto lightOff = extractInteger<int>(json, "lightOff");
    const auto sick = extractInteger<int>(json, "sick");
    const auto attentionReason = extractInteger<int>(json, "attentionReason");
    const auto hunger = extractInteger<int>(json, "hunger");
    const auto happiness = extractInteger<int>(json, "happiness");
    const auto energy = extractInteger<int>(json, "energy");
    const auto hygiene = extractInteger<int>(json, "hygiene");
    const auto health = extractInteger<int>(json, "health");
    const auto affection = extractInteger<int>(json, "affection");
    const auto discipline = extractInteger<int>(json, "discipline");

    const bool hasCareSchedulerFields = containsKey(json, "clockMinutesOfDay")
        || containsKey(json, "wasteCount") || containsKey(json, "attentionDeadlineMinutes")
        || containsKey(json, "nextAttentionEligibleMinutes") || containsKey(json, "lightOff")
        || containsKey(json, "attentionReason");
    const bool hasCompleteCareSchedulerFields = clockMinutesOfDay && wasteCount
        && attentionDeadlineMinutes && nextAttentionEligibleMinutes && lightOff && attentionReason;

    if (!formatVersion || !lastSaved || !seed || !species || !lifeStage || !weight
        || !careMistakes || !ageMinutes || !asleep || !sick || !hunger || !happiness
        || !energy || !hygiene || !health || !affection || !discipline) {
        return invalid("Save file is missing or repeats a required numeric field.");
    }
    if (hasCareSchedulerFields && !hasCompleteCareSchedulerFields) {
        return invalid("Save file has an incomplete or repeated care-scheduler field.");
    }
    if (*formatVersion != SaveData::CurrentFormatVersion) {
        return invalid("Unsupported save format version.");
    }
    if (*lastSaved < 0 || !validSpecies(*species) || !validLifeStage(*lifeStage)
        || *weight < 0 || *careMistakes < 0 || *ageMinutes < 0
        || (*asleep != 0 && *asleep != 1) || (*sick != 0 && *sick != 1)
        || !validNeed(*hunger) || !validNeed(*happiness) || !validNeed(*energy)
        || !validNeed(*hygiene) || !validNeed(*health) || !validNeed(*affection)
        || !validNeed(*discipline)) {
        return invalid("Save file contains a value outside the supported range.");
    }
    if (hasCareSchedulerFields
        && (*clockMinutesOfDay < 0 || *clockMinutesOfDay >= 24 * 60 || *wasteCount < 0
            || *attentionDeadlineMinutes < -1 || *nextAttentionEligibleMinutes < 0
            || (*lightOff != 0 && *lightOff != 1) || !validAttentionReason(*attentionReason))) {
        return invalid("Save file contains a value outside the supported range.");
    }

    SaveData data{};
    data.formatVersion = *formatVersion;
    data.lastSavedUnixSeconds = *lastSaved;
    data.seed = *seed;
    data.pet.species = static_cast<Domain::PetSpecies>(*species);
    data.pet.lifeStage = static_cast<Domain::LifeStage>(*lifeStage);
    data.pet.weight = *weight;
    data.pet.careMistakes = *careMistakes;
    data.pet.ageMinutes = *ageMinutes;
    data.pet.asleep = *asleep == 1;
    data.pet.sick = *sick == 1;
    if (hasCareSchedulerFields) {
        data.pet.clockMinutesOfDay = *clockMinutesOfDay;
        data.pet.wasteCount = *wasteCount;
        data.pet.attentionDeadlineMinutes = *attentionDeadlineMinutes;
        data.pet.nextAttentionEligibleMinutes = *nextAttentionEligibleMinutes;
        data.pet.lightOff = *lightOff == 1;
        data.pet.attentionReason = static_cast<Domain::AttentionReason>(*attentionReason);
    }
    data.pet.needs = Domain::Needs{
        .hunger = *hunger,
        .happiness = *happiness,
        .energy = *energy,
        .hygiene = *hygiene,
        .health = *health,
        .affection = *affection,
        .discipline = *discipline,
    };
    return LoadResult{.data = data, .error = {}};
}

SaveResult SaveRepository::restoreBackup(const std::filesystem::path& path) const
{
    if (path.empty() || path.filename().empty()) {
        return SaveResult{.success = false, .error = "Save path must name a file."};
    }

    const std::filesystem::path backupPath = path.string() + ".bak";
    if (!load(backupPath).success()) {
        return SaveResult{.success = false, .error = "The backup save is missing or invalid."};
    }

    const std::filesystem::path temporaryPath = path.string() + ".tmp";
    std::error_code error;
    std::filesystem::remove(temporaryPath, error);
    error.clear();
    std::filesystem::copy_file(backupPath, temporaryPath,
        std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        return SaveResult{.success = false,
            .error = "Could not prepare the backup for restoration: " + error.message()};
    }

    std::filesystem::rename(temporaryPath, path, error);
    if (error) {
        return SaveResult{.success = false,
            .error = "Could not restore the backup atomically; temporary save was retained: "
                + error.message()};
    }
    return SaveResult{.success = true, .error = {}};
}

namespace {

SaveResult archiveSaveFile(const std::filesystem::path& path, const std::string_view suffix)
{
    if (path.empty() || path.filename().empty()) {
        return SaveResult{.success = false, .error = "Save path must name a file."};
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            return SaveResult{.success = false,
                .error = "Could not inspect the save for archiving: " + error.message()};
        }
        return SaveResult{.success = true, .error = {}};
    }

    for (int index = 0; index < 1'000; ++index) {
        const std::filesystem::path archivePath = path.string() + std::string(suffix)
            + (index == 0 ? std::string{} : "." + std::to_string(index));
        if (std::filesystem::exists(archivePath, error)) {
            if (error) {
                return SaveResult{.success = false,
                    .error = "Could not inspect a save archive: " + error.message()};
            }
            continue;
        }
        if (error) {
            return SaveResult{.success = false,
                .error = "Could not inspect a save archive: " + error.message()};
        }

        std::filesystem::rename(path, archivePath, error);
        if (error) {
            return SaveResult{.success = false,
                .error = "Could not archive the save: " + error.message()};
        }
        return SaveResult{.success = true, .error = {}};
    }

    return SaveResult{.success = false,
        .error = "Could not archive the save: too many archives exist."};
}

} // namespace

SaveResult SaveRepository::archiveCorruptSave(const std::filesystem::path& path) const
{
    return archiveSaveFile(path, ".corrupt");
}

SaveResult SaveRepository::archiveResetSave(const std::filesystem::path& path) const
{
    return archiveSaveFile(path, ".reset");
}

} // namespace CnaTamagotchi::Persistence
