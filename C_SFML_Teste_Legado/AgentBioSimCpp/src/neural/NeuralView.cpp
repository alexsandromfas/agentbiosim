#include "neural/NeuralView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <unordered_map>

namespace agentbiosim::neural
{
namespace
{
// Dense / RNN family: one column per layer. Column 0 activations come from the
// supplied input (the ActivationTrace begins at the first hidden layer, so it
// has no input row); columns 1..L-1 come from trace.layers.
template <typename Brain>
void buildDense(const Brain& brain, const ActivationTrace& trace,
                const std::vector<double>* input, NeuralView& view)
{
    const std::vector<std::size_t>& sizes = brain.layerSizes();
    const std::size_t columns = sizes.size();
    view.columnCount = static_cast<int>(columns);

    std::vector<int> offset(columns + 1, 0);
    for (std::size_t c = 0; c < columns; ++c)
    {
        offset[c + 1] = offset[c] + static_cast<int>(sizes[c]);
    }

    view.nodes.reserve(static_cast<std::size_t>(offset[columns]));
    for (std::size_t c = 0; c < columns; ++c)
    {
        const std::vector<double>* acts = nullptr;
        if (c == 0)
        {
            acts = input;
        }
        else if (c - 1 < trace.layers.size() && trace.layers[c - 1].values.size() == sizes[c])
        {
            acts = &trace.layers[c - 1].values;
        }
        for (std::size_t r = 0; r < sizes[c]; ++r)
        {
            NeuralViewNode node;
            node.column = static_cast<int>(c);
            node.row = static_cast<int>(r);
            node.id = static_cast<int>(c) * 100000 + static_cast<int>(r);
            node.kind = (c == 0) ? 0 : (c + 1 == columns ? 2 : 1);
            node.activation = (acts != nullptr && r < acts->size()) ? (*acts)[r] : 0.0;
            view.nodes.push_back(node);
        }
    }

    // Edges from the weight matrices. weights[wl] is row-major out x in, connecting
    // column wl (in = sizes[wl]) to column wl+1 (out = sizes[wl+1]).
    const auto& weights = brain.weights();
    for (std::size_t wl = 0; wl < weights.size() && wl + 1 < columns; ++wl)
    {
        const std::size_t cols = sizes[wl];
        const std::size_t rows = sizes[wl + 1];
        const auto& w = weights[wl];
        if (w.size() < rows * cols)
        {
            continue;
        }
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t col = 0; col < cols; ++col)
            {
                NeuralViewEdge edge;
                edge.fromNode = offset[wl] + static_cast<int>(col);
                edge.toNode = offset[wl + 1] + static_cast<int>(row);
                edge.weight = w[row * cols + col];
                view.edges.push_back(edge);
            }
        }
    }
}

void buildNeat(const NEATGraphBrain& brain, const ActivationTrace& trace, NeuralView& view)
{
    constexpr int kColumns = 8;
    view.columnCount = kColumns;

    const auto& nodes = brain.nodes();
    std::unordered_map<std::int32_t, int> idToIndex;
    idToIndex.reserve(nodes.size());
    std::vector<int> rowCursor(static_cast<std::size_t>(kColumns), 0);

    view.nodes.reserve(nodes.size());
    for (const auto& n : nodes)
    {
        int col;
        if (n.kind == NEATGraphBrain::NodeKind::Input)
        {
            col = 0;
        }
        else if (n.kind == NEATGraphBrain::NodeKind::Output)
        {
            col = kColumns - 1;
        }
        else
        {
            const double layer = std::clamp(n.layer, 0.05, 0.95);
            col = std::clamp(static_cast<int>(std::lround(layer * (kColumns - 1))), 1, kColumns - 2);
        }
        NeuralViewNode vn;
        vn.column = col;
        vn.row = rowCursor[static_cast<std::size_t>(col)]++;
        vn.id = n.id;
        vn.kind = static_cast<int>(n.kind);
        vn.activation = 0.0;
        idToIndex[n.id] = static_cast<int>(view.nodes.size());
        view.nodes.push_back(vn);
    }

    for (const auto& na : trace.neatNodes)
    {
        const auto it = idToIndex.find(na.id);
        if (it != idToIndex.end())
        {
            view.nodes[static_cast<std::size_t>(it->second)].activation = na.value;
        }
    }

    const auto& conns = brain.connections();
    view.edges.reserve(conns.size());
    for (const auto& c : conns)
    {
        const auto sit = idToIndex.find(c.src);
        const auto dit = idToIndex.find(c.dst);
        if (sit == idToIndex.end() || dit == idToIndex.end())
        {
            continue;
        }
        NeuralViewEdge e;
        e.fromNode = sit->second;
        e.toNode = dit->second;
        e.weight = c.weight;
        e.enabled = c.enabled;
        e.recurrent = c.recurrent;
        view.edges.push_back(e);
    }

    view.connectionCount = brain.connections().size();
    view.enabledConnectionCount = brain.enabledConnectionCount();
    view.recurrentConnectionCount = brain.recurrentConnectionCount();
    if (brain.allowsRecurrent())
    {
        view.hasRecurrentState = true;
        view.memoryDecay = brain.memoryDecay();
        view.stateClip = brain.stateClip();
    }
}
} // namespace

NeuralView buildNeuralView(const BrainVariant& brain, const ActivationTrace& trace,
                           const std::vector<double>* input)
{
    NeuralView view;
    view.type = brainTypeOf(brain);

    std::visit(
        [&](const auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, NEATGraphBrain>)
            {
                buildNeat(b, trace, view);
            }
            else
            {
                buildDense(b, trace, input, view);
                if constexpr (std::is_same_v<T, GatedMLPBrain> ||
                              std::is_same_v<T, ModulatedMLPBrain>)
                {
                    view.gates = b.gates();
                }
                if constexpr (std::is_same_v<T, ShortcutMLPBrain> ||
                              std::is_same_v<T, ModulatedMLPBrain>)
                {
                    view.hasShortcut = true;
                    view.shortcut = trace.shortcutContribution;
                }
                if constexpr (std::is_same_v<T, SimpleRNNBrain>)
                {
                    view.hasRecurrentState = true;
                    view.recurrentState = b.recurrentState();
                    view.memoryDecay = b.memoryDecay();
                    view.stateClip = b.stateClip();
                }
            }
        },
        brain);

    view.valid = !view.nodes.empty();
    return view;
}
} // namespace agentbiosim::neural
