#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace CnaTamagotchi::Presentation {

struct ShellRgba final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha{255U};
};

struct DeviceShellStyle final {
    std::string_view id;
    std::string_view displayName;
    ShellRgba outline;
    ShellRgba body;
    ShellRgba bodyShadow;
    ShellRgba bodyHighlight;
    ShellRgba button;
    ShellRgba buttonShadow;
    ShellRgba buttonHighlight;
    bool translucent{false};
};

// These five palettes represent common international-P1 shell families. They
// are independently authored presentation colours, not sampled retail assets.
inline constexpr std::array<DeviceShellStyle, 5> DeviceShellStyles{{
    {
        .id = "translucent-blue-yellow",
        .displayName = "Translucent Blue / Yellow",
        .outline = {0U, 79U, 94U, 255U},
        .body = {0U, 176U, 190U, 220U},
        .bodyShadow = {0U, 121U, 138U, 255U},
        .bodyHighlight = {112U, 235U, 235U, 126U},
        .button = {248U, 203U, 65U, 255U},
        .buttonShadow = {180U, 125U, 25U, 255U},
        .buttonHighlight = {255U, 240U, 146U, 255U},
        .translucent = true,
    },
    {
        .id = "blue-yellow",
        .displayName = "Blue / Yellow",
        .outline = {14U, 49U, 104U, 255U},
        .body = {38U, 104U, 198U, 255U},
        .bodyShadow = {22U, 72U, 151U, 255U},
        .bodyHighlight = {132U, 181U, 239U, 190U},
        .button = {250U, 205U, 56U, 255U},
        .buttonShadow = {181U, 126U, 24U, 255U},
        .buttonHighlight = {255U, 241U, 151U, 255U},
    },
    {
        .id = "pink-yellow",
        .displayName = "Pink / Yellow",
        .outline = {118U, 43U, 82U, 255U},
        .body = {231U, 102U, 163U, 255U},
        .bodyShadow = {184U, 68U, 124U, 255U},
        .bodyHighlight = {255U, 184U, 216U, 190U},
        .button = {250U, 207U, 61U, 255U},
        .buttonShadow = {181U, 128U, 25U, 255U},
        .buttonHighlight = {255U, 243U, 153U, 255U},
    },
    {
        .id = "green-yellow",
        .displayName = "Green / Yellow",
        .outline = {34U, 86U, 52U, 255U},
        .body = {72U, 165U, 91U, 255U},
        .bodyShadow = {42U, 121U, 65U, 255U},
        .bodyHighlight = {166U, 226U, 153U, 190U},
        .button = {249U, 205U, 57U, 255U},
        .buttonShadow = {179U, 126U, 25U, 255U},
        .buttonHighlight = {255U, 241U, 150U, 255U},
    },
    {
        .id = "white-blue",
        .displayName = "White / Blue",
        .outline = {91U, 98U, 108U, 255U},
        .body = {239U, 238U, 226U, 255U},
        .bodyShadow = {191U, 196U, 194U, 255U},
        .bodyHighlight = {255U, 255U, 250U, 210U},
        .button = {48U, 113U, 194U, 255U},
        .buttonShadow = {23U, 72U, 139U, 255U},
        .buttonHighlight = {145U, 193U, 242U, 255U},
    },
}};

inline constexpr std::string_view DefaultDeviceShellId = DeviceShellStyles.front().id;

[[nodiscard]] inline constexpr bool isValidDeviceShellId(const std::string_view id) noexcept
{
    for (const DeviceShellStyle& style : DeviceShellStyles) {
        if (style.id == id)
            return true;
    }
    return false;
}

[[nodiscard]] inline constexpr const DeviceShellStyle&
deviceShellStyle(const std::string_view id) noexcept
{
    for (const DeviceShellStyle& style : DeviceShellStyles) {
        if (style.id == id)
            return style;
    }
    return DeviceShellStyles.front();
}

[[nodiscard]] inline constexpr std::string_view
nextDeviceShellId(const std::string_view current) noexcept
{
    for (std::size_t index = 0; index < DeviceShellStyles.size(); ++index) {
        if (DeviceShellStyles[index].id == current) {
            return DeviceShellStyles[(index + 1U) % DeviceShellStyles.size()].id;
        }
    }
    return DefaultDeviceShellId;
}

} // namespace CnaTamagotchi::Presentation
