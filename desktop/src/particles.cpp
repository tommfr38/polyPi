#include "particles.h"
#include <random>
#include <cmath>
#include <algorithm>

static float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float x = t - 1.0f;
    return 1.0f + c3 * x * x * x + c1 * x * x;
}

void ParticleField::build(ImVec2 boundsMin, ImVec2 boundsMax, int count) {
    boundsMin_ = boundsMin;
    boundsMax_ = boundsMax;
    float w = boundsMax.x - boundsMin.x;
    float h = boundsMax.y - boundsMin.y;
    float cx = boundsMin.x + w * 0.5f;
    float cy = boundsMin.y + h * 0.5f;

    // Geometric pi (π) glyph: a top bar + two legs, built as target sample points.
    float barH = h * 0.16f;
    float barY0 = boundsMin.y + h * 0.14f;
    float barY1 = barY0 + barH;
    float barX0 = boundsMin.x + w * 0.12f;
    float barX1 = boundsMax.x - w * 0.12f;

    float legW = w * 0.14f;
    float legY1 = boundsMax.y - h * 0.10f;
    float leg1X0 = barX0 + w * 0.05f;
    float leg2X0 = barX1 - w * 0.05f - legW;

    std::mt19937 rng(1234567u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    std::vector<ImVec2> targets;
    targets.reserve(count);

    int barCount = count * 40 / 100;
    int legCount = (count - barCount) / 2;

    for (int i = 0; i < barCount; i++) {
        float x = barX0 + u01(rng) * (barX1 - barX0);
        float y = barY0 + u01(rng) * (barY1 - barY0);
        targets.push_back({x, y});
    }
    for (int i = 0; i < legCount; i++) {
        float x = leg1X0 + u01(rng) * legW;
        float y = barY1 + u01(rng) * (legY1 - barY1);
        // slight outward flare near the bottom
        float t = (y - barY1) / std::max(1.0f, (legY1 - barY1));
        x -= t * t * w * 0.03f;
        targets.push_back({x, y});
    }
    for (int i = 0; i < count - barCount - legCount; i++) {
        float x = leg2X0 + u01(rng) * legW;
        float y = barY1 + u01(rng) * (legY1 - barY1);
        float t = (y - barY1) / std::max(1.0f, (legY1 - barY1));
        x += t * t * w * 0.03f;
        targets.push_back({x, y});
    }

    std::shuffle(targets.begin(), targets.end(), rng);

    particles_.clear();
    particles_.reserve(targets.size());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distDist(0.7f, 1.4f);
    float maxDim = std::max(w, h);

    for (size_t i = 0; i < targets.size(); i++) {
        Particle p;
        float ang = angleDist(rng);
        float dist = distDist(rng) * maxDim;
        p.start = ImVec2(cx + std::cos(ang) * dist, cy + std::sin(ang) * dist);
        p.target = targets[i];
        p.pos = p.start;
        p.delayFrac = (float)i / (float)targets.size();
        p.jitterPhase = u01(rng) * 6.28318f;
        p.radius = 1.3f + u01(rng) * 1.3f;
        particles_.push_back(p);
    }
}

void ParticleField::setState(int state) {
    if (state == 2 && state_ != 2) pulseT_ = 0.0f;
    state_ = state;
}

void ParticleField::setProgress(float p) { progress_ = std::clamp(p, 0.0f, 1.0f); }

void ParticleField::update(float dtSeconds, double nowSeconds) {
    if (state_ == 0) {
        for (auto &p : particles_) {
            float drift = std::sin((float)nowSeconds * 0.6f + p.jitterPhase) * 1.5f;
            p.pos = ImVec2(p.start.x, p.start.y + drift);
        }
    } else if (state_ == 1) {
        for (auto &p : particles_) {
            // each particle "arrives" once overall progress passes its stagger slot,
            // with a short local easing window so it doesn't just teleport in.
            float window = 0.12f;
            float local = (progress_ - p.delayFrac * (1.0f - window)) / window;
            local = std::clamp(local, 0.0f, 1.0f);
            float eased = easeOutBack(local);
            float bx = p.start.x + (p.target.x - p.start.x) * eased;
            float by = p.start.y + (p.target.y - p.start.y) * eased;
            if (local >= 1.0f) {
                float jx = std::sin((float)nowSeconds * 4.0f + p.jitterPhase) * 0.5f;
                float jy = std::cos((float)nowSeconds * 5.2f + p.jitterPhase) * 0.5f;
                bx += jx;
                by += jy;
            }
            p.pos = ImVec2(bx, by);
        }
    } else if (state_ == 2) {
        pulseT_ = std::min(1.0f, pulseT_ + dtSeconds / 0.9f);
        for (auto &p : particles_) {
            float jx = std::sin((float)nowSeconds * 4.0f + p.jitterPhase) * 0.5f;
            float jy = std::cos((float)nowSeconds * 5.2f + p.jitterPhase) * 0.5f;
            p.pos = ImVec2(p.target.x + jx, p.target.y + jy);
        }
    }
}

void ParticleField::draw(ImDrawList *dl, ImU32 colorRGB) const {
    float glow = 0.0f;
    if (state_ == 2) {
        glow = std::sin(pulseT_ * 3.14159f) * 3.0f;
    }
    for (auto &p : particles_) {
        float r = p.radius + glow;
        // cheap glow: faint larger circle behind a bright core
        dl->AddCircleFilled(p.pos, r * 2.2f, IM_COL32(57, 255, 106, 30));
        dl->AddCircleFilled(p.pos, r, colorRGB);
    }
}
