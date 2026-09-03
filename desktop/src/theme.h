#pragma once

namespace theme {
void apply();

// shared palette (RGBA 0..1) used outside ImGui style too (particles etc.)
struct Palette {
    float bg[4];
    float panel[4];
    float green[4];
    float greenDim[4];
    float greenFaint[4];
    float red[4];
    float text[4];
    float textDim[4];
};

extern const Palette kPalette;
}
