#include "widgets.h"
#include "theme.h"
#include <cmath>
#include <algorithm>

namespace {

ImU32 rgba(const float c[4], float alpha) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(alpha * 255));
}

} // namespace

namespace widgets {

bool SmoothSliderInt(const char *id, int *value, int vMin, int vMax, float width, float dt) {
    if (vMax <= vMin) vMax = vMin + 1;

    ImGui::PushID(id);

    const float height = 30.0f;
    const float thumbR = 9.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##track", ImVec2(width, height));
    const bool held = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();

    const float x0 = origin.x + thumbR;
    const float x1 = origin.x + width - thumbR;
    const float cy = origin.y + height * 0.5f;
    const float span = std::max(1.0f, x1 - x0);

    bool changed = false;
    if (held) {
        float t = (ImGui::GetIO().MousePos.x - x0) / span;
        t = std::clamp(t, 0.0f, 1.0f);
        int next = vMin + (int)std::lround(t * (float)(vMax - vMin));
        if (next != *value) {
            *value = next;
            changed = true;
        }
    }

    *value = std::clamp(*value, vMin, vMax);
    const float target = (float)(*value - vMin) / (float)(vMax - vMin);

    // Persist the eased position per-widget in ImGui's own state storage so
    // the thumb travels to a new step rather than teleporting to it.
    float *shown = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("eased"), target);
    const float rate = 18.0f;
    *shown += (target - *shown) * (1.0f - std::exp(-rate * std::max(dt, 0.0001f)));
    if (std::fabs(target - *shown) < 0.0005f) *shown = target;

    const theme::Palette &pal = theme::kPalette;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float thumbX = x0 + *shown * span;

    // track
    dl->AddLine(ImVec2(x0, cy), ImVec2(x1, cy), rgba(pal.greenFaint, 1.0f), 5.0f);
    // filled portion
    dl->AddLine(ImVec2(x0, cy), ImVec2(thumbX, cy), rgba(pal.greenDim, 1.0f), 5.0f);

    // step ticks, brightening as the thumb passes them
    const int steps = vMax - vMin;
    if (steps <= 32) {
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / (float)steps;
            float tx = x0 + t * span;
            bool passed = t <= *shown + 0.001f;
            float alpha = passed ? 0.85f : 0.30f;
            if (std::fabs(tx - thumbX) < thumbR) continue; // don't draw under the thumb
            dl->AddCircleFilled(ImVec2(tx, cy), 1.7f, rgba(passed ? pal.green : pal.textDim, alpha));
        }
    }

    // thumb
    float r = thumbR + (held ? 1.5f : (hovered ? 0.8f : 0.0f));
    dl->AddCircleFilled(ImVec2(thumbX, cy), r + 3.0f, rgba(pal.green, 0.18f));
    dl->AddCircleFilled(ImVec2(thumbX, cy), r, rgba(pal.green, 1.0f));

    ImGui::PopID();
    return changed;
}

} // namespace widgets
