#pragma once

#include <cstdint>
#include <string>

namespace agentbiosim::core
{
// Phase 22: enum of canvas tools selectable via UI/toolbar/shortcuts.
// `None` means no tool active. The InputRouter and UiPanel never inspect the
// engine state to derive this; they only mutate UiState::activeTool through
// commands.
//
// Phase 25 (Divida 8): moved from agentbiosim::ui to the neutral agentbiosim::core
// layer so that the engine (sim::SimulationRunner) can reference CanvasTool /
// Command without depending on the ui:: layer. The dependency arrow now points
// only UI -> core, never engine -> ui.
enum class CanvasTool : std::uint8_t
{
    None = 0,
    Select,
    RectangleSelect,
    LassoSelect,
    AddFood,
    AddAgent,
    PaintObstacle,
    EraseObstacle,
    Move,
    Delete,
    Pan
};

inline const char* canvasToolName(const CanvasTool t) noexcept
{
    switch (t)
    {
    case CanvasTool::None:           return "none";
    case CanvasTool::Select:         return "select";
    case CanvasTool::RectangleSelect:return "rect_select";
    case CanvasTool::LassoSelect:    return "lasso";
    case CanvasTool::AddFood:        return "add_food";
    case CanvasTool::AddAgent:       return "add_agent";
    case CanvasTool::PaintObstacle:  return "paint_obstacle";
    case CanvasTool::EraseObstacle:  return "erase_obstacle";
    case CanvasTool::Move:           return "move";
    case CanvasTool::Delete:         return "delete";
    case CanvasTool::Pan:            return "pan";
    }
    return "none";
}

inline const char* canvasToolLabel(const CanvasTool t) noexcept
{
    // Phase 23.1: labels padronizados em portugues ASCII para casar com os
    // menus (Arquivo / Exibir / Preferencias / Ajuda) e remover mistura
    // portugues+ingles na toolbar.
    switch (t)
    {
    case CanvasTool::None:           return "None";
    case CanvasTool::Select:         return "Selecao";
    case CanvasTool::RectangleSelect:return "Rect";
    case CanvasTool::LassoSelect:    return "Laco";
    case CanvasTool::AddFood:        return "Comida";
    case CanvasTool::AddAgent:       return "Agente";
    case CanvasTool::PaintObstacle:  return "Pincel";
    case CanvasTool::EraseObstacle:  return "Apagar";
    case CanvasTool::Move:           return "Mover";
    case CanvasTool::Delete:         return "Excluir";
    case CanvasTool::Pan:            return "Pan";
    }
    return "None";
}
} // namespace agentbiosim::core
