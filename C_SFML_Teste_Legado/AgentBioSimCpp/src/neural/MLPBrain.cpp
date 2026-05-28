#include "neural/MLPBrain.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>
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

MLPBrain::MLPBrain(BrainConfig config, std::mt19937_64& rng)
    : config_(std::move(config)),
      layerSizes_(config_.layerSizes())
{
    initialize(rng);
}

std::size_t MLPBrain::inputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.front();
}

std::size_t MLPBrain::outputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.back();
}

const std::vector<std::size_t>& MLPBrain::layerSizes() const noexcept
{
    return layerSizes_;
}

const BrainConfig& MLPBrain::config() const noexcept
{
    return config_;
}

const BrainState& MLPBrain::state() const noexcept
{
    return state_;
}

std::uint64_t MLPBrain::revision() const noexcept
{
    return state_.revision;
}

void MLPBrain::initialize(std::mt19937_64& rng)
{
    weights_.clear();
    biases_.clear();
    if (layerSizes_.size() < 2U)
    {
        layerSizes_ = {std::max<std::size_t>(1U, config_.inputSize), std::max<std::size_t>(1U, config_.outputSize)};
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
        for (double& value : weights)
        {
            value = weightDistribution(rng);
        }
        weights_.push_back(std::move(weights));

        std::vector<double> biases(rows, 0.0);
        if (config_.randomBiases)
        {
            for (double& value : biases)
            {
                value = biasDistribution(rng);
            }
        }
        biases_.push_back(std::move(biases));
    }
    state_.revision = 0;
}

std::vector<double> MLPBrain::normalizedInput(const std::vector<double>& input, const std::size_t expectedSize)
{
    std::vector<double> normalized(expectedSize, 0.0);
    const std::size_t copied = std::min(expectedSize, input.size());
    std::copy_n(input.begin(), copied, normalized.begin());
    return normalized;
}

std::vector<double> MLPBrain::forward(const std::vector<double>& input, ActivationTrace* trace) const
{
    if (trace != nullptr)
    {
        trace->clear();
    }

    std::vector<double> activations = normalizedInput(input, inputSize());
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

        if (trace != nullptr)
        {
            ActivationLayer traceLayer;
            traceLayer.outputLayer = layer + 1U == weights_.size();
            traceLayer.name = traceLayer.outputLayer ? "output" : "hidden_" + std::to_string(layer + 1U);
            traceLayer.values = next;
            trace->layers.push_back(std::move(traceLayer));
        }

        activations = std::move(next);
    }
    return activations;
}

MLPBrain MLPBrain::clone() const
{
    return *this;
}

std::string MLPBrain::batchKey() const
{
    std::ostringstream out;
    out << "mlp:";
    for (const std::size_t size : layerSizes_)
    {
        out << size << ',';
    }
    return out.str();
}

double MLPBrain::checksum() const
{
    double total = 0.0;
    double scale = 1.0;
    for (const auto& layer : weights_)
    {
        for (const double value : layer)
        {
            total += value * scale;
            scale += 0.000001;
        }
    }
    for (const auto& layer : biases_)
    {
        for (const double value : layer)
        {
            total += value * scale;
            scale += 0.000001;
        }
    }
    return total;
}

std::size_t MLPBrain::parameterCount() const
{
    std::size_t total = 0;
    for (const auto& layer : weights_)
    {
        total += layer.size();
    }
    for (const auto& layer : biases_)
    {
        total += layer.size();
    }
    return total;
}

bool MLPBrain::mutate(const double rate, const double strength, std::mt19937_64& rng)
{
    const double safeRate = std::clamp(rate, 0.0, 1.0);
    const double safeStrength = std::max(0.0, strength);
    if (safeRate <= 0.0 || safeStrength <= 0.0)
    {
        return false;
    }

    std::bernoulli_distribution shouldMutate(safeRate);
    std::normal_distribution<double> noise(0.0, safeStrength);
    bool changed = false;
    for (auto& layer : weights_)
    {
        for (double& value : layer)
        {
            if (shouldMutate(rng))
            {
                value += noise(rng);
                changed = true;
            }
        }
    }
    for (auto& layer : biases_)
    {
        for (double& value : layer)
        {
            if (shouldMutate(rng))
            {
                value += noise(rng);
                changed = true;
            }
        }
    }
    if (changed)
    {
        ++state_.revision;
    }
    return changed;
}

bool MLPBrain::resizeInput(const std::size_t newInputSize, std::mt19937_64& rng)
{
    if (newInputSize == 0U || newInputSize == inputSize() || weights_.empty())
    {
        return false;
    }

    const std::size_t oldInputSize = inputSize();
    const std::size_t firstRows = layerSizes_[1U];
    const std::vector<double> oldWeights = weights_[0];
    std::vector<double> resized(firstRows * newInputSize, 0.0);
    const std::size_t copiedColumns = std::min(oldInputSize, newInputSize);
    for (std::size_t row = 0; row < firstRows; ++row)
    {
        for (std::size_t col = 0; col < copiedColumns; ++col)
        {
            resized[row * newInputSize + col] = oldWeights[row * oldInputSize + col];
        }
    }

    if (newInputSize > oldInputSize)
    {
        const double stddev = 0.1 / std::sqrt(static_cast<double>(std::max<std::size_t>(1U, newInputSize)));
        std::normal_distribution<double> extraWeightDistribution(0.0, stddev);
        for (std::size_t row = 0; row < firstRows; ++row)
        {
            for (std::size_t col = oldInputSize; col < newInputSize; ++col)
            {
                resized[row * newInputSize + col] = extraWeightDistribution(rng);
            }
        }
    }

    layerSizes_[0] = newInputSize;
    config_.inputSize = newInputSize;
    weights_[0] = std::move(resized);
    ++state_.revision;
    return true;
}

const std::vector<double>& MLPBrain::weightsAt(const std::size_t layer) const
{
    return weights_.at(layer);
}

const std::vector<double>& MLPBrain::biasesAt(const std::size_t layer) const
{
    return biases_.at(layer);
}

bool MLPBrain::setLayerForTesting(const std::size_t layer, std::vector<double> weights, std::vector<double> biases)
{
    if (layer >= weights_.size() || layer + 1U >= layerSizes_.size())
    {
        return false;
    }
    const std::size_t expectedWeights = layerSizes_[layer] * layerSizes_[layer + 1U];
    const std::size_t expectedBiases = layerSizes_[layer + 1U];
    if (weights.size() != expectedWeights || biases.size() != expectedBiases)
    {
        return false;
    }
    weights_[layer] = std::move(weights);
    biases_[layer] = std::move(biases);
    ++state_.revision;
    return true;
}
} // namespace agentbiosim::neural
