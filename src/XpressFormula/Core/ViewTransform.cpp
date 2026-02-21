// ViewTransform.cpp - View-transform implementation.
#include "ViewTransform.h"
#include <cmath>
#include <algorithm>

namespace XpressFormula::Core {

ViewTransform::ViewTransform() = default;

Vec2 ViewTransform::worldToScreen(double wx, double wy) const {
    float sx = screenOriginX + screenWidth  * 0.5f
             + static_cast<float>((wx - centerX) * scaleX);
    float sy = screenOriginY + screenHeight * 0.5f
             - static_cast<float>((wy - centerY) * scaleY);
    return { sx, sy };
}

void ViewTransform::screenToWorld(float sx, float sy,
                                  double& wx, double& wy) const {
    wx =  (sx - screenOriginX - screenWidth  * 0.5f) / scaleX + centerX;
    wy = -(sy - screenOriginY - screenHeight * 0.5f) / scaleY + centerY;
}

void ViewTransform::zoomAll(double factor) {
    scaleX = std::clamp(scaleX * factor, MIN_SCALE, MAX_SCALE);
    scaleY = std::clamp(scaleY * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::zoomX(double factor) {
    scaleX = std::clamp(scaleX * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::zoomY(double factor) {
    scaleY = std::clamp(scaleY * factor, MIN_SCALE, MAX_SCALE);
}

void ViewTransform::pan(double dx, double dy) {
    centerX += dx;
    centerY += dy;
}

void ViewTransform::panPixels(float dx, float dy) {
    centerX -= dx / scaleX;
    centerY += dy / scaleY;
}

void ViewTransform::reset() {
    centerX = 0.0;
    centerY = 0.0;
    scaleX  = DEFAULT_SCALE;
    scaleY  = DEFAULT_SCALE;
}

double ViewTransform::worldXMin() const {
    return centerX - (screenWidth  * 0.5) / scaleX;
}
double ViewTransform::worldXMax() const {
    return centerX + (screenWidth  * 0.5) / scaleX;
}
double ViewTransform::worldYMin() const {
    return centerY - (screenHeight * 0.5) / scaleY;
}
double ViewTransform::worldYMax() const {
    return centerY + (screenHeight * 0.5) / scaleY;
}

double ViewTransform::gridSpacingX() const { return niceGridSpacing(scaleX); }
double ViewTransform::gridSpacingY() const { return niceGridSpacing(scaleY); }

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
