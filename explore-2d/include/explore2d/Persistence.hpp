#pragma once

#include "explore2d/Session.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace explore2d {

struct LoadResult final {
    std::optional<SessionSnapshot> snapshot;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return snapshot.has_value(); }
};

[[nodiscard]] bool saveSnapshot(const SessionSnapshot& snapshot, const std::filesystem::path& path, std::string* error = nullptr);
[[nodiscard]] LoadResult loadSnapshot(const std::filesystem::path& path);

} // namespace explore2d
