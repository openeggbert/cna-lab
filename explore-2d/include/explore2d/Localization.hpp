#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace explore2d {

// A game-facing string with an always-available fallback and optional
// translations addressed by stable language IDs such as "en" or "cs".
// Plain string literals remain source-compatible and become fallback text.
class LocalizedText final {
public:
    LocalizedText() = default;
    LocalizedText(const char* fallback);
    LocalizedText(std::string fallback);

    LocalizedText& addTranslation(std::string languageId, std::string value);

    [[nodiscard]] std::string_view resolve(std::string_view languageId) const noexcept;
    [[nodiscard]] const std::string& fallback() const noexcept { return fallback_; }
    [[nodiscard]] const std::map<std::string, std::string>& translations() const noexcept {
        return translations_;
    }
    [[nodiscard]] bool empty() const noexcept { return fallback_.empty(); }

private:
    std::string fallback_;
    std::map<std::string, std::string> translations_;
};

struct LanguageDefinition final {
    std::string id;
    LocalizedText label;
};

struct LocalizationDefinition final {
    std::string defaultLanguage{"en"};
    std::vector<LanguageDefinition> languages{{"en", "English"}};

    [[nodiscard]] const LanguageDefinition* language(std::string_view id) const noexcept;
    [[nodiscard]] bool supports(std::string_view id) const noexcept;
    [[nodiscard]] std::string_view normalized(std::string_view id) const noexcept;
};

} // namespace explore2d
