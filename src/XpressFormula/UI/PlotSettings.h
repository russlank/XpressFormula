// PlotSettings.h - Compatibility shim for model-owned plot settings policy.
#pragma once

#include "../Model/PlotPolicy.h"

namespace XpressFormula::UI {

using Model::EffectivePlotSettings;
using Model::PlotDefaults;
using Model::PlotHudMode;
using Model::PlotLimits;
using Model::PlotQualityDecision;
using Model::PlotRenderOverrides;
using Model::PlotSettings;
using Model::PlotValueRange;
using Model::ValidationResult;
using Model::XYRenderMode;
using Model::XYRenderModePreference;

using Model::clampImplicitSurfaceResolution;
using Model::clampSurfaceResolution;
using Model::clampWireOpacity;
using Model::clampWireStride;
using Model::clampWireThicknessScale;
using Model::defaultPlotSettings;
using Model::isAxisTriadVisible;
using Model::kDefaultAutoRotateSpeedDegPerSec;
using Model::kDefaultAzimuthDeg;
using Model::kDefaultElevationDeg;
using Model::kDefaultEnvelopeThickness;
using Model::kDefaultHeatmapOpacity;
using Model::kDefaultImplicitSurfaceResolution;
using Model::kDefaultSurfaceOpacity;
using Model::kDefaultSurfaceResolution;
using Model::kDefaultWireOpacity;
using Model::kDefaultWireStride;
using Model::kDefaultWireThickness;
using Model::kDefaultZScale;
using Model::kPlotDefaults;
using Model::kPlotLimits;
using Model::normalizePlotSettings;
using Model::parsePlotHudModeStorageName;
using Model::parseXYRenderModePreferenceStorageName;
using Model::plotDefaults;
using Model::plotHudModeLabel;
using Model::plotLimits;
using Model::resolveCoordinateOverlayPolicy;
using Model::resolveEffectivePlotSettings;
using Model::resolveXYRenderMode;
using Model::toDisplayLabel;
using Model::toStorageName;
using Model::validatePlotSettings;

} // namespace XpressFormula::UI
