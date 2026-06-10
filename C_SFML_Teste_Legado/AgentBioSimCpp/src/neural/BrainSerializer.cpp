#include "neural/BrainSerializer.hpp"

#include "neural/BrainFactory.hpp"

#include <random>
#include <type_traits>

namespace agentbiosim::neural
{
BrainSnapshot BrainSerializer::capture(const BrainVariant& brain)
{
    BrainSnapshot snapshot;
    std::visit(
        [&](const auto& b) {
            using T = std::decay_t<decltype(b)>;
            snapshot.config = b.config();
            if constexpr (std::is_same_v<T, MLPBrain>)
            {
                snapshot.layerSizes = b.layerSizes();
                snapshot.weights = b.weights();
                snapshot.biases = b.biases();
            }
            else if constexpr (std::is_same_v<T, GatedMLPBrain>)
            {
                snapshot.layerSizes = b.layerSizes();
                snapshot.weights = b.weights();
                snapshot.biases = b.biases();
                snapshot.gates = b.gates();
            }
            else if constexpr (std::is_same_v<T, ShortcutMLPBrain>)
            {
                snapshot.layerSizes = b.layerSizes();
                snapshot.weights = b.weights();
                snapshot.biases = b.biases();
                snapshot.shortcutWeights = b.shortcutWeights();
                snapshot.shortcutBias = b.shortcutBias();
            }
            else if constexpr (std::is_same_v<T, ModulatedMLPBrain>)
            {
                snapshot.layerSizes = b.layerSizes();
                snapshot.weights = b.weights();
                snapshot.biases = b.biases();
                snapshot.gates = b.gates();
                snapshot.shortcutWeights = b.shortcutWeights();
                snapshot.shortcutBias = b.shortcutBias_;  // friend access (no public getter)
            }
            else if constexpr (std::is_same_v<T, SimpleRNNBrain>)
            {
                snapshot.layerSizes = b.layerSizes();
                snapshot.weights = b.weights();
                snapshot.biases = b.biases();
                snapshot.recurrentWeights = b.recurrentWeights();
                snapshot.recurrentState = b.recurrentState();
            }
            else if constexpr (std::is_same_v<T, NEATGraphBrain>)
            {
                for (const auto& n : b.nodes())
                {
                    snapshot.neatNodes.push_back(
                        {n.id, static_cast<int>(n.kind), n.layer, static_cast<int>(n.activation)});
                }
                for (const auto& c : b.connections())
                {
                    snapshot.neatConnections.push_back(
                        {c.src, c.dst, c.weight, c.enabled, c.recurrent, c.innovation});
                }
                for (const auto& kv : b.recurrentState())
                {
                    snapshot.neatState.emplace_back(kv.first, kv.second);
                }
                snapshot.neatNextNodeId = b.nextNodeId_;       // friend access
                snapshot.neatNextInnovation = b.nextInnovation_;
            }
        },
        brain);
    return snapshot;
}

BrainVariant BrainSerializer::build(const BrainSnapshot& snapshot)
{
    // Rebuild via the factory so every config-derived field + topology is set
    // exactly as at construction; then overwrite the learned parameters. The
    // dummy rng only affects the discarded random init.
    std::mt19937_64 dummy(0x9E3779B97F4A7C15ULL);
    BrainCreationResult created = BrainFactory::createBrain(snapshot.config, dummy);
    BrainVariant brain = std::move(created.brain);

    std::visit(
        [&](auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, MLPBrain>)
            {
                if (!snapshot.layerSizes.empty()) b.layerSizes_ = snapshot.layerSizes;
                b.weights_ = snapshot.weights;
                b.biases_ = snapshot.biases;
            }
            else if constexpr (std::is_same_v<T, GatedMLPBrain>)
            {
                if (!snapshot.layerSizes.empty()) b.layerSizes_ = snapshot.layerSizes;
                b.weights_ = snapshot.weights;
                b.biases_ = snapshot.biases;
                b.gates_ = snapshot.gates;
            }
            else if constexpr (std::is_same_v<T, ShortcutMLPBrain>)
            {
                if (!snapshot.layerSizes.empty()) b.layerSizes_ = snapshot.layerSizes;
                b.weights_ = snapshot.weights;
                b.biases_ = snapshot.biases;
                b.shortcutWeights_ = snapshot.shortcutWeights;
                b.shortcutBias_ = snapshot.shortcutBias;
            }
            else if constexpr (std::is_same_v<T, ModulatedMLPBrain>)
            {
                if (!snapshot.layerSizes.empty()) b.layerSizes_ = snapshot.layerSizes;
                b.weights_ = snapshot.weights;
                b.biases_ = snapshot.biases;
                b.gates_ = snapshot.gates;
                b.shortcutWeights_ = snapshot.shortcutWeights;
                b.shortcutBias_ = snapshot.shortcutBias;
            }
            else if constexpr (std::is_same_v<T, SimpleRNNBrain>)
            {
                if (!snapshot.layerSizes.empty()) b.layerSizes_ = snapshot.layerSizes;
                b.weights_ = snapshot.weights;
                b.biases_ = snapshot.biases;
                b.recurrentWeights_ = snapshot.recurrentWeights;
                b.state_ = snapshot.recurrentState;
            }
            else if constexpr (std::is_same_v<T, NEATGraphBrain>)
            {
                b.nodes_.clear();
                b.nodes_.reserve(snapshot.neatNodes.size());
                for (const auto& d : snapshot.neatNodes)
                {
                    NEATGraphBrain::Node n;
                    n.id = d.id;
                    n.kind = static_cast<NEATGraphBrain::NodeKind>(d.kind);
                    n.layer = d.layer;
                    n.activation = static_cast<NEATGraphBrain::ActivationKind>(d.activation);
                    b.nodes_.push_back(n);
                }
                b.connections_.clear();
                b.connections_.reserve(snapshot.neatConnections.size());
                for (const auto& d : snapshot.neatConnections)
                {
                    NEATGraphBrain::Connection c;
                    c.src = d.src;
                    c.dst = d.dst;
                    c.weight = d.weight;
                    c.enabled = d.enabled;
                    c.recurrent = d.recurrent;
                    c.innovation = d.innovation;
                    b.connections_.push_back(c);
                }
                b.state_.clear();
                for (const auto& kv : snapshot.neatState)
                {
                    b.state_[kv.first] = kv.second;
                }
                b.nextNodeId_ = snapshot.neatNextNodeId;
                b.nextInnovation_ = snapshot.neatNextInnovation;
            }
        },
        brain);

    return brain;
}

std::string BrainSerializer::signatureOf(const BrainSnapshot& snapshot)
{
    return snapshot.config.architectureSignature();
}
} // namespace agentbiosim::neural
