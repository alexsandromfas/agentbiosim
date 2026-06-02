#include "ui/ImGuiTheme.hpp"

#include <imgui.h>

namespace agentbiosim::ui
{
namespace
{
// Base palette (sRGB-ish, picked for a calm dark technical tool).
constexpr ImVec4 kBg        = ImVec4(0.086f, 0.094f, 0.118f, 1.00f); // window bg
constexpr ImVec4 kBgChild   = ImVec4(0.106f, 0.114f, 0.141f, 1.00f); // child/card bg
constexpr ImVec4 kBgPopup   = ImVec4(0.071f, 0.078f, 0.098f, 0.98f);
constexpr ImVec4 kFrame     = ImVec4(0.157f, 0.169f, 0.204f, 1.00f); // inputs/sliders
constexpr ImVec4 kFrameHov  = ImVec4(0.200f, 0.216f, 0.259f, 1.00f);
constexpr ImVec4 kFrameAct  = ImVec4(0.235f, 0.255f, 0.306f, 1.00f);
constexpr ImVec4 kHeader    = ImVec4(0.169f, 0.184f, 0.224f, 1.00f);
constexpr ImVec4 kText      = ImVec4(0.886f, 0.902f, 0.929f, 1.00f);
constexpr ImVec4 kTextDim   = ImVec4(0.514f, 0.545f, 0.604f, 1.00f);
constexpr ImVec4 kBorder    = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

// Teal/green accent for primary actions, selection and active state.
constexpr ImVec4 kAccent    = ImVec4(0.149f, 0.690f, 0.529f, 1.00f);
constexpr ImVec4 kAccentHov = ImVec4(0.184f, 0.776f, 0.596f, 1.00f);
constexpr ImVec4 kAccentAct = ImVec4(0.247f, 0.847f, 0.659f, 1.00f);
} // namespace

ThemeAccent themeAccent() noexcept
{
    return {kAccent.x, kAccent.y, kAccent.z, kAccent.w};
}

void applyImGuiTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // Spacing rhythm (multiples of 4/8) and rounding for a modern, soft look.
    s.WindowPadding     = ImVec2(14.0f, 12.0f);
    s.FramePadding      = ImVec2(10.0f, 6.0f);
    s.ItemSpacing       = ImVec2(8.0f, 8.0f);
    s.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    s.IndentSpacing     = 18.0f;
    s.ScrollbarSize     = 13.0f;
    s.GrabMinSize       = 11.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;

    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;

    s.WindowTitleAlign  = ImVec2(0.02f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = kText;
    c[ImGuiCol_TextDisabled]          = kTextDim;
    c[ImGuiCol_WindowBg]              = kBg;
    c[ImGuiCol_ChildBg]               = kBgChild;
    c[ImGuiCol_PopupBg]               = kBgPopup;
    c[ImGuiCol_Border]                = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    c[ImGuiCol_BorderShadow]          = kBorder;
    c[ImGuiCol_FrameBg]               = kFrame;
    c[ImGuiCol_FrameBgHovered]        = kFrameHov;
    c[ImGuiCol_FrameBgActive]         = kFrameAct;
    c[ImGuiCol_TitleBg]               = ImVec4(0.071f, 0.078f, 0.098f, 1.0f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.110f, 0.122f, 0.153f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.071f, 0.078f, 0.098f, 1.0f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.071f, 0.078f, 0.098f, 1.0f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = kFrameHov;
    c[ImGuiCol_ScrollbarGrabHovered]  = kFrameAct;
    c[ImGuiCol_ScrollbarGrabActive]   = kAccent;
    c[ImGuiCol_CheckMark]             = kAccentAct;
    c[ImGuiCol_SliderGrab]            = kAccent;
    c[ImGuiCol_SliderGrabActive]      = kAccentAct;
    c[ImGuiCol_Button]                = kFrame;
    c[ImGuiCol_ButtonHovered]         = kFrameHov;
    c[ImGuiCol_ButtonActive]          = kFrameAct;
    c[ImGuiCol_Header]                = kHeader;
    c[ImGuiCol_HeaderHovered]         = kFrameHov;
    c[ImGuiCol_HeaderActive]          = kFrameAct;
    c[ImGuiCol_Separator]             = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    c[ImGuiCol_SeparatorHovered]      = kAccent;
    c[ImGuiCol_SeparatorActive]       = kAccentAct;
    c[ImGuiCol_ResizeGrip]            = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    c[ImGuiCol_ResizeGripHovered]     = kAccent;
    c[ImGuiCol_ResizeGripActive]      = kAccentAct;
    c[ImGuiCol_Tab]                   = ImVec4(0.110f, 0.122f, 0.153f, 1.0f);
    c[ImGuiCol_TabHovered]            = kFrameAct;
    c[ImGuiCol_TabActive]             = kHeader;
    c[ImGuiCol_TabUnfocused]          = ImVec4(0.090f, 0.098f, 0.122f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive]    = kHeader;
    c[ImGuiCol_PlotLines]             = kAccent;
    c[ImGuiCol_PlotLinesHovered]      = kAccentAct;
    c[ImGuiCol_PlotHistogram]         = kAccent;
    c[ImGuiCol_PlotHistogramHovered]  = kAccentAct;
    c[ImGuiCol_TextSelectedBg]        = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f);
    c[ImGuiCol_DragDropTarget]        = kAccentAct;
    c[ImGuiCol_NavHighlight]          = kAccent;
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
}
} // namespace agentbiosim::ui
