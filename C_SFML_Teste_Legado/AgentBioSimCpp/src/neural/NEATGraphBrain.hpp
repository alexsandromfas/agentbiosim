#pragma once

#include "neural/ActivationTrace.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainState.hpp"
#include "neural/NeuralMutationConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::neural
{
// Phase 16: Variable-topology graph brain covering the three NEAT variants
// (BrainType::Neat = common, BrainType::SimpleNeat = simplified / protozoa-style,
// BrainType::RecurrentNeat = with allowed recurrent edges + state).
//
// One class handles all three variants. The Python class behaves the same way:
// brain_type drives mutation policy and whether recurrent edges are allowed.
//
// Note: each instance owns its graph and (if recurrent) its runtime state per agent.
// The brain is excluded from the batched dense path (supports_batch = false).
class NEATGraphBrain
{
public:
    enum class NodeKind : std::uint8_t
    {
        Input = 0,
        Hidden = 1,
        Output = 2
    };

    enum class ActivationKind : std::uint8_t
    {
        Linear = 0,
        Tanh = 1,
        Sigmoid = 2
    };

    struct Node
    {
        std::int32_t id = 0;
        NodeKind kind = NodeKind::Hidden;
        double layer = 0.5;
        ActivationKind activation = ActivationKind::Tanh;
    };

    struct Connection
    {
        std::int32_t src = 0;
        std::int32_t dst = 0;
        double weight = 0.0;
        bool enabled = true;
        bool recurrent = false;
        std::int32_t innovation = 0;
    };

    NEATGraphBrain() = default;
    NEATGraphBrain(BrainConfig config, std::mt19937_64& rng);

    [[nodiscard]] std::size_t inputSize() const noexcept;
    [[nodiscard]] std::size_t outputSize() const noexcept;
    [[nodiscard]] BrainType brainType() const noexcept { return brainType_; }
    [[nodiscard]] const std::vector<std::size_t>& layerSizes() const noexcept;
    [[nodiscard]] const BrainConfig& config() const noexcept;
    [[nodiscard]] const BrainState& state() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Connection>& connections() const noexcept { return connections_; }
    [[nodiscard]] std::size_t hiddenCount() const noexcept;
    [[nodiscard]] std::size_t enabledConnectionCount() const noexcept;
    [[nodiscard]] std::size_t recurrentConnectionCount() const noexcept;
    [[nodiscard]] const std::unordered_map<std::int32_t, double>& recurrentState() const noexcept { return state_; }
    [[nodiscard]] bool allowsRecurrent() const noexcept { return allowRecurrentEdges_; }
    [[nodiscard]] double memoryDecay() const noexcept { return memoryDecay_; }
    [[nodiscard]] double stateClip() const noexcept { return stateClip_; }
    [[nodiscard]] bool resetStateOnCopy() const noexcept { return resetStateOnCopy_; }
    [[nodiscard]] const std::string& initialTopology() const noexcept { return initialTopology_; }
    [[nodiscard]] double weightInitStd() const noexcept { return weightInitStd_; }
    [[nodiscard]] std::size_t maxHiddenNodes() const noexcept { return maxHiddenNodes_; }
    [[nodiscard]] std::size_t maxConnections() const noexcept { return maxConnections_; }

    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr);
    [[nodiscard]] std::vector<double> forward(const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const;

    [[nodiscard]] NEATGraphBrain clone() const;

    [[nodiscard]] std::string batchKey() const;
    [[nodiscard]] double checksum() const;
    [[nodiscard]] std::size_t parameterCount() const { return connections_.size(); }

    bool mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng);
    void resetState() noexcept;

    friend struct BrainSerializer;  // Phase 28: persistence access.

private:
    void initGraph(std::mt19937_64& rng);
    bool addConnection(std::int32_t src, std::int32_t dst, double weight, bool enabled, bool recurrent);
    bool addConnectionRandom(std::int32_t src, std::int32_t dst, bool recurrent, std::mt19937_64& rng);
    bool connectionExists(std::int32_t src, std::int32_t dst, bool recurrent) const;
    Node* nodeById(std::int32_t id);
    [[nodiscard]] const Node* nodeById(std::int32_t id) const;
    [[nodiscard]] double randomWeight(std::mt19937_64& rng) const;
    [[nodiscard]] double applyActivation(double value, ActivationKind kind) const;
    Node& newNode(NodeKind kind, double layer, ActivationKind act);

    bool addRandomConnection(std::mt19937_64& rng);
    bool addRandomNode(std::mt19937_64& rng);
    bool splitConnection(Connection& conn, std::mt19937_64& rng);
    bool toggleRandomConnection(std::mt19937_64& rng);
    bool removeRandomConnection(std::mt19937_64& rng);
    bool mutateProtozoaStyle(double rate, double strength, std::mt19937_64& rng);

    std::vector<double> computeValues(const std::vector<double>& input,
                                       bool updateState,
                                       ActivationTrace* trace);
    std::vector<double> computeValuesConst(const std::vector<double>& input,
                                            ActivationTrace* trace) const;

    BrainConfig config_{};
    BrainState brainState_{};
    std::vector<std::size_t> layerSizes_;
    BrainType brainType_ = BrainType::Neat;

    std::vector<Node> nodes_;
    std::vector<Connection> connections_;
    mutable std::unordered_map<std::int32_t, double> state_;
    std::int32_t nextNodeId_ = 0;
    std::int32_t nextInnovation_ = 1;

    std::string initialTopology_ = "minimal";
    double weightInitStd_ = 0.6;
    double weightMutationRate_ = -1.0;
    double weightMutationStrength_ = -1.0;
    double addConnectionRate_ = 0.08;
    double addNodeRate_ = 0.03;
    double toggleConnectionRate_ = 0.01;
    double removeConnectionRate_ = 0.0;
    double resetWeightRate_ = 0.02;
    std::size_t maxHiddenNodes_ = 64;
    std::size_t maxConnections_ = 512;
    double recurrentConnectionRate_ = 0.12;
    double memoryDecay_ = 0.85;
    double stateClip_ = 1.0;
    bool resetStateOnCopy_ = true;
    bool allowRecurrentEdges_ = false;
};
} // namespace agentbiosim::neural
