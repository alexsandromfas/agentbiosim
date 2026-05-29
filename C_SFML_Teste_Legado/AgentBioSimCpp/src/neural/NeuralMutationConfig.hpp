#pragma once

namespace agentbiosim::neural
{
// Unified mutation config: every brain accepts this struct.
// Per-component overrides default to -1, which means "use base rate/strength".
struct NeuralMutationConfig
{
    double baseRate = 0.05;
    double baseStrength = 0.08;
    double gateRate = -1.0;
    double gateStrength = -1.0;
    double shortcutRate = -1.0;
    double shortcutStrength = -1.0;
    double recurrentRate = -1.0;
    double recurrentStrength = -1.0;

    [[nodiscard]] double effectiveGateRate() const noexcept
    {
        return gateRate < 0.0 ? baseRate : gateRate;
    }
    [[nodiscard]] double effectiveGateStrength() const noexcept
    {
        return gateStrength < 0.0 ? baseStrength : gateStrength;
    }
    [[nodiscard]] double effectiveShortcutRate() const noexcept
    {
        return shortcutRate < 0.0 ? baseRate : shortcutRate;
    }
    [[nodiscard]] double effectiveShortcutStrength() const noexcept
    {
        return shortcutStrength < 0.0 ? baseStrength : shortcutStrength;
    }
    [[nodiscard]] double effectiveRecurrentRate() const noexcept
    {
        return recurrentRate < 0.0 ? baseRate : recurrentRate;
    }
    [[nodiscard]] double effectiveRecurrentStrength() const noexcept
    {
        return recurrentStrength < 0.0 ? baseStrength : recurrentStrength;
    }
};
} // namespace agentbiosim::neural
