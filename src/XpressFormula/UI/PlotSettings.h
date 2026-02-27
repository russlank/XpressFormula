// PlotSettings.h - Shared plotting settings for 2D and 3D render modes.
#pragma once

namespace XpressFormula::UI {

enum class XYRenderMode {
    Surface3D,
    Heatmap2D
};

enum class XYRenderModePreference {
    Auto,
    Force3D,
    Force2D
};

struct PlotSettings {
    XYRenderModePreference xyRenderModePreference = XYRenderModePreference::Auto;
    bool optimizeRendering = true;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;

    [[nodiscard]] XYRenderMode resolveXYRenderMode(bool hasVisible2DFormula,
                                                   bool hasVisible3DFormula) const {
        switch (xyRenderModePreference) {
            case XYRenderModePreference::Force3D:
                return XYRenderMode::Surface3D;
            case XYRenderModePreference::Force2D:
                return XYRenderMode::Heatmap2D;
            case XYRenderModePreference::Auto:
            default:
                // Auto mode keeps 2D and 3D mutually exclusive:
                // mixed visible content defaults to 2D, while purely-3D content activates 3D.
                return (hasVisible3DFormula && !hasVisible2DFormula)
                    ? XYRenderMode::Surface3D
                    : XYRenderMode::Heatmap2D;
        }
    }

    // 3D camera controls for z=f(x,y).
    //float azimuthDeg = 40.0f;
    //float elevationDeg = 30.0f;
    //float zScale = 1.0f;
    //int   surfaceResolution = 36;
    //float surfaceOpacity = 0.82f;
    //float wireThickness = 1.0f;
    //bool  showSurfaceEnvelope = true;
    //float envelopeThickness = 1.25f;
    //bool  showAxisTriad = true;
    //bool  autoRotate = false;
    //float autoRotateSpeedDegPerSec = 20.0f;

    float azimuthDeg = 30.0f;
    float elevationDeg = -60.0f;
    float zScale = 1.5f;
    int   surfaceResolution = 50;
    int   implicitSurfaceResolution = 64;
    float surfaceOpacity = 0.80f;
    float wireThickness = 2.0f;
    bool  showSurfaceEnvelope = true;
    float envelopeThickness = 2.00f;
    bool  showAxisTriad = true;
    bool  autoRotate = false;
    float autoRotateSpeedDegPerSec = 20.0f;

    // Heatmap and scalar-field alpha.
    float heatmapOpacity = 0.62f;
};

} // namespace XpressFormula::UI
