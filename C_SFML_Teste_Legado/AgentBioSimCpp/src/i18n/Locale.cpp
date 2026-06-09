#include "i18n/Locale.hpp"

#include <atomic>

namespace agentbiosim::i18n
{
namespace
{
// Function-local atomic avoids static-initialization-order issues and is
// trivially safe to read from the UI thread while the command-apply path
// writes it. Relaxed ordering is enough: there is no other state published
// alongside the language, and a one-frame delay on a language flip is benign.
std::atomic<Language>& activeLanguage() noexcept
{
    static std::atomic<Language> value{Language::PtBr};
    return value;
}
} // namespace

Language language() noexcept
{
    return activeLanguage().load(std::memory_order_relaxed);
}

void setLanguage(const Language language) noexcept
{
    activeLanguage().store(language, std::memory_order_relaxed);
}

Language languageFromTag(const std::string_view tag) noexcept
{
    if (tag == "en") return Language::En;
    return Language::PtBr;
}

const char* languageTag(const Language language) noexcept
{
    return language == Language::En ? "en" : "pt-br";
}

const char* tr(const char* ptbr, const char* en) noexcept
{
    return language() == Language::En ? en : ptbr;
}
} // namespace agentbiosim::i18n
