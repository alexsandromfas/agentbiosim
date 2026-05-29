#include "neural/NEATGraphBrain.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace agentbiosim::neural
{
namespace
{
constexpr double kSigmoidClip = 60.0;
}

NEATGraphBrain::NEATGraphBrain(BrainConfig config, std::mt19937_64& rng)
    : config_(std::move(config))
{
    layerSizes_ = config_.layerSizes();
    brainType_ = config_.type;
    if (brainType_ != BrainType::Neat && brainType_ != BrainType::SimpleNeat &&
        brainType_ != BrainType::RecurrentNeat)
    {
        brainType_ = BrainType::Neat;
    }

    initialTopology_ = config_.neat.initialTopology == "layered" ? "layered" : "minimal";
    weightInitStd_ = std::max(0.0, config_.neat.weightInitStd);
    weightMutationRate_ = config_.neat.weightMutationRate;
    weightMutationStrength_ = config_.neat.weightMutationStrength;
    addConnectionRate_ = std::clamp(config_.neat.addConnectionRate, 0.0, 1.0);
    addNodeRate_ = std::clamp(config_.neat.addNodeRate, 0.0, 1.0);
    toggleConnectionRate_ = std::clamp(config_.neat.toggleConnectionRate, 0.0, 1.0);
    removeConnectionRate_ = std::clamp(config_.neat.removeConnectionRate, 0.0, 1.0);
    resetWeightRate_ = std::clamp(config_.neat.resetWeightRate, 0.0, 1.0);
    maxHiddenNodes_ = static_cast<std::size_t>(std::max(0, config_.neat.maxHiddenNodes));
    maxConnections_ = static_cast<std::size_t>(std::max(1, config_.neat.maxConnections));
    recurrentConnectionRate_ = std::clamp(config_.neat.recurrentConnectionRate, 0.0, 1.0);
    memoryDecay_ = std::clamp(config_.neat.memoryDecay, 0.0, 0.999);
    stateClip_ = std::max(0.01, config_.neat.stateClip);
    resetStateOnCopy_ = config_.neat.resetStateOnCopy;
    allowRecurrentEdges_ = brainType_ == BrainType::RecurrentNeat;

    initGraph(rng);
}

std::size_t NEATGraphBrain::inputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.front();
}

std::size_t NEATGraphBrain::outputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.back();
}

const std::vector<std::size_t>& NEATGraphBrain::layerSizes() const noexcept
{
    return layerSizes_;
}

const BrainConfig& NEATGraphBrain::config() const noexcept
{
    return config_;
}

const BrainState& NEATGraphBrain::state() const noexcept
{
    return brainState_;
}

std::uint64_t NEATGraphBrain::revision() const noexcept
{
    return brainState_.revision;
}

std::size_t NEATGraphBrain::hiddenCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& n : nodes_)
    {
        if (n.kind == NodeKind::Hidden) ++count;
    }
    return count;
}

std::size_t NEATGraphBrain::enabledConnectionCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& c : connections_)
    {
        if (c.enabled) ++count;
    }
    return count;
}

std::size_t NEATGraphBrain::recurrentConnectionCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& c : connections_)
    {
        if (c.recurrent) ++count;
    }
    return count;
}

double NEATGraphBrain::randomWeight(std::mt19937_64& rng) const
{
    std::normal_distribution<double> dist(0.0, weightInitStd_);
    return dist(rng);
}

double NEATGraphBrain::applyActivation(const double value, const ActivationKind kind) const
{
    switch (kind)
    {
    case ActivationKind::Linear:
        return value;
    case ActivationKind::Sigmoid:
    {
        const double clamped = std::clamp(value, -kSigmoidClip, kSigmoidClip);
        return 1.0 / (1.0 + std::exp(-clamped));
    }
    case ActivationKind::Tanh:
    default:
        return std::tanh(value);
    }
}

NEATGraphBrain::Node& NEATGraphBrain::newNode(const NodeKind kind, const double layer, const ActivationKind act)
{
    Node n;
    n.id = nextNodeId_++;
    n.kind = kind;
    n.layer = layer;
    n.activation = act;
    nodes_.push_back(n);
    if (kind != NodeKind::Input)
    {
        state_[n.id] = 0.0;
    }
    return nodes_.back();
}

