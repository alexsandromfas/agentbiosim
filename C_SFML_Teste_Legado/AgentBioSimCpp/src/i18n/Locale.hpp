#pragma once

#include <string>
#include <string_view>

// Phase 25.2: lightweight internationalization (i18n) layer.
//
// The whole application uses two languages: Brazilian Portuguese (the default)
// and English. There is exactly one source of truth for the *active* language
// (a process-global held in Locale.cpp); every place that needs a localized
// string asks this module which variant to use.
//
// Two complementary mechanisms consume that state, on purpose:
//
//   * `tr(ptbr, en)` — for free-form UI strings written inline in the UI code
//     (menus, buttons, help text). Both translations sit side by side at the
//     call site, so there is no separate catalog to keep in sync and no string
//     keys to typo. Ideal when the string lives next to the widget that draws
//     it.
//
//   * `Text` / table columns — for translations that already live in DATA
//     tables (parameter labels, enum values, tooltips in ParameterMetadata).
//     Those tables simply grow a second column and resolve it through the same
//     active-language state.
//
// This module is a dependency-free leaf: it includes nothing from the project,
// so any layer (core, config, ui) may use it without creating a dependency
// cycle.
namespace agentbiosim::i18n
{
enum class Language
{
    PtBr,
    En
};

// The active UI language. Reads are cheap and lock-free; writes are expected
// to happen on the main thread when the user changes the preference.
[[nodiscard]] Language language() noexcept;
void setLanguage(Language language) noexcept;

// Registry tag <-> enum mapping. The `ui_language` parameter stores the tag
// "pt-br" or "en"; unknown tags fall back to Portuguese.
[[nodiscard]] Language languageFromTag(std::string_view tag) noexcept;
[[nodiscard]] const char* languageTag(Language language) noexcept;

// Workhorse for inline UI strings. Returns the variant for the active
// language. The arguments must outlive the returned pointer; passing string
// literals (static lifetime) is the intended use.
[[nodiscard]] const char* tr(const char* ptbr, const char* en) noexcept;

// A bilingual string pair for storage in data tables. `get()` resolves it
// against the active language using the same rule as `tr`.
struct Text
{
    const char* ptbr = "";
    const char* en = "";

    [[nodiscard]] const char* get() const noexcept { return tr(ptbr, en); }
};
} // namespace agentbiosim::i18n
