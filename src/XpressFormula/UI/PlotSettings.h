// PlotSettings.h - Shared plotting settings for 2D and 3D render modes.
#pragma once

namespace XpressFormula::UI {

enum class XYRenderMode {
    Surface3D,
    Heatmap2D
};

struct PlotSettings {
    XYRenderMode xyRenderMode = XYRenderMode::Surface3D;
    bool optimizeRendering = true;

    // 3D camera controls for z=f(x,y).
    //float azimuthDeg = 40.0f;
    //float elevationDeg = 30.0f;
    //float zScale = 1.0f;
    //int   surfaceResolution = 36;
    //float surfaceOpacity = 0.82f;
    //float wireThickness = 1.0f;
    //bool  showSurfaceEnvelope = true;
    //float envelopeThickness = 1.25f;
    //bool  showDimensionArrows = true;
    //bool  autoRotate = false;
    //float autoRotateSpeedDegPerSec = 20.0f;

    float azimuthDeg = 30.0f;
    float elevationDeg = -60.0f;
    float zScale = 1.5f;
    int   surfaceResolution = 50;
    float surfaceOpacity = 0.80f;
    float wireThickness = 2.0f;
    bool  showSurfaceEnvelope = true;
    float envelopeThickness = 2.00f;
    bool  showDimensionArrows = true;
    bool  autoRotate = false;
    float autoRotateSpeedDegPerSec = 20.0f;

    // Heatmap and scalar-field alpha.
    float heatmapOpacity = 0.62f;
};

} // namespace XpressFormula::UI
