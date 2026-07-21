// PlotPolicy.h - Centralized plot defaults, ranges, labels, and effective settings.
#pragma once

#include "PlotSettings.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Model {

struct ValidationResult {
    bool valid = true;
    std::vector<std::string> messages;
};

struct PlotRenderOverrides {
    bool active = false;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showEnvelope = true;
    bool showAxisTriad = true;
    bool showHud = true;
    bool showCanvasBorder = true;
    std::array<float, 4> backgroundColor = kPlotDefaults.backgroundColor;
};

struct PlotQualityDecision {
    bool overrideQuality = false;
    int surfaceResolution = kDefaultSurfaceResolution;
    int implicitSurfaceResolution = kDefaultImplicitSurfaceResolution;
    float wireThicknessScale = 1.0f;
    bool interactiveThrottle = false;
};

struct EffectivePlotSettings {
    XYRenderMode renderMode = XYRenderMode::Heatmap2D;
    bool is3DMode = false;
    bool optimizeRendering = true;

    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showEnvelope = true;
    bool showAxisTriad = false;
    bool showHud = true;
    bool showCanvasBorder = true;
    std::array<float, 4> backgroundColor = kPlotDefaults.backgroundColor;

    float azimuthDeg = kDefaultAzimuthDeg;
    float elevationDeg = kDefaultElevationDeg;
    float zScale = kDefaultZScale;
    bool autoRotate = false;
    float autoRotateSpeedDegPerSec = kDefaultAutoRotateSpeedDegPerSec;

    int surfaceResolution = kDefaultSurfaceResolution;
    int implicitSurfaceResolution = kDefaultImplicitSurfaceResolution;
    float surfaceOpacity = kDefaultSurfaceOpacity;
    float wireOpacity = kDefaultWireOpacity;
    float wireThickness = kDefaultWireThickness;
    int wireStride = kDefaultWireStride;
    float envelopeThickness = kDefaultEnvelopeThickness;
    float heatmapOpacity = kDefaultHeatmapOpacity;
};

[[nodiscard]] const PlotDefaults& plotDefaults();
[[nodiscard]] const PlotLimits& plotLimits();

[[nodiscard]] PlotSettings defaultPlotSettings();
void normalizePlotSettings(PlotSettings& settings);
[[nodiscard]] ValidationResult validatePlotSettings(const PlotSettings& settings);

[[nodiscard]] bool isAxisTriadVisible(bool showCoordinates, bool showAxisTriad);
bool resolveCoordinateOverlayPolicy(bool showCoordinates, bool& showAxisTriad);

[[nodiscard]] float clampWireOpacity(float value);
[[nodiscard]] int clampWireStride(int value);
[[nodiscard]] int clampSurfaceResolution(int value);
[[nodiscard]] int clampImplicitSurfaceResolution(int value);
[[nodiscard]] float clampWireThicknessScale(float value);

[[nodiscard]] XYRenderMode resolveXYRenderMode(XYRenderModePreference preference,
                                               const SceneSummary& scene);
[[nodiscard]] EffectivePlotSettings resolveEffectivePlotSettings(
    const PlotSettings& base,
    const PlotRenderOverrides& overrides,
    const PlotQualityDecision& quality,
    const SceneSummary& scene);

[[nodiscard]] std::string_view toStorageName(XYRenderModePreference preference);
[[nodiscard]] std::string_view toDisplayLabel(XYRenderModePreference preference);
bool parseXYRenderModePreferenceStorageName(std::string_view text,
                                            XYRenderModePreference& preference);

[[nodiscard]] std::string_view toStorageName(PlotHudMode mode);
[[nodiscard]] std::string_view toDisplayLabel(PlotHudMode mode);
bool parsePlotHudModeStorageName(std::string_view text, PlotHudMode& mode);

[[nodiscard]] const char* plotHudModeLabel(PlotHudMode mode);

} // namespace XpressFormula::Model
