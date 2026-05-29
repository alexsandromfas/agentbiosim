#include "neural/GatedMLPBrain.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace agentbiosim::neural
{
namespace
{
double fanInStd(const double initStd, const std::size_t fanIn)
{
    return std::max(0.0, initStd) / std::sqrt(static_cast<double>(std::max<std::size_t>(1U, fanIn)));
}
} // namespace

GatedMLPBrain::GatedMLPBrain(BrainConfig config, std::mt19937_64& rng)
    : config_(std::move(config)),
      layerSizes_(config_.layerSizes())
{
    initialize(rng);
}

std::size_t GatedMLPBrain::inputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.front();
}

std::size_t GatedMLPBrain::outputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.back();
}

const std::vector<std::size_t>& GatedMLPBrain::layerSizes() const noexcept
{
    return layerSizes_;
}

const BrainConfig& GatedMLPBrain::config() const noexcept
{
    return config_;
}

const BrainState& GatedMLPBrain::state() const noexcept
{
    return state_;
}

std::uint64_t GatedMLPBrain::revision() const noexcept
{
    return state_.revision;
}

void GatedMLPBrain::initialize(std::mt19937_64& rng)
{
    weights_.clear();
    biases_.clear();
    gates_.clear();
    if (layerSizes_.size() < 2U)
    {
        layerSizes_ = {std::max<std::size_t>(1U, config_.inputSize),
                       std::max<std::size_t>(1U, config_.outputSize)};
    }

    weights_.reserve(layerSizes_.size() - 1U);
    biases_.reserve(layerSizes_.size() - 1U);
    for (std::size_t layer = 1; layer < layerSizes_.size(); ++layer)
    {
        const std::size_t rows = layerSizes_[layer];
        const std::size_t cols = layerSizes_[layer - 1U];
        const double stddev = fanInStd(config_.initStd, cols);
        std::normal_distribution<double> weightDistribution(0.0, stddev);
        std::normal_distribution<double> biasDistribution(0.0, stddev * 0.5);

        std::vector<double> weights(rows * cols, 0.0);
        for (double& v : weights)
        {
            v = weightDistribution(rng);
        }
        weights_.push_back(std::move(weights));

        std::vector<double> biases(rows, 0.0);
        if (config_.randomBiases)
        {
            for (double& v : biases)
            {
                v = biasDistribution(rng);
            }
        }
        biases_.push_back(std::move(biases));
    }

    // Gates: one vector per hidden layer (layerSizes_[1 .. -2]).
    if (layerSizes_.size() > 2U)
    {
        gates_.reserve(layerSizes_.size() - 2U);
        for (std::size_t i = 1; i + 1 < layerSizes_.size(); ++i)
        {
            std::vector<double> g(layerSizes_[i], config_.future.gateInit);
            gates_.push_back(std::move(g));
        }
    }
    clampGates();
    state_.revision = 0;
}

void GatedMLPBrain::clampGates()
{
    const double lo = config_.future.gateMin;
    const double hi = std::max(lo, config_.future.gateMax);
    for (auto& g : gates_)
    {
        for (double& v : g)
        {
            v = std::clamp(v, lo, hi);
        }
    }
}

