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
// Simple RNN with recurrent state on the FIRST hidden layer.
// First hidden activation: tanh(W[0] @ input + b[0] + recurrentScale * (W_rec @ state))
// State update: state = clamp(memory_decay * state + (1 - memory_decay) * hidden, -clip, +clip)
// State persists between forward() calls; reset() zeroes it.
class SimpleRNNBrain
{
public:
    SimpleRNNBrain() = default;
    SimpleRNNBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] std::size_t stateSize() const noexcept { return state_.size(); }
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr);
    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const;

    // RNN-aware clone: honors config.future.rnnResetStateOnCopy (true = zero state in child).
    [[nodiscard]] SimpleRNNBrain clone() const;

    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const;

    bool mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng);

    void resetState() noexcept;
    [[nodiscard]] const std::vector<double>& recurrentState() const noexcept { return state_; }
    [[nodiscard]] const std::vector<double>& recurrentWeights() const noexcept { return recurrentWeights_; }
    // Phase 26: whole-network read access for the neural viewer (read-only).
    [[nodiscard]] const std::vector<std::vector<double>>& weights() const noexcept { return weights_; }
    [[nodiscard]] const std::vector<std::vector<double>>& biases() const noexcept { return biases_; }
    [[nodiscard]] double memoryDecay() const noexcept { return memoryDecay_; }
    [[nodiscard]] double stateClip() const noexcept { return stateClip_; }
    [[nodiscard]] double recurrentScale() const noexcept { return recurrentScale_; }
    [[nodiscard]] bool resetStateOnCopy() const noexcept { return resetStateOnCopy_; }

private:
    void initialize(std::mt19937_64& rng);
    std::vector<double> forwardImpl(const std::vector<double>& input,
                                     ActivationTrace* trace,
                                     bool updateState);

    BrainConfig config_{};
    BrainState brainState_{};
    std::vector<std::size_t> layerSizes_;
    std::vector<std::vector<double>> weights_;
    std::vector<std::vector<double>> biases_;
    // recurrent_weights: state_size x state_size, row-major.
    std::vector<double> recurrentWeights_;
    // Mutable runtime recurrent state per brain (per agent).
    mutable std::vector<double> state_;
    double recurrentScale_ = 0.35;
    double memoryDecay_ = 0.6;
    double stateClip_ = 1.0;
    bool resetStateOnCopy_ = true;
};
} // namespace agentbiosim::neural
