#include "TamagotchiCna/Persistence/SaveRepository.hpp"

#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <system_error>

namespace TamagotchiCna::Persistence {
namespace {

constexpr std::size_t MaximumSaveBytes = 64U * 1024U;
constexpr int MaximumPersistedMinutes = std::numeric_limits<int>::max();
constexpr int MaximumPersistedWeight = 9'999;
constexpr int MaximumPersistedAge = 9'999;
constexpr int MaximumMedicineDoses = 4;

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
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
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

std::optional<std::string> extractIdentifier(const std::string& json, const std::string_view key)
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
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
        ++valueStart;
    }
    if (valueStart >= json.size() || json[valueStart] != '\"') {
        return std::nullopt;
    }

    const std::size_t valueEnd = json.find('\"', valueStart + 1U);
    const std::size_t escape = json.find('\\', valueStart + 1U);
    if (valueEnd == std::string::npos || (escape != std::string::npos && escape < valueEnd)) {
        return std::nullopt;
    }
    return json.substr(valueStart + 1U, valueEnd - valueStart - 1U);
}

bool validIdentifier(const std::string_view value) noexcept
{
    if (value.empty() || value.size() > 64U) {
        return false;
    }
    for (const char character : value) {
        const bool allowed = (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9') || character == '-';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool validStage(const int value) noexcept
{
    return value >= static_cast<int>(Domain::ProgramStage::Egg)
        && value <= static_cast<int>(Domain::ProgramStage::End);
}

bool validBoolean(const int value) noexcept
{
    return value == 0 || value == 1;
}

bool validAttentionReason(const int value) noexcept
{
    return value >= static_cast<int>(Domain::ProgramAttentionReason::None)
        && value <= static_cast<int>(Domain::ProgramAttentionReason::Discipline);
}

bool validTeenLineage(const int value) noexcept
{
    return value >= static_cast<int>(Domain::ProgramTeenLineage::None)
        && value <= static_cast<int>(Domain::ProgramTeenLineage::TypeB);
}

bool validPetState(const Domain::ProgramPetState& pet) noexcept
{
    return validIdentifier(pet.characterId)
        && pet.minutesSinceClockSet >= 0 && pet.minutesSinceClockSet <= MaximumPersistedMinutes
        && pet.minutesSinceHatch >= 0 && pet.minutesSinceHatch <= MaximumPersistedMinutes
        && pet.age >= 0 && pet.age <= MaximumPersistedAge
        && pet.weight >= 0 && pet.weight <= MaximumPersistedWeight
        && pet.hungerHearts >= 0 && pet.hungerHearts <= 4
        && pet.happinessHearts >= 0 && pet.happinessHearts <= 4
        && pet.disciplineBars >= 0 && pet.disciplineBars <= 4
        && pet.medicineDosesRemaining >= 0
        && pet.medicineDosesRemaining <= MaximumMedicineDoses
        && pet.clockMinutesOfDay >= 0 && pet.clockMinutesOfDay < 24 * 60
        && pet.wasteCount >= 0 && pet.wasteCount <= MaximumPersistedWeight
        && pet.careMistakes >= 0 && pet.careMistakes <= MaximumPersistedAge
        && pet.disciplineMistakes >= 0 && pet.disciplineMistakes <= MaximumPersistedAge
        && validTeenLineage(static_cast<int>(pet.teenLineage))
        && pet.stageAwakeMinutes >= 0 && pet.stageAwakeMinutes <= MaximumPersistedMinutes
        && pet.hungerLossElapsedMinutes >= 0
        && pet.hungerLossElapsedMinutes <= MaximumPersistedMinutes
        && pet.happinessLossElapsedMinutes >= 0
        && pet.happinessLossElapsedMinutes <= MaximumPersistedMinutes
        && pet.needHeartDecrementsSinceDisciplineCall >= 0
        && pet.needHeartDecrementsSinceDisciplineCall <= MaximumPersistedMinutes
        && pet.disciplineCallQuota >= 0 && pet.disciplineCallQuota <= 4
        && pet.disciplineCallsIssued >= 0
        && pet.disciplineCallsIssued <= pet.disciplineCallQuota
        && pet.attentionDeadlineMinutes >= -1
        // -1 is the deliberate P1 sentinel used after an ignored attention
        // call. It prevents the same empty meter from immediately raising a
        // duplicate call until the player restores it.
        && pet.nextAttentionEligibleMinutes >= -1
        && validAttentionReason(static_cast<int>(pet.attentionReason))
        && validStage(static_cast<int>(pet.stage));
}

bool validSaveData(const SaveData& data) noexcept
{
    return data.formatVersion == SaveData::CurrentFormatVersion
        && data.lastSavedUnixSeconds >= 0
        && data.clockSetupMinutes >= 0 && data.clockSetupMinutes < 24 * 60
        && Presentation::isValidDeviceShellId(data.shellId)
        && validIdentifier(data.programId)
        && validPetState(data.pet);
}

std::string serialise(const SaveData& data)
{
    const Domain::ProgramPetState& pet = data.pet;
    return "{\n"
        "  \"formatVersion\": " + std::to_string(data.formatVersion) + ",\n"
        "  \"programId\": \"" + data.programId + "\",\n"
        "  \"shellId\": \"" + data.shellId + "\",\n"
        "  \"lastSavedUnixSeconds\": " + std::to_string(data.lastSavedUnixSeconds) + ",\n"
        "  \"clockSetPaused\": " + std::to_string(data.clockSetPaused ? 1 : 0) + ",\n"
        "  \"clockSetupMinutes\": " + std::to_string(data.clockSetupMinutes) + ",\n"
        "  \"seed\": " + std::to_string(data.seed) + ",\n"
        "  \"pet\": {\n"
        "    \"characterId\": \"" + pet.characterId + "\",\n"
        "    \"stage\": " + std::to_string(static_cast<int>(pet.stage)) + ",\n"
        "    \"minutesSinceClockSet\": " + std::to_string(pet.minutesSinceClockSet) + ",\n"
        "    \"minutesSinceHatch\": " + std::to_string(pet.minutesSinceHatch) + ",\n"
        "    \"age\": " + std::to_string(pet.age) + ",\n"
        "    \"weight\": " + std::to_string(pet.weight) + ",\n"
        "    \"hungerHearts\": " + std::to_string(pet.hungerHearts) + ",\n"
        "    \"happinessHearts\": " + std::to_string(pet.happinessHearts) + ",\n"
        "    \"disciplineBars\": " + std::to_string(pet.disciplineBars) + ",\n"
        "    \"medicineDosesRemaining\": " + std::to_string(pet.medicineDosesRemaining) + ",\n"
        "    \"clockMinutesOfDay\": " + std::to_string(pet.clockMinutesOfDay) + ",\n"
        "    \"wasteCount\": " + std::to_string(pet.wasteCount) + ",\n"
        "    \"careMistakes\": " + std::to_string(pet.careMistakes) + ",\n"
        "    \"disciplineMistakes\": " + std::to_string(pet.disciplineMistakes) + ",\n"
        "    \"teenLineage\": "
            + std::to_string(static_cast<int>(pet.teenLineage)) + ",\n"
        "    \"teenStartedWithNoDiscipline\": "
            + std::to_string(pet.teenStartedWithNoDiscipline ? 1 : 0) + ",\n"
        "    \"stageAwakeMinutes\": " + std::to_string(pet.stageAwakeMinutes) + ",\n"
        "    \"hungerLossElapsedMinutes\": "
            + std::to_string(pet.hungerLossElapsedMinutes) + ",\n"
        "    \"happinessLossElapsedMinutes\": "
            + std::to_string(pet.happinessLossElapsedMinutes) + ",\n"
        "    \"needHeartDecrementsSinceDisciplineCall\": "
            + std::to_string(pet.needHeartDecrementsSinceDisciplineCall) + ",\n"
        "    \"disciplineCallQuota\": " + std::to_string(pet.disciplineCallQuota) + ",\n"
        "    \"disciplineCallsIssued\": " + std::to_string(pet.disciplineCallsIssued) + ",\n"
        "    \"pendingDisciplineCall\": "
            + std::to_string(pet.pendingDisciplineCall ? 1 : 0) + ",\n"
        "    \"attentionDeadlineMinutes\": " + std::to_string(pet.attentionDeadlineMinutes) + ",\n"
        "    \"nextAttentionEligibleMinutes\": "
            + std::to_string(pet.nextAttentionEligibleMinutes) + ",\n"
        "    \"asleep\": " + std::to_string(pet.asleep ? 1 : 0) + ",\n"
        "    \"lightOff\": " + std::to_string(pet.lightOff ? 1 : 0) + ",\n"
        "    \"sick\": " + std::to_string(pet.sick ? 1 : 0) + ",\n"
        "    \"attentionReason\": "
            + std::to_string(static_cast<int>(pet.attentionReason)) + "\n"
        "  }\n"
        "}\n";
}

LoadResult invalid(const std::string_view reason)
{
    return LoadResult{.data = std::nullopt, .legacyPrototype = false, .error = std::string(reason)};
}

LoadResult legacyPrototype()
{
    return LoadResult{.data = std::nullopt, .legacyPrototype = true,
        .error = "This slot belongs to the retired pre-P1 prototype."};
}

} // namespace

SaveResult SaveRepository::save(const std::filesystem::path& path, const SaveData& data) const
{
    if (path.empty() || path.filename().empty()) {
        return SaveResult{.success = false, .error = "Save path must name a file."};
    }
    if (!validSaveData(data)) {
        return SaveResult{.success = false, .error = "Refusing to write an invalid or unsupported P1 save."};
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
    if (!formatVersion) {
        return invalid("Save file is missing or repeats its format version.");
    }
    if (*formatVersion == 1) {
        return legacyPrototype();
    }
    if (*formatVersion != 2 && *formatVersion != 3 && *formatVersion != 4
        && *formatVersion != 5
        && *formatVersion != SaveData::CurrentFormatVersion) {
        return invalid("Unsupported save format version.");
    }

    const auto programId = extractIdentifier(json, "programId");
    const auto shellId = extractIdentifier(json, "shellId");
    const auto lastSaved = extractInteger<std::int64_t>(json, "lastSavedUnixSeconds");
    const auto clockSetPaused = extractInteger<int>(json, "clockSetPaused");
    const auto clockSetupMinutes = extractInteger<int>(json, "clockSetupMinutes");
    const auto seed = extractInteger<std::uint64_t>(json, "seed");
    const auto characterId = extractIdentifier(json, "characterId");
    const auto stage = extractInteger<int>(json, "stage");
    const auto minutesSinceClockSet = extractInteger<int>(json, "minutesSinceClockSet");
    const auto minutesSinceHatch = extractInteger<int>(json, "minutesSinceHatch");
    const auto age = extractInteger<int>(json, "age");
    const auto weight = extractInteger<int>(json, "weight");
    const auto hungerHearts = extractInteger<int>(json, "hungerHearts");
    const auto happinessHearts = extractInteger<int>(json, "happinessHearts");
    const auto disciplineBars = extractInteger<int>(json, "disciplineBars");
    const auto medicineDosesRemaining = extractInteger<int>(json, "medicineDosesRemaining");
    const auto clockMinutesOfDay = extractInteger<int>(json, "clockMinutesOfDay");
    const auto wasteCount = extractInteger<int>(json, "wasteCount");
    const auto careMistakes = extractInteger<int>(json, "careMistakes");
    const auto disciplineMistakes = extractInteger<int>(json, "disciplineMistakes");
    const auto teenLineage = extractInteger<int>(json, "teenLineage");
    const auto teenStartedWithNoDiscipline = extractInteger<int>(json, "teenStartedWithNoDiscipline");
    const auto stageAwakeMinutes = extractInteger<int>(json, "stageAwakeMinutes");
    const auto hungerLossElapsedMinutes = extractInteger<int>(json, "hungerLossElapsedMinutes");
    const auto happinessLossElapsedMinutes = extractInteger<int>(json, "happinessLossElapsedMinutes");
    const auto needHeartDecrementsSinceDisciplineCall = extractInteger<int>(
        json, "needHeartDecrementsSinceDisciplineCall");
    const auto disciplineCallQuota = extractInteger<int>(json, "disciplineCallQuota");
    const auto disciplineCallsIssued = extractInteger<int>(json, "disciplineCallsIssued");
    const auto pendingDisciplineCall = extractInteger<int>(json, "pendingDisciplineCall");
    const auto attentionDeadlineMinutes = extractInteger<int>(json, "attentionDeadlineMinutes");
    const auto nextAttentionEligibleMinutes = extractInteger<int>(json, "nextAttentionEligibleMinutes");
    const auto asleep = extractInteger<int>(json, "asleep");
    const auto lightOff = extractInteger<int>(json, "lightOff");
    const auto sick = extractInteger<int>(json, "sick");
    const auto attentionReason = extractInteger<int>(json, "attentionReason");

    if (!programId || !lastSaved || !seed || !characterId || !stage || !minutesSinceClockSet
        || !minutesSinceHatch || !age || !weight || !hungerHearts || !happinessHearts
        || !disciplineBars || !medicineDosesRemaining || !clockMinutesOfDay || !wasteCount
        || !careMistakes || !attentionDeadlineMinutes || !nextAttentionEligibleMinutes || !asleep
        || !lightOff || !sick || !attentionReason) {
        return invalid("Save file is missing or repeats a required P1 field.");
    }
    if (*formatVersion >= 3
        && (!disciplineMistakes || !teenLineage || !teenStartedWithNoDiscipline)) {
        return invalid("Save file is missing or repeats an evolution-history field.");
    }
    if (*formatVersion >= 4
        && (!stageAwakeMinutes || !hungerLossElapsedMinutes || !happinessLossElapsedMinutes
            || !needHeartDecrementsSinceDisciplineCall || !disciplineCallQuota
            || !disciplineCallsIssued || !pendingDisciplineCall)) {
        return invalid("Save file is missing or repeats a P1 timer field.");
    }
    if (*formatVersion >= 5
        && (!shellId || !Presentation::isValidDeviceShellId(*shellId))) {
        return invalid("Save file is missing or contains an unsupported shell identifier.");
    }
    if (*formatVersion == SaveData::CurrentFormatVersion
        && (!clockSetPaused || !clockSetupMinutes)) {
        return invalid("Save file is missing or repeats its Clock SET pause state.");
    }
    if (!validIdentifier(*programId) || !validIdentifier(*characterId) || *lastSaved < 0
        || !validStage(*stage) || *minutesSinceClockSet < 0 || *minutesSinceHatch < 0
        || *age < 0 || *weight < 0 || *hungerHearts < 0 || *hungerHearts > 4
        || *happinessHearts < 0 || *happinessHearts > 4 || *disciplineBars < 0
        || *disciplineBars > 4 || *medicineDosesRemaining < 0
        || *medicineDosesRemaining > MaximumMedicineDoses || !validBoolean(*asleep)
        || *clockMinutesOfDay < 0 || *clockMinutesOfDay >= 24 * 60 || *wasteCount < 0
        || *careMistakes < 0 || *attentionDeadlineMinutes < -1
        || *nextAttentionEligibleMinutes < -1 || !validBoolean(*lightOff)
        || !validBoolean(*sick) || !validAttentionReason(*attentionReason)) {
        return invalid("Save file contains a value outside the supported P1 range.");
    }
    if (*formatVersion >= 3
        && (*disciplineMistakes < 0 || *disciplineMistakes > MaximumPersistedAge
            || !validTeenLineage(*teenLineage)
            || !validBoolean(*teenStartedWithNoDiscipline))) {
        return invalid("Save file contains an invalid P1 evolution-history value.");
    }
    if (*formatVersion >= 4
        && (*stageAwakeMinutes < 0 || *hungerLossElapsedMinutes < 0
            || *happinessLossElapsedMinutes < 0 || *needHeartDecrementsSinceDisciplineCall < 0
            || *stageAwakeMinutes > MaximumPersistedMinutes
            || *hungerLossElapsedMinutes > MaximumPersistedMinutes
            || *happinessLossElapsedMinutes > MaximumPersistedMinutes
            || *needHeartDecrementsSinceDisciplineCall > MaximumPersistedMinutes
            || *disciplineCallQuota < 0 || *disciplineCallQuota > 4
            || *disciplineCallsIssued < 0 || *disciplineCallsIssued > *disciplineCallQuota
            || !validBoolean(*pendingDisciplineCall))) {
        return invalid("Save file contains an invalid P1 timer value.");
    }
    if (*formatVersion == SaveData::CurrentFormatVersion
        && (!validBoolean(*clockSetPaused)
            || *clockSetupMinutes < 0 || *clockSetupMinutes >= 24 * 60)) {
        return invalid("Save file contains an invalid Clock SET pause state.");
    }

    SaveData data{};
    // Accepted older P1 data is migrated in memory and written back as the
    // current format at the next real save action. Pre-v5 slots receive the
    // stable default shell; pre-v6 slots receive a running clock without
    // changing any P1 simulation state.
    data.formatVersion = SaveData::CurrentFormatVersion;
    data.programId = *programId;
    data.shellId = *formatVersion >= 5
        ? *shellId : std::string(Presentation::DefaultDeviceShellId);
    data.lastSavedUnixSeconds = *lastSaved;
    data.clockSetPaused = *formatVersion == SaveData::CurrentFormatVersion
        && *clockSetPaused == 1;
    data.clockSetupMinutes = *formatVersion == SaveData::CurrentFormatVersion
        ? *clockSetupMinutes : *clockMinutesOfDay;
    data.seed = *seed;
    data.pet.characterId = *characterId;
    data.pet.stage = static_cast<Domain::ProgramStage>(*stage);
    data.pet.minutesSinceClockSet = *minutesSinceClockSet;
    data.pet.minutesSinceHatch = *minutesSinceHatch;
    data.pet.age = *age;
    data.pet.weight = *weight;
    data.pet.hungerHearts = *hungerHearts;
    data.pet.happinessHearts = *happinessHearts;
    data.pet.disciplineBars = *disciplineBars;
    data.pet.medicineDosesRemaining = *medicineDosesRemaining;
    data.pet.clockMinutesOfDay = *clockMinutesOfDay;
    data.pet.wasteCount = *wasteCount;
    data.pet.careMistakes = *careMistakes;
    data.pet.disciplineMistakes = disciplineMistakes.value_or(0);
    data.pet.teenLineage = teenLineage
        ? static_cast<Domain::ProgramTeenLineage>(*teenLineage)
        : Domain::ProgramTeenLineage::None;
    data.pet.teenStartedWithNoDiscipline = teenStartedWithNoDiscipline.value_or(0) == 1;
    data.pet.stageAwakeMinutes = stageAwakeMinutes.value_or(0);
    data.pet.hungerLossElapsedMinutes = hungerLossElapsedMinutes.value_or(0);
    data.pet.happinessLossElapsedMinutes = happinessLossElapsedMinutes.value_or(0);
    data.pet.needHeartDecrementsSinceDisciplineCall =
        needHeartDecrementsSinceDisciplineCall.value_or(0);
    // A pre-v4 slot has no exact timer phase. Preserve its visible meter and
    // start a conservative new discipline-call cycle rather than rejecting a
    // user's existing P1 pet.
    data.pet.disciplineCallQuota = disciplineCallQuota.value_or(4 - *disciplineBars);
    data.pet.disciplineCallsIssued = disciplineCallsIssued.value_or(0);
    data.pet.pendingDisciplineCall = pendingDisciplineCall.value_or(0) == 1;
    data.pet.attentionDeadlineMinutes = *attentionDeadlineMinutes;
    data.pet.nextAttentionEligibleMinutes = *nextAttentionEligibleMinutes;
    data.pet.asleep = *asleep == 1;
    data.pet.lightOff = *lightOff == 1;
    data.pet.sick = *sick == 1;
    data.pet.attentionReason = static_cast<Domain::ProgramAttentionReason>(*attentionReason);
    return LoadResult{.data = std::move(data), .legacyPrototype = false, .error = {}};
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

SaveResult SaveRepository::archiveLegacySave(const std::filesystem::path& path) const
{
    return archiveSaveFile(path, ".legacy");
}

} // namespace TamagotchiCna::Persistence
