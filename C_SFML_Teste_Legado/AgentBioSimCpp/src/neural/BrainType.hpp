#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace agentbiosim::neural
{
enum class BrainType
{
    Mlp,
    GatedMlp,
    ShortcutMlp,
    ModulatedMlp,
    SimpleRnn,
    Neat,
    SimpleNeat,
    RecurrentNeat
};

inline std::string normalizeBrainTypeText(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        const char lower = static_cast<char>(std::tolower(ch));
        return lower == '-' || lower == ' ' ? '_' : lower;
    });
    return value;
}

inline BrainType normalizeBrainType(const std::string& value)
{
    const std::string text = normalizeBrainTypeText(value);
    if (text == "gated" || text == "gates" || text == "mlp_gates" || text == "gated_mlp")
    {
        return BrainType::GatedMlp;
    }
    if (text == "shortcut" || text == "atalho" || text == "shortcut_mlp")
    {
        return BrainType::ShortcutMlp;
    }
    if (text == "modulated" || text == "modulada" || text == "modulated_mlp")
    {
        return BrainType::ModulatedMlp;
    }
    if (text == "rnn" || text == "simple_rnn")
    {
        return BrainType::SimpleRnn;
    }
    if (text == "neat" || text == "neat_common" || text == "common_neat" || text == "neat_comum")
    {
        return BrainType::Neat;
    }
    if (text == "proto_neat" || text == "protozoa_neat" || text == "neat_protozoa" ||
        text == "neat_simplified" || text == "neat_simplificada")
    {
        return BrainType::SimpleNeat;
    }
    if (text == "recurrent_neat" || text == "neat_recurrent" || text == "neat_recorrente")
    {
        return BrainType::RecurrentNeat;
    }
    return BrainType::Mlp;
}

inline const char* brainTypeName(const BrainType type)
{
    switch (type)
    {
    case BrainType::Mlp:
        return "mlp";
    case BrainType::GatedMlp:
        return "gated_mlp";
    case BrainType::ShortcutMlp:
        return "shortcut_mlp";
    case BrainType::ModulatedMlp:
        return "modulated_mlp";
    case BrainType::SimpleRnn:
        return "simple_rnn";
    case BrainType::Neat:
        return "neat_common";
    case BrainType::SimpleNeat:
        return "neat_simplified";
    case BrainType::RecurrentNeat:
        return "neat_recurrent";
    }
    return "mlp";
}

inline const char* brainTypeLabel(const BrainType type)
{
    switch (type)
    {
    case BrainType::Mlp:
        return "MLP padrao";
    case BrainType::GatedMlp:
        return "MLP com gates";
    case BrainType::ShortcutMlp:
        return "MLP com atalho";
    case BrainType::ModulatedMlp:
        return "MLP modulada";
    case BrainType::SimpleRnn:
        return "RNN simples";
    case BrainType::Neat:
        return "NEAT comum";
    case BrainType::SimpleNeat:
        return "NEAT simplificada";
    case BrainType::RecurrentNeat:
        return "NEAT recorrente";
    }
    return "MLP padrao";
}

inline bool isImplementedInPhase9(const BrainType type)
{
    return type == BrainType::Mlp;
}

inline bool isImplementedInPhase14(const BrainType type)
{
    return type == BrainType::Mlp || type == BrainType::GatedMlp ||
           type == BrainType::ShortcutMlp || type == BrainType::ModulatedMlp;
}

inline bool isImplementedInPhase15(const BrainType type)
{
    return isImplementedInPhase14(type) || type == BrainType::SimpleRnn;
}
} // namespace agentbiosim::neural