NEATGraphBrain::Node* NEATGraphBrain::nodeById(const std::int32_t id)
{
    for (auto& n : nodes_)
    {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const NEATGraphBrain::Node* NEATGraphBrain::nodeById(const std::int32_t id) const
{
    for (const auto& n : nodes_)
    {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool NEATGraphBrain::connectionExists(const std::int32_t src, const std::int32_t dst, const bool recurrent) const
{
    for (const auto& c : connections_)
    {
        if (c.src == src && c.dst == dst && c.recurrent == recurrent) return true;
    }
    return false;
}

bool NEATGraphBrain::addConnection(const std::int32_t src, const std::int32_t dst, const double weight,
                                    const bool enabled, const bool recurrent)
{
    if (connections_.size() >= maxConnections_) return false;
    const Node* s = nodeById(src);
    const Node* d = nodeById(dst);
    if (s == nullptr || d == nullptr || d->kind == NodeKind::Input) return false;
    const bool rec = recurrent && allowRecurrentEdges_;
    if (!rec && s->layer >= d->layer) return false;
    if (connectionExists(src, dst, rec)) return false;
    Connection c;
    c.src = src;
    c.dst = dst;
    c.weight = weight;
    c.enabled = enabled;
    c.recurrent = rec;
    c.innovation = nextInnovation_++;
    connections_.push_back(c);
    return true;
}

bool NEATGraphBrain::addConnectionRandom(const std::int32_t src, const std::int32_t dst,
                                          const bool recurrent, std::mt19937_64& rng)
{
    return addConnection(src, dst, randomWeight(rng), true, recurrent);
}

void NEATGraphBrain::initGraph(std::mt19937_64& rng)
{
    nodes_.clear();
    connections_.clear();
    state_.clear();
    nextNodeId_ = 0;
    nextInnovation_ = 1;

    const std::size_t inSize = std::max<std::size_t>(1U, layerSizes_.empty() ? 1U : layerSizes_.front());
    const std::size_t outSize = std::max<std::size_t>(1U, layerSizes_.empty() ? 1U : layerSizes_.back());

    for (std::size_t i = 0; i < inSize; ++i)
    {
        static_cast<void>(newNode(NodeKind::Input, 0.0, ActivationKind::Linear));
    }

    const bool layered = initialTopology_ == "layered" && layerSizes_.size() > 2U;
    if (layered)
    {
        std::vector<std::size_t> hiddenLayers(layerSizes_.begin() + 1, layerSizes_.end() - 1);
        const std::size_t denom = std::max<std::size_t>(1U, hiddenLayers.size() + 1U);
        for (std::size_t idx = 0; idx < hiddenLayers.size(); ++idx)
        {
            const double layer = static_cast<double>(idx + 1) / static_cast<double>(denom);
            for (std::size_t k = 0; k < hiddenLayers[idx]; ++k)
            {
                static_cast<void>(newNode(NodeKind::Hidden, layer, ActivationKind::Tanh));
            }
        }
    }

    const std::size_t outputStartIndex = nodes_.size();
    for (std::size_t i = 0; i < outSize; ++i)
    {
        static_cast<void>(newNode(NodeKind::Output, 1.0, ActivationKind::Linear));
    }

    if (layered)
    {
        std::vector<std::vector<std::int32_t>> layers;
        std::vector<std::int32_t> inputIds;
        for (const auto& n : nodes_) if (n.kind == NodeKind::Input) inputIds.push_back(n.id);
        layers.push_back(inputIds);

        std::map<double, std::vector<std::int32_t>> hiddenByLayer;
        for (const auto& n : nodes_)
        {
            if (n.kind == NodeKind::Hidden) hiddenByLayer[n.layer].push_back(n.id);
        }
        for (auto& kv : hiddenByLayer) layers.push_back(kv.second);

        std::vector<std::int32_t> outIds;
        for (const auto& n : nodes_) if (n.kind == NodeKind::Output) outIds.push_back(n.id);
        layers.push_back(outIds);

        for (std::size_t li = 0; li + 1 < layers.size(); ++li)
        {
            for (const auto srcId : layers[li])
            {
                for (const auto dstId : layers[li + 1])
                {
                    static_cast<void>(addConnectionRandom(srcId, dstId, false, rng));
                }
            }
        }
    }
    else
    {
        for (std::size_t i = 0; i < inSize; ++i)
        {
            for (std::size_t j = 0; j < outSize; ++j)
            {
                const std::int32_t srcId = nodes_[i].id;
                const std::int32_t dstId = nodes_[outputStartIndex + j].id;
                static_cast<void>(addConnectionRandom(srcId, dstId, false, rng));
            }
        }
    }

    brainState_.revision = 0;
}

std::vector<double> NEATGraphBrain::computeValues(const std::vector<double>& input,
                                                    const bool updateState,
                                                    ActivationTrace* trace)
{
    std::unordered_map<std::int32_t, double> values;
    values.reserve(nodes_.size());

    std::vector<const Node*> inputs;
    std::vector<const Node*> outputs;
    std::map<double, std::vector<const Node*>> hiddenByLayer;
    for (const auto& n : nodes_)
    {
        if (n.kind == NodeKind::Input) inputs.push_back(&n);
        else if (n.kind == NodeKind::Output) outputs.push_back(&n);
        else hiddenByLayer[n.layer].push_back(&n);
    }
    std::sort(inputs.begin(), inputs.end(), [](const Node* a, const Node* b) { return a->id < b->id; });
    std::sort(outputs.begin(), outputs.end(), [](const Node* a, const Node* b) { return a->id < b->id; });
    for (auto& kv : hiddenByLayer)
    {
        std::sort(kv.second.begin(), kv.second.end(), [](const Node* a, const Node* b) { return a->id < b->id; });
    }

    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        values[inputs[i]->id] = i < input.size() ? input[i] : 0.0;
    }

    std::unordered_map<std::int32_t, std::vector<const Connection*>> incoming;
    for (const auto& c : connections_)
    {
        if (c.enabled) incoming[c.dst].push_back(&c);
    }

    auto sumNodeInputs = [&](const Node* node) -> double {
        double total = 0.0;
        const auto it = incoming.find(node->id);
        if (it == incoming.end()) return 0.0;
        for (const Connection* c : it->second)
        {
            double srcValue = 0.0;
            if (c->recurrent)
            {
                const auto sit = state_.find(c->src);
                srcValue = sit != state_.end() ? sit->second : 0.0;
            }
            else
            {
                const auto vit = values.find(c->src);
                srcValue = vit != values.end() ? vit->second : 0.0;
            }
            total += srcValue * c->weight;
        }
        return total;
    };

    auto blendState = [&](const std::int32_t nodeId, const double value) -> double {
        if (!allowRecurrentEdges_) return value;
        const auto sit = state_.find(nodeId);
        const double old = sit != state_.end() ? sit->second : 0.0;
        return memoryDecay_ * old + (1.0 - memoryDecay_) * value;
    };

    std::unordered_map<std::int32_t, double> newState = state_;

    for (auto& kv : hiddenByLayer)
    {
        for (const Node* node : kv.second)
        {
            const double sum = sumNodeInputs(node);
            const double out = applyActivation(sum, node->activation);
            values[node->id] = out;
            newState[node->id] = blendState(node->id, out);
        }
    }

    std::vector<double> outputValues;
    outputValues.reserve(outputs.size());
    for (const Node* node : outputs)
    {
        const double sum = sumNodeInputs(node);
        const double out = applyActivation(sum, node->activation);
        values[node->id] = out;
        newState[node->id] = blendState(node->id, out);
        outputValues.push_back(out);
    }

    if (updateState && allowRecurrentEdges_)
    {
        for (auto& kv : newState)
        {
            kv.second = std::clamp(kv.second, -stateClip_, stateClip_);
        }
        state_ = std::move(newState);
    }

    if (trace != nullptr)
    {
        trace->clear();
        trace->brainType = brainType_;
        trace->recurrentMemoryDecay = memoryDecay_;
        trace->recurrentStateClip = stateClip_;
        trace->neatConnectionCount = connections_.size();
        trace->neatEnabledConnectionCount = enabledConnectionCount();
        trace->neatRecurrentConnectionCount = recurrentConnectionCount();
        for (const auto& n : nodes_)
        {
            NeatNodeActivation entry;
            entry.id = n.id;
            entry.kind = static_cast<int>(n.kind);
            entry.layer = n.layer;
            const auto vit = values.find(n.id);
            entry.value = vit != values.end() ? vit->second : 0.0;
            trace->neatNodes.push_back(entry);
        }
        // Also produce a single "output" activation layer for visualization compatibility.
        ActivationLayer layer;
        layer.name = "output";
        layer.values = outputValues;
        layer.outputLayer = true;
        trace->layers.push_back(std::move(layer));
    }

    return outputValues;
}

std::vector<double> NEATGraphBrain::computeValuesConst(const std::vector<double>& input,
                                                        ActivationTrace* trace) const
{
    NEATGraphBrain temp = *this;
    return temp.computeValues(input, /*updateState=*/false, trace);
}

std::vector<double> NEATGraphBrain::forward(const std::vector<double>& input, ActivationTrace* trace)
{
    return computeValues(input, /*updateState=*/true, trace);
}

std::vector<double> NEATGraphBrain::forward(const std::vector<double>& input, ActivationTrace* trace) const
{
    return computeValuesConst(input, trace);
}

NEATGraphBrain NEATGraphBrain::clone() const
{
    NEATGraphBrain c = *this;
    if (resetStateOnCopy_)
    {
        for (auto& kv : c.state_) kv.second = 0.0;
    }
    return c;
}

std::string NEATGraphBrain::batchKey() const
{
    std::ostringstream out;
    out << brainTypeName(brainType_) << ":individual:";
    out << reinterpret_cast<std::uintptr_t>(this) << ':' << brainState_.revision;
    return out.str();
}

double NEATGraphBrain::checksum() const
{
    double total = 0.0;
    double scale = 1.0;
    for (const auto& c : connections_)
    {
        if (c.enabled)
        {
            total += c.weight * scale;
            scale += 0.000001;
        }
    }
    total += static_cast<double>(nodes_.size()) * 0.0001;
    total += static_cast<double>(connections_.size()) * 0.00001;
    return total;
}

bool NEATGraphBrain::addRandomConnection(std::mt19937_64& rng)
{
    if (connections_.size() >= maxConnections_) return false;
    if (nodes_.size() < 2) return false;
    std::vector<const Node*> targets;
    for (const auto& n : nodes_) if (n.kind != NodeKind::Input) targets.push_back(&n);
    if (targets.empty()) return false;

    std::uniform_int_distribution<std::size_t> nodePick(0, nodes_.size() - 1);
    std::uniform_int_distribution<std::size_t> tgtPick(0, targets.size() - 1);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (int attempt = 0; attempt < 80; ++attempt)
    {
        const Node* dst = targets[tgtPick(rng)];
        const Node* src = &nodes_[nodePick(rng)];
        if (src->id == dst->id) continue;
        bool recurrent = false;
        if (allowRecurrentEdges_ && uni(rng) < recurrentConnectionRate_)
        {
            recurrent = true;
        }
        else if (src->layer >= dst->layer)
        {
            continue;
        }
        if (addConnectionRandom(src->id, dst->id, recurrent, rng)) return true;
    }
    return false;
}

bool NEATGraphBrain::splitConnection(Connection& conn, std::mt19937_64& rng)
{
    const Node* src = nodeById(conn.src);
    const Node* dst = nodeById(conn.dst);
    if (src == nullptr || dst == nullptr) return false;
    if (hiddenCount() >= maxHiddenNodes_) return false;
    if (src->layer >= dst->layer) return false;
    const double newLayer = (src->layer + dst->layer) * 0.5;
    conn.enabled = false;
    const double oldWeight = conn.weight;
    const std::int32_t srcId = conn.src;
    const std::int32_t dstId = conn.dst;
    Node& nn = newNode(NodeKind::Hidden, newLayer, ActivationKind::Tanh);
    const bool ok1 = addConnection(srcId, nn.id, 1.0, true, false);
    const bool ok2 = addConnection(nn.id, dstId, oldWeight, true, false);
    static_cast<void>(rng);
    return ok1 || ok2;
}

bool NEATGraphBrain::addRandomNode(std::mt19937_64& rng)
{
    if (hiddenCount() >= maxHiddenNodes_) return false;
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < connections_.size(); ++i)
    {
        if (connections_[i].enabled && !connections_[i].recurrent) candidates.push_back(i);
    }
    if (candidates.empty()) return false;
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    Connection& chosen = connections_[candidates[pick(rng)]];
    return splitConnection(chosen, rng);
}

bool NEATGraphBrain::toggleRandomConnection(std::mt19937_64& rng)
{
    if (connections_.empty()) return false;
    std::uniform_int_distribution<std::size_t> pick(0, connections_.size() - 1);
    Connection& c = connections_[pick(rng)];
    c.enabled = !c.enabled;
    return true;
}

bool NEATGraphBrain::removeRandomConnection(std::mt19937_64& rng)
{
    if (connections_.empty()) return false;
    std::uniform_int_distribution<std::size_t> pick(0, connections_.size() - 1);
    connections_.erase(connections_.begin() + static_cast<std::ptrdiff_t>(pick(rng)));
    return true;
}

bool NEATGraphBrain::mutateProtozoaStyle(const double rate, const double strength, std::mt19937_64& rng)
{
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    if (uni(rng) > std::clamp(rate, 0.0, 1.0)) return false;
    std::vector<std::size_t> enabledIdx;
    for (std::size_t i = 0; i < connections_.size(); ++i)
    {
        if (connections_[i].enabled && !connections_[i].recurrent) enabledIdx.push_back(i);
    }
    if (enabledIdx.empty())
    {
        return addRandomConnection(rng);
    }
    std::uniform_int_distribution<std::size_t> pick(0, enabledIdx.size() - 1);
    Connection& chosen = connections_[enabledIdx[pick(rng)]];
    if (uni(rng) < addNodeRate_)
    {
        return splitConnection(chosen, rng);
    }
    if (uni(rng) < 0.5)
    {
        chosen.weight = randomWeight(rng);
    }
    else
    {
        std::normal_distribution<double> noise(0.0, std::max(0.0, strength));
        chosen.weight += noise(rng);
    }
    return true;
}

bool NEATGraphBrain::mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng)
{
    const double baseRate = std::clamp(mutCfg.baseRate, 0.0, 1.0);
    const double baseStrength = std::max(0.0, mutCfg.baseStrength);
    const double weightRate = weightMutationRate_ < 0.0 ? baseRate : std::clamp(weightMutationRate_, 0.0, 1.0);
    const double weightStrength = weightMutationStrength_ < 0.0 ? baseStrength : std::max(0.0, weightMutationStrength_);

    bool changed = false;
    if (weightRate > 0.0 && weightStrength > 0.0)
    {
        std::bernoulli_distribution wm(weightRate);
        std::normal_distribution<double> noise(0.0, weightStrength);
        for (auto& c : connections_)
        {
            if (wm(rng))
            {
                c.weight += noise(rng);
                changed = true;
            }
        }
    }
    if (resetWeightRate_ > 0.0)
    {
        std::bernoulli_distribution rm(resetWeightRate_);
        for (auto& c : connections_)
        {
            if (rm(rng))
            {
                c.weight = randomWeight(rng);
                changed = true;
            }
        }
    }

    std::uniform_real_distribution<double> uni(0.0, 1.0);
    if (brainType_ == BrainType::SimpleNeat)
    {
        if (mutateProtozoaStyle(baseRate, baseStrength, rng)) changed = true;
    }
    else
    {
        if (uni(rng) < addConnectionRate_)
        {
            if (addRandomConnection(rng)) changed = true;
        }
        if (uni(rng) < addNodeRate_)
        {
            if (addRandomNode(rng)) changed = true;
        }
        if (uni(rng) < toggleConnectionRate_)
        {
            if (toggleRandomConnection(rng)) changed = true;
        }
        if (uni(rng) < removeConnectionRate_)
        {
            if (removeRandomConnection(rng)) changed = true;
        }
    }

    if (changed)
    {
        ++brainState_.revision;
    }
    return changed;
}

void NEATGraphBrain::resetState() noexcept
{
    for (auto& kv : state_) kv.second = 0.0;
}
} // namespace agentbiosim::neural