std::vector<double> GatedMLPBrain::forward(const std::vector<double>& input,
                                            ActivationTrace* trace) const
{
    if (trace != nullptr)
    {
        trace->clear();
        trace->brainType = BrainType::GatedMlp;
    }

    std::vector<double> activations(inputSize(), 0.0);
    const std::size_t copied = std::min(inputSize(), input.size());
    std::copy_n(input.begin(), copied, activations.begin());

    for (std::size_t layer = 0; layer < weights_.size(); ++layer)
    {
        const std::size_t rows = layerSizes_[layer + 1U];
        const std::size_t cols = layerSizes_[layer];
        std::vector<double> next(rows, 0.0);
        for (std::size_t row = 0; row < rows; ++row)
        {
            double sum = biases_[layer][row];
            for (std::size_t col = 0; col < cols; ++col)
            {
                sum += weights_[layer][row * cols + col] * activations[col];
            }
            if (layer + 1U < weights_.size())
            {
                sum = std::tanh(sum);
            }
            next[row] = sum;
        }

        // Apply gates element-wise on hidden activations.
        if (layer + 1U < weights_.size() && layer < gates_.size())
        {
            const auto& g = gates_[layer];
            const std::size_t lim = std::min(next.size(), g.size());
            for (std::size_t i = 0; i < lim; ++i)
            {
                next[i] *= g[i];
            }
            if (trace != nullptr)
            {
                trace->gateValues.push_back(g);
            }
        }

        if (trace != nullptr)
        {
            ActivationLayer l;
            l.outputLayer = layer + 1U == weights_.size();
            l.name = l.outputLayer ? "output" : "hidden_" + std::to_string(layer + 1U);
            l.values = next;
            trace->layers.push_back(std::move(l));
        }

        activations = std::move(next);
    }
    return activations;
}

GatedMLPBrain GatedMLPBrain::clone() const
{
    return *this;
}

std::string GatedMLPBrain::batchKey() const
{
    std::ostringstream out;
    out << "gated_mlp:";
    for (const std::size_t s : layerSizes_)
    {
        out << s << ',';
    }
    out << ':' << config_.future.gateMin << ':' << config_.future.gateMax;
    return out.str();
}

double GatedMLPBrain::checksum() const
{
    double total = 0.0;
    double scale = 1.0;
    for (const auto& layer : weights_)
    {
        for (const double v : layer)
        {
            total += v * scale;
            scale += 0.000001;
        }
    }
    for (const auto& layer : biases_)
    {
        for (const double v : layer)
        {
            total += v * scale;
            scale += 0.000001;
        }
    }
    for (const auto& g : gates_)
    {
        for (const double v : g)
        {
            total += v * scale;
            scale += 0.000001;
        }
    }
    return total;
}

std::size_t GatedMLPBrain::parameterCount() const
{
    std::size_t total = 0;
    for (const auto& l : weights_) total += l.size();
    for (const auto& l : biases_) total += l.size();
    for (const auto& g : gates_) total += g.size();
    return total;
}

bool GatedMLPBrain::mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng)
{
    const double baseRate = std::clamp(mutCfg.baseRate, 0.0, 1.0);
    const double baseStr = std::max(0.0, mutCfg.baseStrength);
    bool changed = false;

    // Mutate weights and biases with base rules.
    if (baseRate > 0.0 && baseStr > 0.0)
    {
        std::bernoulli_distribution bm(baseRate);
        std::normal_distribution<double> noise(0.0, baseStr);
        for (auto& layer : weights_)
        {
            for (double& v : layer)
            {
                if (bm(rng))
                {
                    v += noise(rng);
                    changed = true;
                }
            }
        }
        for (auto& layer : biases_)
        {
            for (double& v : layer)
            {
                if (bm(rng))
                {
                    v += noise(rng);
                    changed = true;
                }
            }
        }
    }

    // Mutate gates with gate-specific rules (fallback to base when -1).
    const double gateRate = std::clamp(mutCfg.effectiveGateRate(), 0.0, 1.0);
    const double gateStr = std::max(0.0, mutCfg.effectiveGateStrength());
    if (gateRate > 0.0 && gateStr > 0.0)
    {
        std::bernoulli_distribution gm(gateRate);
        std::normal_distribution<double> gnoise(0.0, gateStr);
        for (auto& g : gates_)
        {
            for (double& v : g)
            {
                if (gm(rng))
                {
                    v += gnoise(rng);
                    changed = true;
                }
            }
        }
    }

    if (changed)
    {
        clampGates();
        ++state_.revision;
    }
    return changed;
}
} // namespace agentbiosim::neural
