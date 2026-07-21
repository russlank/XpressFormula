// PlotSettings.h - Domain model for 2D/3D plot display settings.
#pragma once

#include "SceneSummary.h"

#include <array>

namespace XpressFormula::Model {

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

template <typename T>
struct PlotValueRange {
    T min;
    T max;
    T fallback;
};

struct PlotDefaults {
    XYRenderModePreference xyRenderModePreference = XYRenderModePreference::Auto;
    PlotHudMode hudMode = PlotHudMode::Minimal;
    bool optimizeRendering = true;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    float azimuthDeg = 30.0f;
    float elevationDeg = -60.0f;
    float zScale = 1.5f;
    int surfaceResolution = 50;
    int implicitSurfaceResolution = 64;
    float surfaceOpacity = 1.0f;
    float wireOpacity = 0.25f;
    float wireThickness = 1.0f;
    int wireStride = 2;
    bool showSurfaceEnvelope = true;
    float envelopeThickness = 2.0f;
    bool showAxisTriad = false;
    bool autoRotate = false;
    float autoRotateSpeedDegPerSec = 20.0f;
    float heatmapOpacity = 0.62f;
    std::array<float, 4> backgroundColor = { 0.098f, 0.098f, 0.118f, 1.0f };
};

inline constexpr PlotDefaults kPlotDefaults{};
inline constexpr float kDefaultAzimuthDeg = kPlotDefaults.azimuthDeg;
inline constexpr float kDefaultElevationDeg = kPlotDefaults.elevationDeg;
inline constexpr float kDefaultZScale = kPlotDefaults.zScale;
inline constexpr int   kDefaultSurfaceResolution = kPlotDefaults.surfaceResolution;
inline constexpr int   kDefaultImplicitSurfaceResolution = kPlotDefaults.implicitSurfaceResolution;
inline constexpr float kDefaultSurfaceOpacity = kPlotDefaults.surfaceOpacity;
inline constexpr float kDefaultWireOpacity = kPlotDefaults.wireOpacity;
inline constexpr float kDefaultWireThickness = kPlotDefaults.wireThickness;
inline constexpr int   kDefaultWireStride = kPlotDefaults.wireStride;
inline constexpr float kDefaultEnvelopeThickness = kPlotDefaults.envelopeThickness;
inline constexpr float kDefaultAutoRotateSpeedDegPerSec = kPlotDefaults.autoRotateSpeedDegPerSec;
inline constexpr float kDefaultHeatmapOpacity = kPlotDefaults.heatmapOpacity;

struct PlotLimits {
    PlotValueRange<float> azimuthDeg{ -180.0f, 180.0f, kDefaultAzimuthDeg };
    PlotValueRange<float> elevationDeg{ -85.0f, 85.0f, kDefaultElevationDeg };
    PlotValueRange<float> zScale{ 0.1f, 8.0f, kDefaultZScale };
    PlotValueRange<int> surfaceResolution{ 16, 256, kDefaultSurfaceResolution };
    PlotValueRange<int> implicitSurfaceResolution{ 16, 192, kDefaultImplicitSurfaceResolution };
    PlotValueRange<float> surfaceOpacity{ 0.0f, 1.0f, kDefaultSurfaceOpacity };
    PlotValueRange<float> wireOpacity{ 0.0f, 1.0f, kDefaultWireOpacity };
    PlotValueRange<float> wireThickness{ 0.05f, 8.0f, kDefaultWireThickness };
    PlotValueRange<int> wireStride{ 1, 16, kDefaultWireStride };
    PlotValueRange<float> envelopeThickness{ 0.05f, 8.0f, kDefaultEnvelopeThickness };
    PlotValueRange<float> autoRotateSpeedDegPerSec{
        2.0f,
        90.0f,
        kDefaultAutoRotateSpeedDegPerSec
    };
    PlotValueRange<float> heatmapOpacity{ 0.0f, 1.0f, kDefaultHeatmapOpacity };
    PlotValueRange<float> wireThicknessScale{ 0.25f, 3.0f, 1.0f };
};

inline constexpr PlotLimits kPlotLimits{};

struct PlotSettings {
    XYRenderModePreference xyRenderModePreference = kPlotDefaults.xyRenderModePreference;
    PlotHudMode hudMode = kPlotDefaults.hudMode;
    bool optimizeRendering = kPlotDefaults.optimizeRendering;
    bool showGrid = kPlotDefaults.showGrid;
    bool showCoordinates = kPlotDefaults.showCoordinates;
    bool showWires = kPlotDefaults.showWires;

    [[nodiscard]] XYRenderMode resolveXYRenderMode(const SceneSummary& scene) const;
    [[nodiscard]] bool effectiveShowAxisTriad() const;
    bool applyCoordinateOverlayPolicy();

    float azimuthDeg = kPlotDefaults.azimuthDeg;
    float elevationDeg = kPlotDefaults.elevationDeg;
    float zScale = kPlotDefaults.zScale;
    int surfaceResolution = kPlotDefaults.surfaceResolution;
    int implicitSurfaceResolution = kPlotDefaults.implicitSurfaceResolution;
    float surfaceOpacity = kPlotDefaults.surfaceOpacity;
    float wireOpacity = kPlotDefaults.wireOpacity;
    float wireThickness = kPlotDefaults.wireThickness;
    int wireStride = kPlotDefaults.wireStride;
    bool showSurfaceEnvelope = kPlotDefaults.showSurfaceEnvelope;
    float envelopeThickness = kPlotDefaults.envelopeThickness;
    bool showAxisTriad = kPlotDefaults.showAxisTriad;
    bool autoRotate = kPlotDefaults.autoRotate;
    float autoRotateSpeedDegPerSec = kPlotDefaults.autoRotateSpeedDegPerSec;
    float heatmapOpacity = kPlotDefaults.heatmapOpacity;
};

} // namespace XpressFormula::Model
