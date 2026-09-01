#include "explore2d/Localization.hpp"

#include <utility>

namespace explore2d {

LocalizedText::LocalizedText(const char* const fallback)
    : fallback_{fallback == nullptr ? "" : fallback}
{
}

LocalizedText::LocalizedText(std::string fallback)
    : fallback_{std::move(fallback)}
{
}

LocalizedText& LocalizedText::addTranslation(std::string languageId, std::string value) {
    translations_.insert_or_assign(std::move(languageId), std::move(value));
    return *this;
}

std::string_view LocalizedText::resolve(const std::string_view languageId) const noexcept {
    const auto it = translations_.find(std::string{languageId});
    return it == translations_.end() ? std::string_view{fallback_} : std::string_view{it->second};
}

const LanguageDefinition* LocalizationDefinition::language(const std::string_view id) const noexcept {
    for (const LanguageDefinition& candidate : languages) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

bool LocalizationDefinition::supports(const std::string_view id) const noexcept {
    return language(id) != nullptr;
}

std::string_view LocalizationDefinition::normalized(const std::string_view id) const noexcept {
    if (supports(id)) return id;
    if (supports(defaultLanguage)) return defaultLanguage;
    return languages.empty() ? std::string_view{} : std::string_view{languages.front().id};
}

} // namespace explore2d
