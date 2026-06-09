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
// Shortcut MLP: dense feed-forward with a direct linear input->output bypass.
// Output = MLP(x) + shortcutScale * (shortcutWeights @ x + shortcutBias).
class ShortcutMLPBrain
{
public:
    ShortcutMLPBrain() = default;
    ShortcutMLPBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const;
    [[nodiscard]] ShortcutMLPBrain clone() const;
    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const;

    bool mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng);

    [[nodiscard]] const std::vector<double>& shortcutWeights() const noexcept { return shortcutWeights_; }
    [[nodiscard]] const std::vector<double>& shortcutBias() const noexcept { return shortcutBias_; }
    [[nodiscard]] double shortcutScale() const noexcept { return shortcutScale_; }
    // Phase 26: whole-network read access for the neural viewer (read-only).
    [[nodiscard]] const std::vector<std::vector<double>>& weights() const noexcept { return weights_; }
    [[nodiscard]] const std::vector<std::vector<double>>& biases() const noexcept { return biases_; }

private:
    void initialize(std::mt19937_64& rng);

    BrainConfig config_{};
    BrainState state_{};
    std::vector<std::size_t> layerSizes_;
    std::vector<std::vector<double>> weights_;
    std::vector<std::vector<double>> biases_;
    std::vector<double> shortcutWeights_;  // outputSize * inputSize, row-major
    std::vector<double> shortcutBias_;     // outputSize
    double shortcutScale_ = 0.25;
};
} // namespace agentbiosim::neural
