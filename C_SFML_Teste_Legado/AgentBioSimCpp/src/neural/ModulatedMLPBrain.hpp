#pragma once

#include "neural/ActivationTrace.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainState.hpp"
#include "neural/NeuralMutationConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
// Modulated MLP: Gated + Shortcut combined.
// Hidden layers apply gates; final output adds shortcutScale * (Wshort @ x + bShort).
class ModulatedMLPBrain
{
public:
    ModulatedMLPBrain() = default;
    ModulatedMLPBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const;
    [[nodiscard]] ModulatedMLPBrain clone() const;
    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const;

    bool mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng);

    [[nodiscard]] const std::vector<std::vector<double>>& gates() const noexcept { return gates_; }
    [[nodiscard]] const std::vector<double>& shortcutWeights() const noexcept { return shortcutWeights_; }
    [[nodiscard]] double shortcutScale() const noexcept { return shortcutScale_; }

private:
    void initialize(std::mt19937_64& rng);
    void clampGates();

    BrainConfig config_{};
    BrainState state_{};
    std::vector<std::size_t> layerSizes_;
    std::vector<std::vector<double>> weights_;
    std::vector<std::vector<double>> biases_;
    std::vector<std::vector<double>> gates_;
    std::vector<double> shortcutWeights_;
    std::vector<double> shortcutBias_;
    double shortcutScale_ = 0.25;
};
} // namespace agentbiosim::neural
