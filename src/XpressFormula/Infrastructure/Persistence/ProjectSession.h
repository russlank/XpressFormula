// SPDX-License-Identifier: MIT
// ProjectSession.h - Versioned .xfplot persistence DTOs.
#pragma once

#include "../../Model/PlotSettings.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Infrastructure::Persistence {

inline constexpr int kProjectSessionSchemaVersion = 1;
inline constexpr std::string_view kProjectSessionFileType = "XpressFormulaProject";

struct ProjectFormulaRecord {
    std::string expression;
    std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool visible = true;
    float zSlice = 0.0f;
};

struct ProjectViewRecord {
    double centerX = 0.0;
    double centerY = 0.0;
    double scaleX = 60.0;
    double scaleY = 60.0;
};

struct ProjectPlotSettingsRecord {
    Model::XYRenderModePreference xyRenderModePreference = Model::XYRenderModePreference::Auto;
    Model::PlotHudMode hudMode = Model::PlotHudMode::Minimal;
    bool optimizeRendering = true;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    float azimuthDeg = Model::kDefaultAzimuthDeg;
    float elevationDeg = Model::kDefaultElevationDeg;
    float zScale = Model::kDefaultZScale;
    int surfaceResolution = Model::kDefaultSurfaceResolution;
    int implicitSurfaceResolution = Model::kDefaultImplicitSurfaceResolution;
    float surfaceOpacity = Model::kDefaultSurfaceOpacity;
    float wireOpacity = Model::kDefaultWireOpacity;
    float wireThickness = Model::kDefaultWireThickness;
    int wireStride = Model::kDefaultWireStride;
    bool showSurfaceEnvelope = true;
    float envelopeThickness = Model::kDefaultEnvelopeThickness;
    bool showAxisTriad = false;
    bool autoRotate = false;
    float autoRotateSpeedDegPerSec = Model::kDefaultAutoRotateSpeedDegPerSec;
    float heatmapOpacity = Model::kDefaultHeatmapOpacity;
};

struct ProjectSession {
    int schemaVersion = kProjectSessionSchemaVersion;
    std::vector<ProjectFormulaRecord> formulas;
    ProjectViewRecord view;
    ProjectPlotSettingsRecord plot;
};

} // namespace XpressFormula::Infrastructure::Persistence
