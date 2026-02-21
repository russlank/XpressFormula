// ViewTransform.h - Handles world ↔ screen coordinate mapping with zoom and pan.
#pragma once

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

    // --- World coordinate of the viewport center ---
    double centerX = 0.0;
    double centerY = 0.0;

    // --- Pixels per world unit (zoom level) ---
    double scaleX = 60.0;
    double scaleY = 60.0;

    // --- Plot area in screen coordinates ---
    float screenWidth   = 800.0f;
    float screenHeight  = 600.0f;
    float screenOriginX = 0.0f;   // top-left X of the plot area
    float screenOriginY = 0.0f;   // top-left Y of the plot area

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
