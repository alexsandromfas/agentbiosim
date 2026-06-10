#pragma once

#include "neural/BrainConfig.hpp"
#include "neural/BrainType.hpp"
#include "neural/BrainVariant.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Phase 28: capture/rebuild a brain's full learned state for persistence.
//
// Strategy: a BrainSnapshot stores the BrainConfig plus the *learned* parameters
// (dense weights/biases/gates/shortcut, RNN recurrent weights+state, or the NEAT
// graph + innovation counters + recurrent state). Rebuild reconstructs the brain
// via BrainFactory::createBrain(config) — which correctly derives every
// config-driven field and the correct topology — then overwrites the learned
// parameters. This keeps future mutation/forward behavior identical after a
// load, which the determinism selftest relies on.
namespace agentbiosim::neural
{
struct NeatNodeData
{
    std::int32_t id = 0;
    int kind = 1;        // 0 input, 1 hidden, 2 output
    double layer = 0.5;
    int activation = 1;  // 0 linear, 1 tanh, 2 sigmoid
};

struct NeatConnData
{
    std::int32_t src = 0;
    std::int32_t dst = 0;
    double weight = 0.0;
    bool enabled = true;
    bool recurrent = false;
    std::int32_t innovation = 0;
};

struct BrainSnapshot
{
    BrainConfig config{};  // identity + reconstruction recipe (topology, rates).
    std::vector<std::size_t> layerSizes;  // actual topology (handles resized inputs).

    // Dense / RNN learned parameters.
    std::vector<std::vector<double>> weights;
    std::vector<std::vector<double>> biases;
    std::vector<std::vector<double>> gates;       // gated / modulated
    std::vector<double> shortcutWeights;          // shortcut / modulated
    std::vector<double> shortcutBias;
    std::vector<double> recurrentWeights;         // RNN
    std::vector<double> recurrentState;           // RNN runtime state

    // NEAT graph + counters + recurrent state.
    std::vector<NeatNodeData> neatNodes;
    std::vector<NeatConnData> neatConnections;
    std::vector<std::pair<std::int32_t, double>> neatState;  // recurrent state by node id
    std::int32_t neatNextNodeId = 0;
    std::int32_t neatNextInnovation = 1;
};

// BrainSerializer is a friend of every brain class so it can read the few private
// fields the public accessors do not expose (e.g. NEAT innovation counters) and
// overwrite learned parameters when rebuilding.
struct BrainSerializer
{
    [[nodiscard]] static BrainSnapshot capture(const BrainVariant& brain);
    [[nodiscard]] static BrainVariant build(const BrainSnapshot& snapshot);

    // Stable architecture signature for the rebuilt brain (matches what
    // NeuralSystem stores per slot, so a loaded brain is not recreated).
    [[nodiscard]] static std::string signatureOf(const BrainSnapshot& snapshot);
};
} // namespace agentbiosim::neural
