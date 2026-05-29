#pragma once

#include "neural/BrainType.hpp"
#include "neural/GatedMLPBrain.hpp"
#include "neural/MLPBrain.hpp"
#include "neural/ModulatedMLPBrain.hpp"
#include "neural/NEATGraphBrain.hpp"
#include "neural/ShortcutMLPBrain.hpp"
#include "neural/SimpleRNNBrain.hpp"

#include <variant>

namespace agentbiosim::neural
{
// Brain variant. Phase 16 adds NEATGraphBrain (single class covering common,
// simplified and recurrent NEAT variants via its internal brainType()). std::visit
// gives compile-time dispatch with no virtual calls and avoids heap allocation per brain.
using BrainVariant = std::variant<MLPBrain, GatedMLPBrain, ShortcutMLPBrain,
                                  ModulatedMLPBrain, SimpleRNNBrain, NEATGraphBrain>;

inline BrainType brainTypeOf(const BrainVariant& v)
{
    return std::visit(
        [](const auto& b) -> BrainType {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, MLPBrain>)
            {
                return BrainType::Mlp;
            }
            else if constexpr (std::is_same_v<T, GatedMLPBrain>)
            {
                return BrainType::GatedMlp;
            }
            else if constexpr (std::is_same_v<T, ShortcutMLPBrain>)
            {
                return BrainType::ShortcutMlp;
            }
            else if constexpr (std::is_same_v<T, ModulatedMLPBrain>)
            {
                return BrainType::ModulatedMlp;
            }
            else if constexpr (std::is_same_v<T, SimpleRNNBrain>)
            {
                return BrainType::SimpleRnn;
            }
            else
            {
                return b.brainType();
            }
        },
        v);
}

inline std::size_t inputSizeOf(const BrainVariant& v)
{
    return std::visit([](const auto& b) { return b.inputSize(); }, v);
}

inline std::size_t outputSizeOf(const BrainVariant& v)
{
    return std::visit([](const auto& b) { return b.outputSize(); }, v);
}

// Const overload: forward without mutating live state (RNN/NEAT recurrent state is captured/restored).
inline std::vector<double> forwardOf(const BrainVariant& v,
                                     const std::vector<double>& input,
                                     ActivationTrace* trace = nullptr)
{
    return std::visit([&](const auto& b) { return b.forward(input, trace); }, v);
}

// Non-const overload: forward AND update RNN/NEAT recurrent state when applicable.
// MLP/Gated/Shortcut/Modulated brains have only const forward, which is also called here.
inline std::vector<double> forwardOf(BrainVariant& v,
                                     const std::vector<double>& input,
                                     ActivationTrace* trace = nullptr)
{
    return std::visit([&](auto& b) { return b.forward(input, trace); }, v);
}

inline void resetStateOf(BrainVariant& v)
{
    std::visit(
        [](auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, SimpleRNNBrain>)
            {
                b.resetState();
            }
            else if constexpr (std::is_same_v<T, NEATGraphBrain>)
            {
                b.resetState();
            }
        },
        v);
}

inline BrainVariant cloneOf(const BrainVariant& v)
{
    return std::visit(
        [](const auto& b) -> BrainVariant { return BrainVariant{b.clone()}; },
        v);
}

inline bool mutateOf(BrainVariant& v, const NeuralMutationConfig& mutCfg, std::mt19937_64& rng)
{
    return std::visit(
        [&](auto& b) {
            return b.mutate(mutCfg, rng);
        },
        v);
}

inline std::string batchKeyOf(const BrainVariant& v)
{
    return std::visit([](const auto& b) { return b.batchKey(); }, v);
}
} // namespace agentbiosim::neural
