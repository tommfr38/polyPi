#pragma once
#include "imgui.h"

namespace widgets {

// A discrete slider whose thumb glides to its target instead of snapping
// between steps, with a tick dot per step. `dt` is the frame delta so the
// easing is frame-rate independent. Returns true when the value changed.
bool SmoothSliderInt(const char *id, int *value, int vMin, int vMax, float width, float dt);

}
