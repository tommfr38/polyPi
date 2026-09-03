#pragma once
#include <vector>
#include "imgui.h"

// A field of particles that assembles into a pi (π) glyph. Formation is
// driven by an external progress fraction [0,1] so it can track the real
// binary-splitting progress on desktop (unlike the web build, which fakes a
// fixed-duration animation since it never gets true progress).
class ParticleField {
public:
    void build(ImVec2 boundsMin, ImVec2 boundsMax, int count = 2200);

    // state: 0=idle drift, 1=forming/holding (driven by progress), 2=pulse (done flash)
    void setState(int state);
    void setProgress(float p); // 0..1, only used while state==1

    void update(float dtSeconds, double nowSeconds);
    void draw(ImDrawList *dl, ImU32 colorRGB) const;

private:
    struct Particle {
        ImVec2 start;
        ImVec2 target;
        ImVec2 pos;
        float delayFrac;   // 0..1 offset into the formation order
        float jitterPhase;
        float radius;
    };

    std::vector<Particle> particles_;
    ImVec2 boundsMin_{0, 0}, boundsMax_{0, 0};
    int state_ = 0;
    float progress_ = 0.0f;
    float pulseT_ = 0.0f;
};
