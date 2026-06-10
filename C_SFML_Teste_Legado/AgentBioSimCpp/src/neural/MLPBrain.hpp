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
class MLPBrain
{
public:
    MLPBrain() = default;
    MLPBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input, ActivationTrace* trace = nullptr) const;
    [[nodiscard]] MLPBrain clone() const;
    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const;

    bool mutate(double rate, double strength, std::mt19937_64& rng);
    bool mutate(const NeuralMutationConfig& config, std::mt19937_64& rng);
    bool resizeInput(std::size_t newInputSize, std::mt19937_64& rng);

    [[nodiscard]] const std::vector<double>& weightsAt(std::size_t layer) const;
    [[nodiscard]] const std::vector<double>& biasesAt(std::size_t layer) const;
    // Phase 26: whole-network read access for the neural viewer (read-only).
    [[nodiscard]] const std::vector<std::vector<double>>& weights() const noexcept { return weights_; }
    [[nodiscard]] const std::vector<std::vector<double>>& biases() const noexcept { return biases_; }
    bool setLayerForTesting(std::size_t layer, std::vector<double> weights, std::vector<double> biases);

    friend struct BrainSerializer;  // Phase 28: persistence access.

private:
    void initialize(std::mt19937_64& rng);
    [[nodiscard]] static std::vector<double> normalizedInput(const std::vector<double>& input, std::size_t expectedSize);

    BrainConfig config_{};
    BrainState state_{};
    std::vector<std::size_t> layerSizes_;
    std::vector<std::vector<double>> weights_;
    std::vector<std::vector<double>> biases_;
};
} // namespace agentbiosim::neural
