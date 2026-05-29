#include "neural/SimpleRNNBrain.hpp"

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

SimpleRNNBrain::SimpleRNNBrain(BrainConfig config, std::mt19937_64& rng)
    : config_(std::move(config)),
      layerSizes_(config_.layerSizes()),
      recurrentScale_(config_.future.rnnRecurrentScale),
      memoryDecay_(std::clamp(config_.future.rnnMemoryDecay, 0.0, 0.999)),
      stateClip_(std::max(0.01, config_.future.rnnStateClip)),
      resetStateOnCopy_(config_.future.rnnResetStateOnCopy)
{
    initialize(rng);
}

std::size_t SimpleRNNBrain::inputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.front();
}

std::size_t SimpleRNNBrain::outputSize() const noexcept
{
    return layerSizes_.empty() ? 0U : layerSizes_.back();
}

const std::vector<std::size_t>& SimpleRNNBrain::layerSizes() const noexcept
{
    return layerSizes_;
}

const BrainConfig& SimpleRNNBrain::config() const noexcept
{
    return config_;
}

const BrainState& SimpleRNNBrain::state() const noexcept
{
    return brainState_;
}

std::uint64_t SimpleRNNBrain::revision() const noexcept
{
    return brainState_.revision;
}

void SimpleRNNBrain::initialize(std::mt19937_64& rng)
{
    weights_.clear();
    biases_.clear();
    recurrentWeights_.clear();
    state_.clear();

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
        std::normal_distribution<double> weightDist(0.0, stddev);
        std::normal_distribution<double> biasDist(0.0, stddev * 0.5);

        std::vector<double> w(rows * cols, 0.0);
        for (double& v : w) v = weightDist(rng);
        weights_.push_back(std::move(w));

        std::vector<double> b(rows, 0.0);
        if (config_.randomBiases)
        {
            for (double& v : b) v = biasDist(rng);
        }
        biases_.push_back(std::move(b));
    }

    // Recurrent weights: state_size x state_size (state_size = first hidden layer size).
    // If no hidden layer (direct input->output), no recurrent state.
    const std::size_t stateSize = layerSizes_.size() > 2U ? layerSizes_[1] : 0U;
    if (stateSize > 0U)
    {
        const double rStd = std::max(0.0, config_.future.rnnRecurrentInitStd) /
                            std::sqrt(static_cast<double>(std::max<std::size_t>(1U, stateSize)));
        std::normal_distribution<double> rDist(0.0, rStd);
        recurrentWeights_.assign(stateSize * stateSize, 0.0);
        for (double& v : recurrentWeights_) v = rDist(rng);
        state_.assign(stateSize, 0.0);
    }

    brainState_.revision = 0;
    brainState_.recurrentState = state_;
}

std::vector<double> SimpleRNNBrain::forwardImpl(const std::vector<double>& input,
                                                  ActivationTrace* trace,
                                                  const bool updateState)
{
    if (trace != nullptr)
    {
        trace->clear();
        trace->brainType = BrainType::SimpleRnn;
        trace->recurrentMemoryDecay = memoryDecay_;
        trace->recurrentStateClip = stateClip_;
        trace->recurrentStateBefore = state_;
    }

    const std::size_t inSize = inputSize();
    std::vector<double> activations(inSize, 0.0);
    const std::size_t copied = std::min(inSize, input.size());
    std::copy_n(input.begin(), copied, activations.begin());

    const std::size_t numLayers = weights_.size();
    if (numLayers == 0U)
    {
        if (trace != nullptr)
        {
            trace->recurrentStateAfter = state_;
        }
        return activations;
    }

    // First layer: optionally includes recurrent contribution.
    // hidden = tanh(W[0] @ input + b[0] + scale * (W_rec @ state))
    {
        const std::size_t rows = layerSizes_[1U];
        const std::size_t cols = layerSizes_[0];
        std::vector<double> hidden(rows, 0.0);

        for (std::size_t row = 0; row < rows; ++row)
        {
            double sum = biases_[0][row];
            for (std::size_t col = 0; col < cols; ++col)
            {
                sum += weights_[0][row * cols + col] * activations[col];
            }
            // Recurrent contribution applied at first hidden layer.
            if (!state_.empty() && row < state_.size() && rows == state_.size())
            {
                double rec = 0.0;
                for (std::size_t k = 0; k < state_.size(); ++k)
                {
                    rec += recurrentWeights_[row * state_.size() + k] * state_[k];
                }
                sum += recurrentScale_ * rec;
            }
            // Apply tanh only if this is a hidden layer (not the output layer).
            if (numLayers > 1U)
            {
                sum = std::tanh(sum);
            }
            hidden[row] = sum;
        }

        // Update state when running live forward (not pure activation trace).
        if (updateState && !state_.empty() && rows == state_.size())
        {
            const double keep = memoryDecay_;
            const double bring = 1.0 - keep;
            for (std::size_t i = 0; i < state_.size(); ++i)
            {
                double v = keep * state_[i] + bring * hidden[i];
                v = std::clamp(v, -stateClip_, stateClip_);
                state_[i] = v;
            }
        }

        if (trace != nullptr)
        {
            ActivationLayer l;
            l.outputLayer = numLayers == 1U;
            l.name = l.outputLayer ? "output" : "hidden_1";
            l.values = hidden;
            trace->layers.push_back(std::move(l));
        }

        activations = std::move(hidden);
    }

    // Remaining layers: standard MLP path (linear at output).
    for (std::size_t layer = 1; layer < numLayers; ++layer)
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
            if (layer + 1U < numLayers)
            {
                sum = std::tanh(sum);
            }
            next[row] = sum;
        }

        if (trace != nullptr)
        {
            ActivationLayer l;
            l.outputLayer = layer + 1U == numLayers;
            l.name = l.outputLayer ? "output" : "hidden_" + std::to_string(layer + 1U);
            l.values = next;
            trace->layers.push_back(std::move(l));
        }

        activations = std::move(next);
    }

    if (trace != nullptr)
    {
        trace->recurrentStateAfter = state_;
    }
    return activations;
}

