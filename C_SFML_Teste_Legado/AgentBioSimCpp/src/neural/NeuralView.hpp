#pragma once

#include "neural/ActivationTrace.hpp"
#include "neural/BrainType.hpp"
#include "neural/BrainVariant.hpp"

#include <cstddef>
#include <vector>

namespace agentbiosim::neural
{
// Phase 26: a read-only, UI-agnostic snapshot of a single brain ready to be
// drawn by the neural viewer. It bundles topology (from the brain) with the
// activations of the last forward (from the ActivationTrace), in a uniform
// column/row node + edge model that works for every brain type:
//
//   * Dense (MLP/Gated/Shortcut/Modulated) and RNN: one column per layer.
//   * NEAT (common/simplified/recurrent): columns derived from each node's
//     graph layer (0 = inputs, last = outputs), edges from the connections.
//
// The builder reads only — it never mutates the brain. It is the single source
// the headless selftest and the ImGui viewer both consume, so the UI never
// reaches into brain internals.
struct NeuralViewNode
{
    int id = 0;            // dense: column*100000 + row; NEAT: graph node id
    int column = 0;        // x bucket (0 = inputs ... columnCount-1 = outputs)
    int row = 0;           // y position within the column
    int kind = 0;          // 0 = input, 1 = hidden, 2 = output
    double activation = 0.0;
};

struct NeuralViewEdge
{
    int fromNode = 0;      // index into NeuralView::nodes
    int toNode = 0;        // index into NeuralView::nodes
    double weight = 0.0;
    bool enabled = true;
    bool recurrent = false;
};

struct NeuralView
{
    bool valid = false;
    BrainType type = BrainType::Mlp;

    int columnCount = 0;
    std::vector<NeuralViewNode> nodes;
    std::vector<NeuralViewEdge> edges;

    // Dense extras (empty / false when not applicable).
    std::vector<std::vector<double>> gates;  // gated/modulated: per hidden layer
    std::vector<double> shortcut;            // shortcut/modulated: per-output contribution
    bool hasShortcut = false;

    // Recurrent extras (RNN / recurrent NEAT).
    bool hasRecurrentState = false;
    std::vector<double> recurrentState;
    double memoryDecay = 0.0;
    double stateClip = 0.0;

    // NEAT statistics.
    std::size_t connectionCount = 0;
    std::size_t enabledConnectionCount = 0;
    std::size_t recurrentConnectionCount = 0;

    void clear()
    {
        valid = false;
        type = BrainType::Mlp;
        columnCount = 0;
        nodes.clear();
        edges.clear();
        gates.clear();
        shortcut.clear();
        hasShortcut = false;
        hasRecurrentState = false;
        recurrentState.clear();
        memoryDecay = 0.0;
        stateClip = 0.0;
        connectionCount = 0;
        enabledConnectionCount = 0;
        recurrentConnectionCount = 0;
    }

    [[nodiscard]] int maxRowsInAnyColumn() const noexcept
    {
        int maxRows = 0;
        for (const auto& node : nodes)
        {
            maxRows = node.row + 1 > maxRows ? node.row + 1 : maxRows;
        }
        return maxRows;
    }
};

// Build a view from a brain + the trace of its last forward. `input` (optional)
// supplies the input-layer activations, which the dense/RNN ActivationTrace does
// not store (it begins at the first hidden layer). Pure read; no mutation.
[[nodiscard]] NeuralView buildNeuralView(const BrainVariant& brain,
                                         const ActivationTrace& trace,
                                         const std::vector<double>* input = nullptr);
} // namespace agentbiosim::neural
