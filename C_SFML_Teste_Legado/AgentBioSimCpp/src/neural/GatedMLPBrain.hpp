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
// Gated MLP: dense feed-forward with per-hidden-neuron scalar gates that
// multiply post-tanh activations. Gates are clamped to [gateMin, gateMax].
class GatedMLPBrain
{
public:
    GatedMLPBrain() = default;
    GatedMLPBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const;
    [[nodiscard]] GatedMLPBrain clone() const;
    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const;

    bool mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng);

    [[nodiscard]] const std::vector<std::vector<double>>& gates() const noexcept { return gates_; }
    // Phase 26: whole-network read access for the neural viewer (read-only).
    [[nodiscard]] const std::vector<std::vector<double>>& weights() const noexcept { return weights_; }
    [[nodiscard]] const std::vector<std::vector<double>>& biases() const noexcept { return biases_; }

private:
    void initialize(std::mt19937_64& rng);
    void clampGates();

    BrainConfig config_{};
    BrainState state_{};
    std::vector<std::size_t> layerSizes_;
    std::vector<std::vector<double>> weights_;
    std::vector<std::vector<double>> biases_;
    std::vector<std::vector<double>> gates_;  // one vector per hidden layer
};
} // namespace agentbiosim::neural
