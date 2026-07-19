// PlotPolicy.cpp - Centralized plot settings policy implementation.
#include "PlotPolicy.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace XpressFormula::Model {
namespace {

template <typename T>
T clampFinite(T value, const PlotValueRange<T>& range) {
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
            return range.fallback;
        }
    }
    return std::clamp(value, range.min, range.max);
}

bool isKnown(XYRenderModePreference preference) {
    switch (preference) {
        case XYRenderModePreference::Auto:
        case XYRenderModePreference::Force3D:
        case XYRenderModePreference::Force2D:
            return true;
        default:
            return false;
    }
}

bool isKnown(PlotHudMode mode) {
    switch (mode) {
        case PlotHudMode::Off:
        case PlotHudMode::Minimal:
        case PlotHudMode::Detailed:
        case PlotHudMode::OnlyWhileInteracting:
            return true;
        default:
            return false;
    }
}

template <typename T>
void validateRange(ValidationResult& result,
                   std::string_view field,
                   T value,
                   const PlotValueRange<T>& range) {
    bool valid = value >= range.min && value <= range.max;
    if constexpr (std::is_floating_point_v<T>) {
        valid = valid && std::isfinite(value);
    }
    if (!valid) {
        result.valid = false;
        result.messages.emplace_back(std::string(field) + " is outside the plot policy range.");
    }
}

} // namespace

const PlotDefaults& plotDefaults() {
    static constexpr PlotDefaults defaults{};
    return defaults;
}

const PlotLimits& plotLimits() {
    static constexpr PlotLimits limits{};
    return limits;
}

PlotSettings defaultPlotSettings() {
    return PlotSettings{};
}

void normalizePlotSettings(PlotSettings& settings) {
    const PlotDefaults& defaults = plotDefaults();
    const PlotLimits& limits = plotLimits();

    if (!isKnown(settings.xyRenderModePreference)) {
        settings.xyRenderModePreference = defaults.xyRenderModePreference;
    }
    if (!isKnown(settings.hudMode)) {
        settings.hudMode = defaults.hudMode;
    }

    settings.azimuthDeg = clampFinite(settings.azimuthDeg, limits.azimuthDeg);
    settings.elevationDeg = clampFinite(settings.elevationDeg, limits.elevationDeg);
    settings.zScale = clampFinite(settings.zScale, limits.zScale);
    settings.surfaceResolution = clampFinite(settings.surfaceResolution, limits.surfaceResolution);
    settings.implicitSurfaceResolution =
        clampFinite(settings.implicitSurfaceResolution, limits.implicitSurfaceResolution);
    settings.surfaceOpacity = clampFinite(settings.surfaceOpacity, limits.surfaceOpacity);
    settings.wireOpacity = clampFinite(settings.wireOpacity, limits.wireOpacity);
    settings.wireThickness = clampFinite(settings.wireThickness, limits.wireThickness);
    settings.wireStride = clampFinite(settings.wireStride, limits.wireStride);
    settings.envelopeThickness = clampFinite(settings.envelopeThickness, limits.envelopeThickness);
    settings.autoRotateSpeedDegPerSec =
        clampFinite(settings.autoRotateSpeedDegPerSec, limits.autoRotateSpeedDegPerSec);
    settings.heatmapOpacity = clampFinite(settings.heatmapOpacity, limits.heatmapOpacity);
    settings.applyCoordinateOverlayPolicy();
}

ValidationResult validatePlotSettings(const PlotSettings& settings) {
    ValidationResult result;
    const PlotLimits& limits = plotLimits();

    if (!isKnown(settings.xyRenderModePreference)) {
        result.valid = false;
        result.messages.emplace_back("xyRenderModePreference is not a known value.");
    }
    if (!isKnown(settings.hudMode)) {
        result.valid = false;
        result.messages.emplace_back("hudMode is not a known value.");
    }

    validateRange(result, "azimuthDeg", settings.azimuthDeg, limits.azimuthDeg);
    validateRange(result, "elevationDeg", settings.elevationDeg, limits.elevationDeg);
    validateRange(result, "zScale", settings.zScale, limits.zScale);
    validateRange(result, "surfaceResolution", settings.surfaceResolution, limits.surfaceResolution);
    validateRange(result, "implicitSurfaceResolution",
                  settings.implicitSurfaceResolution, limits.implicitSurfaceResolution);
    validateRange(result, "surfaceOpacity", settings.surfaceOpacity, limits.surfaceOpacity);
    validateRange(result, "wireOpacity", settings.wireOpacity, limits.wireOpacity);
    validateRange(result, "wireThickness", settings.wireThickness, limits.wireThickness);
    validateRange(result, "wireStride", settings.wireStride, limits.wireStride);
    validateRange(result, "envelopeThickness", settings.envelopeThickness, limits.envelopeThickness);
    validateRange(result, "autoRotateSpeedDegPerSec",
                  settings.autoRotateSpeedDegPerSec, limits.autoRotateSpeedDegPerSec);
    validateRange(result, "heatmapOpacity", settings.heatmapOpacity, limits.heatmapOpacity);

    if (settings.showCoordinates && settings.showAxisTriad) {
        result.valid = false;
        result.messages.emplace_back("showCoordinates and showAxisTriad conflict.");
    }
    return result;
}

