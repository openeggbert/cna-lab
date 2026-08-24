#include "CnaTamagotchi/Presentation/DeviceShell.hpp"

#include <iostream>
#include <set>
#include <string>

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

void testCatalogueIsCompleteAndStable()
{
    const std::set<std::string> expected{
        "translucent-blue-yellow", "blue-yellow", "pink-yellow", "green-yellow", "white-blue",
    };
    std::set<std::string> actual;
    for (const Presentation::DeviceShellStyle& style : Presentation::DeviceShellStyles) {
        actual.emplace(style.id);
        expect(!style.displayName.empty(), "each shell must have a visible name");
        expect(style.body.alpha > 0U, "each shell must have a visible body");
    }
    expect(actual == expected, "the five promised P1 shell families must remain available");
    expect(actual.size() == Presentation::DeviceShellStyles.size(),
           "shell identifiers must be unique");
}

void testCyclingAndFallback()
{
    std::string_view id = Presentation::DefaultDeviceShellId;
    for (std::size_t step = 0; step < Presentation::DeviceShellStyles.size(); ++step) {
        expect(Presentation::isValidDeviceShellId(id),
               "every shell reached by cycling must be valid");
        id = Presentation::nextDeviceShellId(id);
    }
    expect(id == Presentation::DefaultDeviceShellId,
           "cycling all shell variants must return to the default");
    expect(Presentation::nextDeviceShellId("unknown") == Presentation::DefaultDeviceShellId,
           "an unknown shell must recover to the safe default");
    expect(Presentation::deviceShellStyle("unknown").id == Presentation::DefaultDeviceShellId,
           "style lookup must fall back safely");
}

} // namespace

int main()
{
    testCatalogueIsCompleteAndStable();
    testCyclingAndFallback();
    if (failures == 0) {
        std::cout << "Device shell tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
