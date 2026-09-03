#include "theme.h"
#include "imgui.h"

namespace theme {

const Palette kPalette = {
    {0.02f, 0.03f, 0.024f, 1.0f},   // bg
    {0.04f, 0.06f, 0.047f, 1.0f},   // panel
    {0.22f, 1.0f, 0.416f, 1.0f},    // green  #39ff6a
    {0.09f, 0.64f, 0.29f, 1.0f},    // greenDim #17a34a
    {0.05f, 0.29f, 0.15f, 1.0f},    // greenFaint #0d4a26
    {1.0f, 0.30f, 0.30f, 1.0f},     // red
    {0.66f, 1.0f, 0.81f, 1.0f},     // text
    {0.44f, 0.68f, 0.51f, 1.0f},    // textDim
};

static ImVec4 v(const float c[4], float a = -1.0f) {
    return ImVec4(c[0], c[1], c[2], a < 0 ? c[3] : a);
}

void apply() {
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 6.0f;
    s.PopupRounding = 8.0f;
    s.GrabRounding = 999.0f;
    s.ScrollbarRounding = 999.0f;
    s.TabRounding = 6.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.WindowPadding = ImVec2(18, 18);
    s.FramePadding = ImVec2(10, 8);
    s.ItemSpacing = ImVec2(10, 10);
    s.GrabMinSize = 16.0f;

    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text] = v(kPalette.text);
    c[ImGuiCol_TextDisabled] = v(kPalette.textDim);
    c[ImGuiCol_WindowBg] = v(kPalette.bg);
    c[ImGuiCol_ChildBg] = v(kPalette.panel, 0.6f);
    c[ImGuiCol_PopupBg] = v(kPalette.panel, 0.98f);
    c[ImGuiCol_Border] = v(kPalette.greenFaint, 0.7f);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = v(kPalette.panel);
    c[ImGuiCol_FrameBgHovered] = v(kPalette.greenFaint, 0.6f);
    c[ImGuiCol_FrameBgActive] = v(kPalette.greenFaint, 0.9f);
    c[ImGuiCol_TitleBg] = v(kPalette.bg);
    c[ImGuiCol_TitleBgActive] = v(kPalette.bg);
    c[ImGuiCol_TitleBgCollapsed] = v(kPalette.bg);
    c[ImGuiCol_MenuBarBg] = v(kPalette.panel);
    c[ImGuiCol_ScrollbarBg] = v(kPalette.bg);
    c[ImGuiCol_ScrollbarGrab] = v(kPalette.greenFaint);
    c[ImGuiCol_ScrollbarGrabHovered] = v(kPalette.greenDim);
    c[ImGuiCol_ScrollbarGrabActive] = v(kPalette.green);
    c[ImGuiCol_CheckMark] = v(kPalette.green);
    c[ImGuiCol_SliderGrab] = v(kPalette.greenDim);
    c[ImGuiCol_SliderGrabActive] = v(kPalette.green);
    c[ImGuiCol_Button] = v(kPalette.greenFaint);
    c[ImGuiCol_ButtonHovered] = v(kPalette.greenDim);
    c[ImGuiCol_ButtonActive] = v(kPalette.green, 0.85f);
    c[ImGuiCol_Header] = v(kPalette.greenFaint);
    c[ImGuiCol_HeaderHovered] = v(kPalette.greenDim);
    c[ImGuiCol_HeaderActive] = v(kPalette.green);
    c[ImGuiCol_Separator] = v(kPalette.greenFaint);
    c[ImGuiCol_SeparatorHovered] = v(kPalette.greenDim);
    c[ImGuiCol_SeparatorActive] = v(kPalette.green);
    c[ImGuiCol_ResizeGrip] = v(kPalette.greenFaint, 0.3f);
    c[ImGuiCol_ResizeGripHovered] = v(kPalette.greenDim);
    c[ImGuiCol_ResizeGripActive] = v(kPalette.green);
    c[ImGuiCol_Tab] = v(kPalette.panel);
    c[ImGuiCol_TabHovered] = v(kPalette.greenDim);
    c[ImGuiCol_TabActive] = v(kPalette.greenFaint);
    c[ImGuiCol_PlotLines] = v(kPalette.green);
    c[ImGuiCol_PlotHistogram] = v(kPalette.green);
    c[ImGuiCol_TextSelectedBg] = v(kPalette.greenFaint, 0.6f);
    c[ImGuiCol_DragDropTarget] = v(kPalette.green);
    c[ImGuiCol_NavHighlight] = v(kPalette.green);
}

}
