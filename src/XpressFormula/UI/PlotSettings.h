// PlotSettings.h - Shared plotting settings for 2D and 3D render modes.
#pragma once

#include <algorithm>

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

enum class PlotHudMode {
    Off,
    Minimal,
    Detailed,
    OnlyWhileInteracting
};

inline constexpr float kDefaultAzimuthDeg = 30.0f;
inline constexpr float kDefaultElevationDeg = -60.0f;
inline constexpr float kDefaultZScale = 1.5f;
inline constexpr int   kDefaultSurfaceResolution = 50;
inline constexpr int   kDefaultImplicitSurfaceResolution = 64;
inline constexpr float kDefaultSurfaceOpacity = 1.00f;
inline constexpr float kDefaultWireOpacity = 0.25f;
inline constexpr float kDefaultWireThickness = 1.0f;
inline constexpr int   kDefaultWireStride = 2;
inline constexpr float kDefaultEnvelopeThickness = 2.0f;
inline constexpr float kDefaultAutoRotateSpeedDegPerSec = 20.0f;
inline constexpr float kDefaultHeatmapOpacity = 0.62f;

[[nodiscard]] inline bool isAxisTriadVisible(bool showCoordinates, bool showAxisTriad) {
    return showAxisTriad && !showCoordinates;
}

inline bool resolveCoordinateOverlayPolicy(bool showCoordinates, bool& showAxisTriad) {
    if (!showCoordinates || !showAxisTriad) {
        return false;
    }

    showAxisTriad = false;
    return true;
}

[[nodiscard]] inline const char* plotHudModeLabel(PlotHudMode mode) {
    switch (mode) {
        case PlotHudMode::Off:
            return "Off";
        case PlotHudMode::Minimal:
            return "Minimal";
        case PlotHudMode::Detailed:
            return "Detailed";
        case PlotHudMode::OnlyWhileInteracting:
            return "Only While Interacting";
        default:
            return "Minimal";
    }
}

[[nodiscard]] inline float clampWireOpacity(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] inline int clampWireStride(int value) {
    return std::clamp(value, 1, 16);
}

struct PlotSettings {
    XYRenderModePreference xyRenderModePreference = XYRenderModePreference::Auto;
    PlotHudMode hudMode = PlotHudMode::Minimal;
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

    [[nodiscard]] bool effectiveShowAxisTriad() const {
        return isAxisTriadVisible(showCoordinates, showAxisTriad);
    }

    bool applyCoordinateOverlayPolicy() {
        return resolveCoordinateOverlayPolicy(showCoordinates, showAxisTriad);
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
    //bool  showAxisTriad = false;
    //bool  autoRotate = false;
    //float autoRotateSpeedDegPerSec = 20.0f;

    float azimuthDeg = kDefaultAzimuthDeg;
    float elevationDeg = kDefaultElevationDeg;
    float zScale = kDefaultZScale;
    int   surfaceResolution = kDefaultSurfaceResolution;
    int   implicitSurfaceResolution = kDefaultImplicitSurfaceResolution;
    float surfaceOpacity = kDefaultSurfaceOpacity;
    float wireOpacity = kDefaultWireOpacity;
    float wireThickness = kDefaultWireThickness;
    int   wireStride = kDefaultWireStride;
    bool  showSurfaceEnvelope = true;
    float envelopeThickness = kDefaultEnvelopeThickness;
    bool  showAxisTriad = false;
    bool  autoRotate = false;
    float autoRotateSpeedDegPerSec = kDefaultAutoRotateSpeedDegPerSec;

    // Heatmap and scalar-field alpha.
    float heatmapOpacity = kDefaultHeatmapOpacity;
};

} // namespace XpressFormula::UI
