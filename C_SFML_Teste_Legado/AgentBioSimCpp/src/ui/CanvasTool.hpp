#pragma once

#include <cstdint>
#include <string>

namespace agentbiosim::ui
{
// Phase 22: enum of canvas tools selectable via UI/toolbar/shortcuts.
// `None` means no tool active. The InputRouter and UiPanel never inspect the
// engine state to derive this; they only mutate UiState::activeTool through
// commands.
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
    switch (t)
    {
    case CanvasTool::None:           return "None";
    case CanvasTool::Select:         return "Select";
    case CanvasTool::RectangleSelect:return "Rect";
    case CanvasTool::LassoSelect:    return "Lasso";
    case CanvasTool::AddFood:        return "Food+";
    case CanvasTool::AddAgent:       return "Agent+";
    case CanvasTool::PaintObstacle:  return "Brush";
    case CanvasTool::EraseObstacle:  return "Eraser";
    case CanvasTool::Move:           return "Move";
    case CanvasTool::Delete:         return "Delete";
    case CanvasTool::Pan:            return "Pan";
    }
    return "None";
}
} // namespace agentbiosim::ui
