// ViewTransform.cpp - View-transform implementation.
#include "ViewTransform.h"
#include <cmath>
#include <algorithm>

namespace XpressFormula::Core {

ViewTransform::ViewTransform()
    : state{},
      viewport{0.0f, 0.0f, 800.0f, 600.0f} {
}

ViewTransform::ViewTransform(const Model::ViewState& stateIn,
                             const Model::Viewport& viewportIn)
    : state(stateIn),
      viewport(viewportIn) {
}

Vec2 ViewTransform::worldToScreen(double wx, double wy) const {
    float sx = viewport.originX + viewport.width  * 0.5f
             + static_cast<float>((wx - state.centerX) * state.scaleX);
    float sy = viewport.originY + viewport.height * 0.5f
             - static_cast<float>((wy - state.centerY) * state.scaleY);
    return { sx, sy };
}

void ViewTransform::screenToWorld(float sx, float sy,
                                  double& wx, double& wy) const {
    wx =  (sx - viewport.originX - viewport.width  * 0.5f) / state.scaleX + state.centerX;
    wy = -(sy - viewport.originY - viewport.height * 0.5f) / state.scaleY + state.centerY;
}

void ViewTransform::zoomAll(double factor) {
    state.scaleX = std::clamp(state.scaleX * factor, MIN_SCALE, MAX_SCALE);
    state.scaleY = std::clamp(state.scaleY * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::zoomX(double factor) {
    state.scaleX = std::clamp(state.scaleX * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::zoomY(double factor) {
    state.scaleY = std::clamp(state.scaleY * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::pan(double dx, double dy) {
    state.centerX += dx;
    state.centerY += dy;
}

void ViewTransform::panPixels(float dx, float dy) {
    state.centerX -= dx / state.scaleX;
    state.centerY += dy / state.scaleY;
}

void ViewTransform::reset() {
    state.centerX = 0.0;
    state.centerY = 0.0;
    state.scaleX  = DEFAULT_SCALE;
    state.scaleY  = DEFAULT_SCALE;
}

double ViewTransform::worldXMin() const {
    return state.centerX - (viewport.width  * 0.5) / state.scaleX;
}
double ViewTransform::worldXMax() const {
    return state.centerX + (viewport.width  * 0.5) / state.scaleX;
}
double ViewTransform::worldYMin() const {
    return state.centerY - (viewport.height * 0.5) / state.scaleY;
}
double ViewTransform::worldYMax() const {
    return state.centerY + (viewport.height * 0.5) / state.scaleY;
}

double ViewTransform::gridSpacingX() const { return niceGridSpacing(state.scaleX); }
double ViewTransform::gridSpacingY() const { return niceGridSpacing(state.scaleY); }

double ViewTransform::niceGridSpacing(double pixelsPerUnit) const {
    // Aim for grid lines roughly every 80-150 pixels
    double target    = 100.0 / pixelsPerUnit;
    double magnitude = std::pow(10.0, std::floor(std::log10(target)));
    double norm      = target / magnitude;

    double nice;
    if      (norm < 1.5) nice = 1.0;
    else if (norm < 3.5) nice = 2.0;
    else if (norm < 7.5) nice = 5.0;
    else                 nice = 10.0;

    return nice * magnitude;
}

} // namespace XpressFormula::Core