bool isAxisTriadVisible(bool showCoordinates, bool showAxisTriad) {
    return showAxisTriad && !showCoordinates;
}

bool resolveCoordinateOverlayPolicy(bool showCoordinates, bool& showAxisTriad) {
    if (!showCoordinates || !showAxisTriad) {
        return false;
    }
    showAxisTriad = false;
    return true;
}

float clampWireOpacity(float value) {
    return clampFinite(value, plotLimits().wireOpacity);
}

int clampWireStride(int value) {
    return clampFinite(value, plotLimits().wireStride);
}

int clampSurfaceResolution(int value) {
    return clampFinite(value, plotLimits().surfaceResolution);
}

int clampImplicitSurfaceResolution(int value) {
    return clampFinite(value, plotLimits().implicitSurfaceResolution);
}

float clampWireThicknessScale(float value) {
    return clampFinite(value, plotLimits().wireThicknessScale);
}

XYRenderMode resolveXYRenderMode(XYRenderModePreference preference, const SceneSummary& scene) {
    switch (preference) {
        case XYRenderModePreference::Force3D:
            return XYRenderMode::Surface3D;
        case XYRenderModePreference::Force2D:
            return XYRenderMode::Heatmap2D;
        case XYRenderModePreference::Auto:
        default:
            return (scene.hasVisible3D() && !scene.hasVisible2D())
                ? XYRenderMode::Surface3D
                : XYRenderMode::Heatmap2D;
    }
}

EffectivePlotSettings resolveEffectivePlotSettings(const PlotSettings& base,
                                                   const PlotRenderOverrides& overrides,
                                                   const PlotQualityDecision& quality,
                                                   const SceneSummary& scene) {
    PlotSettings settings = base;
    normalizePlotSettings(settings);

    EffectivePlotSettings effective;
    effective.renderMode = resolveXYRenderMode(settings.xyRenderModePreference, scene);
    effective.is3DMode = (effective.renderMode == XYRenderMode::Surface3D);
    effective.optimizeRendering = settings.optimizeRendering && !quality.overrideQuality;

    const bool useOverrides = overrides.active;
    effective.showGrid = useOverrides ? overrides.showGrid : settings.showGrid;
    effective.showCoordinates = useOverrides ? overrides.showCoordinates : settings.showCoordinates;
    effective.showWires = useOverrides ? overrides.showWires : settings.showWires;
    effective.showEnvelope = useOverrides ? overrides.showEnvelope : settings.showSurfaceEnvelope;
    const bool axisTriadPreference = useOverrides ? overrides.showAxisTriad : settings.showAxisTriad;
    effective.showAxisTriad =
        isAxisTriadVisible(effective.showCoordinates, axisTriadPreference);
    effective.showHud = useOverrides ? overrides.showHud : true;
    effective.showCanvasBorder = useOverrides ? overrides.showCanvasBorder : true;
    effective.backgroundColor = useOverrides ? overrides.backgroundColor : plotDefaults().backgroundColor;

    effective.azimuthDeg = settings.azimuthDeg;
    effective.elevationDeg = settings.elevationDeg;
    effective.zScale = settings.zScale;
    effective.autoRotate = settings.autoRotate;
    effective.autoRotateSpeedDegPerSec = settings.autoRotateSpeedDegPerSec;

    effective.surfaceResolution = settings.surfaceResolution;
    effective.implicitSurfaceResolution = settings.implicitSurfaceResolution;
    effective.surfaceOpacity = settings.surfaceOpacity;
    effective.wireOpacity = settings.wireOpacity;
    effective.wireThickness = settings.wireThickness;
    effective.wireStride = settings.wireStride;
    effective.envelopeThickness = settings.envelopeThickness;
    effective.heatmapOpacity = settings.heatmapOpacity;

    if (quality.overrideQuality) {
        effective.surfaceResolution = clampSurfaceResolution(quality.surfaceResolution);
        effective.implicitSurfaceResolution =
            clampImplicitSurfaceResolution(quality.implicitSurfaceResolution);
        if (effective.wireThickness > 0.0f) {
            effective.wireThickness =
                (std::max)(plotLimits().wireThickness.min,
                           effective.wireThickness * clampWireThicknessScale(quality.wireThicknessScale));
        }
    }

    if (quality.interactiveThrottle) {
        if (effective.surfaceResolution > 24) {
            effective.surfaceResolution = (effective.surfaceResolution * 2) / 3;
        }
        if (effective.surfaceResolution < 24) {
            effective.surfaceResolution = 24;
        }

        if (effective.implicitSurfaceResolution > 20) {
            effective.implicitSurfaceResolution /= 2;
        }
        if (effective.implicitSurfaceResolution > 40) {
            effective.implicitSurfaceResolution = 40;
        }
        if (effective.implicitSurfaceResolution < 20) {
            effective.implicitSurfaceResolution = 20;
        }
        effective.wireThickness = 0.0f;
    }

    if (!effective.showWires || effective.wireOpacity <= 0.0f || effective.wireThickness <= 0.01f) {
        effective.wireOpacity = 0.0f;
        effective.wireThickness = 0.0f;
    } else {
        effective.wireOpacity = clampWireOpacity(effective.wireOpacity);
    }
    effective.wireStride = clampWireStride(effective.wireStride);

    return effective;
}

