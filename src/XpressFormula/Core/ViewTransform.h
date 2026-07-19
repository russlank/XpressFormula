// ViewTransform.h - Handles world ↔ screen coordinate mapping with zoom and pan.
#pragma once

#include "../Model/ViewState.h"

namespace XpressFormula::Core {

/// Simple 2D float vector used for screen coordinates.
struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
};

/// Maps between world-space coordinates and screen-space pixel coordinates.
/// The world origin is drawn at the center of the plot area.
class ViewTransform {
public:
    ViewTransform();
    ViewTransform(const Model::ViewState& state, const Model::Viewport& viewport);

    Model::ViewState state;
    Model::Viewport viewport;

    // Coordinate conversions
    Vec2   worldToScreen(double wx, double wy) const;
    void   screenToWorld(float sx, float sy, double& wx, double& wy) const;

    // Zoom helpers
    void zoomAll(double factor);
    void zoomX(double factor);
    void zoomY(double factor);

    // Pan by world units
    void pan(double dx, double dy);
    // Pan by screen pixels
    void panPixels(float dx, float dy);

    // Reset to default view
    void reset();

    // Visible world-space range
    double worldXMin() const;
    double worldXMax() const;
    double worldYMin() const;
    double worldYMax() const;

    // Choose a "nice" grid spacing for the current scale
    double gridSpacingX() const;
    double gridSpacingY() const;

private:
    double niceGridSpacing(double pixelsPerUnit) const;
    static constexpr double DEFAULT_SCALE = 60.0;
    static constexpr double MIN_SCALE     = 0.1;
    static constexpr double MAX_SCALE     = 100000.0;
};

} // namespace XpressFormula::Core
