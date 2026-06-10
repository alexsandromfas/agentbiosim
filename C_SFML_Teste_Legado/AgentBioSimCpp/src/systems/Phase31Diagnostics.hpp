#pragma once

#include <cstddef>
#include <string>

namespace agentbiosim::systems
{
// Phase 31 selftest: UI parity. Headless (no window/ImGui):
//   * essential keyboard shortcuts dispatch the right commands (real
//     InputRouter fed with synthetic sf::Event — no window needed), including
//     the Phase 31 fixes (H opens the LIVE help window flag, V toggles the
//     selected-agent vision overlay, W no longer pans);
//   * every preferences tab's parameter model resolves without crashing;
//   * critical flows end to end: spawn agent/food, clear food, paint/erase
//     obstacle, species ops (create/assign/label/color/population/remove),
//     per-species neural reset actually replaces brains, organism
//     export -> import roundtrip preserves the mind, save/open roundtrip;
//   * window-opening commands are accepted by the engine (UI smoke proper is
//     the manual app launch — ImGui windows need a GL context).
struct Phase31ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase31ValidationSummary runPhase31Validation();
} // namespace agentbiosim::systems
