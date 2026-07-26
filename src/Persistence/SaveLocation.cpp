#include "CnaTamagotchi/Persistence/SaveLocation.hpp"

#include <cstdlib>
#include <system_error>

namespace CnaTamagotchi::Persistence {
namespace {

std::filesystem::path environmentPath(const char* const name)
{
    const char* const value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::filesystem::path(value)
                                               : std::filesystem::path{};
}

bool hasLegacyActiveSlot(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return true;
    }
    if (error) {
        // If we cannot inspect the legacy slot, prefer it. This prevents an
        // update from silently selecting a different writable location.
        return true;
    }

    return std::filesystem::exists(path.string() + ".bak", error) || error;
}

} // namespace

std::filesystem::path SaveLocation::legacySlot(const std::filesystem::path& workingDirectory)
{
    return workingDirectory / "saves" / "slot-1.json";
}

std::filesystem::path SaveLocation::platformDataDirectory()
{
#if defined(_WIN32)
    return environmentPath("LOCALAPPDATA");
#elif defined(__APPLE__)
    const std::filesystem::path home = environmentPath("HOME");
    return home.empty() ? std::filesystem::path{}
                        : home / "Library" / "Application Support";
#else
    const std::filesystem::path xdgDataHome = environmentPath("XDG_DATA_HOME");
    if (!xdgDataHome.empty()) {
        return xdgDataHome;
    }
    const std::filesystem::path home = environmentPath("HOME");
    return home.empty() ? std::filesystem::path{} : home / ".local" / "share";
#endif
}

std::filesystem::path SaveLocation::resolveSlot(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& platformDirectory)
{
    const std::filesystem::path legacy = legacySlot(workingDirectory);
    if (hasLegacyActiveSlot(legacy) || platformDirectory.empty()) {
        return legacy;
    }

    return platformDirectory / "cna-tamagotchi" / "saves" / "slot-1.json";
}

} // namespace CnaTamagotchi::Persistence