std::vector<double> SimpleRNNBrain::forward(const std::vector<double>& input,
                                              ActivationTrace* trace)
{
    return forwardImpl(input, trace, /*updateState=*/true);
}

std::vector<double> SimpleRNNBrain::forward(const std::vector<double>& input,
                                              ActivationTrace* trace) const
{
    // Const overload: state is captured into a local copy so callers reading
    // const can still get outputs without mutating live state.
    SimpleRNNBrain temp = *this;
    return temp.forwardImpl(input, trace, /*updateState=*/false);
}

SimpleRNNBrain SimpleRNNBrain::clone() const
{
    SimpleRNNBrain c = *this;
    if (resetStateOnCopy_)
    {
        std::fill(c.state_.begin(), c.state_.end(), 0.0);
        c.brainState_.recurrentState = c.state_;
    }
    return c;
}

void SimpleRNNBrain::resetState() noexcept
{
    std::fill(state_.begin(), state_.end(), 0.0);
}

std::string SimpleRNNBrain::batchKey() const
{
    std::ostringstream out;
    out << "simple_rnn:";
    for (const std::size_t s : layerSizes_) out << s << ',';
    out << ':' << recurrentScale_ << ':' << memoryDecay_ << ':' << stateClip_;
    return out.str();
}

double SimpleRNNBrain::checksum() const
{
    double total = 0.0;
    double scale = 1.0;
    for (const auto& l : weights_) for (const double v : l) { total += v * scale; scale += 0.000001; }
    for (const auto& l : biases_) for (const double v : l) { total += v * scale; scale += 0.000001; }
    for (const double v : recurrentWeights_) { total += v * scale; scale += 0.000001; }
    return total;
}

std::size_t SimpleRNNBrain::parameterCount() const
{
    std::size_t total = 0;
    for (const auto& l : weights_) total += l.size();
    for (const auto& l : biases_) total += l.size();
    total += recurrentWeights_.size();
    return total;
}

bool SimpleRNNBrain::mutate(const NeuralMutationConfig& mutCfg, std::mt19937_64& rng)
{
    const double baseRate = std::clamp(mutCfg.baseRate, 0.0, 1.0);
    const double baseStr = std::max(0.0, mutCfg.baseStrength);
    bool changed = false;

    if (baseRate > 0.0 && baseStr > 0.0)
    {
        std::bernoulli_distribution bm(baseRate);
        std::normal_distribution<double> noise(0.0, baseStr);
        for (auto& layer : weights_)
            for (double& v : layer) if (bm(rng)) { v += noise(rng); changed = true; }
        for (auto& layer : biases_)
            for (double& v : layer) if (bm(rng)) { v += noise(rng); changed = true; }
    }

    const double rRate = std::clamp(mutCfg.effectiveRecurrentRate(), 0.0, 1.0);
    const double rStr = std::max(0.0, mutCfg.effectiveRecurrentStrength());
    if (rRate > 0.0 && rStr > 0.0)
    {
        std::bernoulli_distribution rm(rRate);
        std::normal_distribution<double> rnoise(0.0, rStr);
        for (double& v : recurrentWeights_)
        {
            if (rm(rng)) { v += rnoise(rng); changed = true; }
        }
    }

    if (changed)
    {
        ++brainState_.revision;
    }
    return changed;
}
} // namespace agentbiosim::neural
