#pragma once

namespace agentbiosim::ui
{
// Phase 25: single source of truth for the Dear ImGui visual language — colors,
// spacing, rounding and typography. Applied once after ImGui::SFML::Init so that
// every panel (menu, toolbar, dock, preferences) and every future UI phase (26,
// 30, 31) shares one coherent, modern, dark theme instead of the raw ImGui gray.
//
// Design intent (UI/UX, Gestalt):
//   * dark neutral surfaces with one teal/green accent for primary actions and
//     selected/active state (echoes the biological-simulation domain);
//   * consistent 8px spacing rhythm and rounded corners for a soft, modern feel;
//   * clear hover/active/disabled states so affordance is obvious.
void applyImGuiTheme();

// Accent color (RGBA floats 0..1) so panels can reuse it for highlights without
// duplicating the literal.
struct ThemeAccent
{
    float r, g, b, a;
};
[[nodiscard]] ThemeAccent themeAccent() noexcept;
} // namespace agentbiosim::ui