std::string_view toStorageName(XYRenderModePreference preference) {
    switch (preference) {
        case XYRenderModePreference::Auto: return "auto";
        case XYRenderModePreference::Force3D: return "force3D";
        case XYRenderModePreference::Force2D: return "force2D";
        default: return "auto";
    }
}

std::string_view toDisplayLabel(XYRenderModePreference preference) {
    switch (preference) {
        case XYRenderModePreference::Auto: return "Auto";
        case XYRenderModePreference::Force3D: return "3D";
        case XYRenderModePreference::Force2D: return "2D";
        default: return "Auto";
    }
}

bool parseXYRenderModePreferenceStorageName(std::string_view text,
                                            XYRenderModePreference& preference) {
    if (text == "auto" || text == "Auto") {
        preference = XYRenderModePreference::Auto;
        return true;
    }
    if (text == "force3D" || text == "3D" || text == "Force3D") {
        preference = XYRenderModePreference::Force3D;
        return true;
    }
    if (text == "force2D" || text == "2D" || text == "Force2D") {
        preference = XYRenderModePreference::Force2D;
        return true;
    }
    return false;
}

std::string_view toStorageName(PlotHudMode mode) {
    switch (mode) {
        case PlotHudMode::Off: return "off";
        case PlotHudMode::Minimal: return "minimal";
        case PlotHudMode::Detailed: return "detailed";
        case PlotHudMode::OnlyWhileInteracting: return "onlyWhileInteracting";
        default: return "minimal";
    }
}

std::string_view toDisplayLabel(PlotHudMode mode) {
    switch (mode) {
        case PlotHudMode::Off: return "Off";
        case PlotHudMode::Minimal: return "Minimal";
        case PlotHudMode::Detailed: return "Detailed";
        case PlotHudMode::OnlyWhileInteracting: return "Only While Interacting";
        default: return "Minimal";
    }
}

bool parsePlotHudModeStorageName(std::string_view text, PlotHudMode& mode) {
    if (text == "off" || text == "Off") {
        mode = PlotHudMode::Off;
        return true;
    }
    if (text == "minimal" || text == "Minimal") {
        mode = PlotHudMode::Minimal;
        return true;
    }
    if (text == "detailed" || text == "Detailed") {
        mode = PlotHudMode::Detailed;
        return true;
    }
    if (text == "onlyWhileInteracting" || text == "Only While Interacting") {
        mode = PlotHudMode::OnlyWhileInteracting;
        return true;
    }
    return false;
}

const char* plotHudModeLabel(PlotHudMode mode) {
    return toDisplayLabel(mode).data();
}

XYRenderMode PlotSettings::resolveXYRenderMode(const SceneSummary& scene) const {
    return Model::resolveXYRenderMode(xyRenderModePreference, scene);
}

bool PlotSettings::effectiveShowAxisTriad() const {
    return isAxisTriadVisible(showCoordinates, showAxisTriad);
}

bool PlotSettings::applyCoordinateOverlayPolicy() {
    return resolveCoordinateOverlayPolicy(showCoordinates, showAxisTriad);
}

} // namespace XpressFormula::Model
