#pragma once

#include "neural/BrainType.hpp"
#include "neural/GatedMLPBrain.hpp"
#include "neural/MLPBrain.hpp"
#include "neural/ModulatedMLPBrain.hpp"
#include "neural/ShortcutMLPBrain.hpp"

#include <variant>

namespace agentbiosim::neural
{
// Dense-brain variant. Future Phase 15 (RNN) and Phase 16 (NEAT) will add new
// variants here. std::visit gives compile-time dispatch with no virtual calls
// and avoids heap allocation per brain.
using BrainVariant = std::variant<MLPBrain, GatedMLPBrain, ShortcutMLPBrain, ModulatedMLPBrain>;

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
            else
            {
                return BrainType::ModulatedMlp;
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

inline std::vector<double> forwardOf(const BrainVariant& v,
                                     const std::vector<double>& input,
                                     ActivationTrace* trace = nullptr)
{
    return std::visit([&](const auto& b) { return b.forward(input, trace); }, v);
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
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, MLPBrain>)
            {
                return b.mutate(mutCfg, rng);
            }
            else
            {
                return b.mutate(mutCfg, rng);
            }
        },
        v);
}

inline std::string batchKeyOf(const BrainVariant& v)
{
    return std::visit([](const auto& b) { return b.batchKey(); }, v);
}
} // namespace agentbiosim::neural
