#include "neural/ShortcutMLPBrain.hpp"

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

ShortcutMLPBrain::ShortcutMLPBrain(BrainConfig config, std::mt19937_64& rng)
    : config_(std::move(config)),
      layerSizes_(config_.layerSizes()),
      shortcutScale_(config_.future.shortcutScale)
{
    initialize(rng);
}

std::size_t ShortcutMLPBrain::inputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.front();
}

std::size_t ShortcutMLPBrain::outputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.back();
}

const std::vector<std::size_t>& ShortcutMLPBrain::layerSizes() const noexcept
{
    return layerSizes_;
}

const BrainConfig& ShortcutMLPBrain::config() const noexcept
{
    return config_;
}

const BrainState& ShortcutMLPBrain::state() const noexcept
{
    return state_;
}

std::uint64_t ShortcutMLPBrain::revision() const noexcept
{
    return state_.revision;
}

void ShortcutMLPBrain::initialize(std::mt19937_64& rng)
{
    weights_.clear();
    biases_.clear();
    shortcutWeights_.clear();
    shortcutBias_.clear();
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

        std::vector<double> w(rows * cols, 0.0);
        for (double& v : w)
        {
            v = weightDistribution(rng);
        }
        weights_.push_back(std::move(w));

        std::vector<double> b(rows, 0.0);
        if (config_.randomBiases)
        {
            for (double& v : b)
            {
                v = biasDistribution(rng);
            }
        }
        biases_.push_back(std::move(b));
    }

    // Shortcut: output_size x input_size, init std normalized by sqrt(input_size).
    const std::size_t inSize = inputSize();
    const std::size_t outSize = outputSize();
    const double sStd = std::max(0.0, config_.future.shortcutInitStd) /
                        std::sqrt(static_cast<double>(std::max<std::size_t>(1U, inSize)));
    std::normal_distribution<double> sDist(0.0, sStd);
    shortcutWeights_.assign(outSize * inSize, 0.0);
    for (double& v : shortcutWeights_)
    {
        v = sDist(rng);
    }
    shortcutBias_.assign(outSize, 0.0);

    state_.revision = 0;
}

std::vector<double> ShortcutMLPBrain::forward(const std::vector<double>& input,
                                               ActivationTrace* trace) const
{
    if (trace != nullptr)
    {
        trace->clear();
        trace->brainType = BrainType::ShortcutMlp;
    }

    const std::size_t inSize = inputSize();
    const std::size_t outSize = outputSize();
    std::vector<double> activations(inSize, 0.0);
    const std::size_t copied = std::min(inSize, input.size());
    std::copy_n(input.begin(), copied, activations.begin());

    // Standard MLP path with tanh hidden, linear output.
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
            ActivationLayer l;
            l.outputLayer = layer + 1U == weights_.size();
            l.name = l.outputLayer ? "output" : "hidden_" + std::to_string(layer + 1U);
            l.values = next;
            trace->layers.push_back(std::move(l));
        }
        activations = std::move(next);
    }

    // Add shortcut contribution: scale * (Wshort * input + bShort)
    std::vector<double> contribution(outSize, 0.0);
    if (shortcutScale_ != 0.0 && !shortcutWeights_.empty())
    {
        for (std::size_t row = 0; row < outSize; ++row)
        {
            double sum = shortcutBias_[row];
            for (std::size_t col = 0; col < inSize; ++col)
            {
                sum += shortcutWeights_[row * inSize + col] * (col < input.size() ? input[col] : 0.0);
            }
            contribution[row] = shortcutScale_ * sum;
            activations[row] += contribution[row];
        }
    }

    if (trace != nullptr)
    {
        trace->shortcutContribution = contribution;
        // Update output layer values to include shortcut contribution.
        if (!trace->layers.empty())
        {
            trace->layers.back().values = activations;
        }
    }
    return activations;
}

ShortcutMLPBrain ShortcutMLPBrain::clone() const
{
    return *this;
}

std::string ShortcutMLPBrain::batchKey() const
{
    std::ostringstream out;
    out << "shortcut_mlp:";
    for (const std::size_t s : layerSizes_)
    {
        out << s << ',';
    }
    out << ':' << shortcutScale_;
    return out.str();
}

double ShortcutMLPBrain::checksum() const
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
    for (const double v : shortcutWeights_)
    {
        total += v * scale;
        scale += 0.000001;
    }
    for (const double v : shortcutBias_)
    {
        total += v * scale;
        scale += 0.000001;
    }
    return total;
}

std::size_t ShortcutMLPBrain::parameterCount() const
{
    std::size_t total = 0;
    for (const auto& l : weights_) total += l.size();
    for (const auto& l : biases_) total += l.size();
    total += shortcutWeights_.size();
    total += shortcutBias_.size();
    return total;
}

bool ShortcutMLPBrain::mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng)
{
    const double baseRate = std::clamp(mutCfg.baseRate, 0.0, 1.0);
    const double baseStr = std::max(0.0, mutCfg.baseStrength);
    bool changed = false;

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

    const double sRate = std::clamp(mutCfg.effectiveShortcutRate(), 0.0, 1.0);
    const double sStr = std::max(0.0, mutCfg.effectiveShortcutStrength());
    if (sRate > 0.0 && sStr > 0.0)
    {
        std::bernoulli_distribution sm(sRate);
        std::normal_distribution<double> snoise(0.0, sStr);
        for (double& v : shortcutWeights_)
        {
            if (sm(rng))
            {
                v += snoise(rng);
                changed = true;
            }
        }
        for (double& v : shortcutBias_)
        {
            if (sm(rng))
            {
                v += snoise(rng);
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
} // namespace agentbiosim::neural
