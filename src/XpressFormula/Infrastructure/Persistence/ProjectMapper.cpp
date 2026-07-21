// SPDX-License-Identifier: MIT
// ProjectMapper.cpp - Mapping between live model state and project persistence DTOs.
#include "ProjectMapper.h"
#include "ProjectSerializer.h"
#include "../../Core/InputLimits.h"
#include "../../Model/PlotPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace XpressFormula::Infrastructure::Persistence {
namespace {

ProjectPlotSettingsRecord toProjectPlotRecord(const Model::PlotSettings& plot) {
    ProjectPlotSettingsRecord record;
    record.xyRenderModePreference = plot.xyRenderModePreference;
    record.hudMode = plot.hudMode;
    record.optimizeRendering = plot.optimizeRendering;
    record.showGrid = plot.showGrid;
    record.showCoordinates = plot.showCoordinates;
    record.showWires = plot.showWires;
    record.azimuthDeg = plot.azimuthDeg;
    record.elevationDeg = plot.elevationDeg;
    record.zScale = plot.zScale;
    record.surfaceResolution = plot.surfaceResolution;
    record.implicitSurfaceResolution = plot.implicitSurfaceResolution;
    record.surfaceOpacity = plot.surfaceOpacity;
    record.wireOpacity = plot.wireOpacity;
    record.wireThickness = plot.wireThickness;
    record.wireStride = plot.wireStride;
    record.showSurfaceEnvelope = plot.showSurfaceEnvelope;
    record.envelopeThickness = plot.envelopeThickness;
    record.showAxisTriad = plot.showAxisTriad;
    record.autoRotate = plot.autoRotate;
    record.autoRotateSpeedDegPerSec = plot.autoRotateSpeedDegPerSec;
    record.heatmapOpacity = plot.heatmapOpacity;
    return record;
}

Model::PlotSettings toModelPlotSettings(const ProjectPlotSettingsRecord& record) {
    Model::PlotSettings plot;
    plot.xyRenderModePreference = record.xyRenderModePreference;
    plot.hudMode = record.hudMode;
    plot.optimizeRendering = record.optimizeRendering;
    plot.showGrid = record.showGrid;
    plot.showCoordinates = record.showCoordinates;
    plot.showWires = record.showWires;
    plot.azimuthDeg = record.azimuthDeg;
    plot.elevationDeg = record.elevationDeg;
    plot.zScale = record.zScale;
    plot.surfaceResolution = record.surfaceResolution;
    plot.implicitSurfaceResolution = record.implicitSurfaceResolution;
    plot.surfaceOpacity = record.surfaceOpacity;
    plot.wireOpacity = record.wireOpacity;
    plot.wireThickness = record.wireThickness;
    plot.wireStride = record.wireStride;
    plot.showSurfaceEnvelope = record.showSurfaceEnvelope;
    plot.envelopeThickness = record.envelopeThickness;
    plot.showAxisTriad = record.showAxisTriad;
    plot.autoRotate = record.autoRotate;
    plot.autoRotateSpeedDegPerSec = record.autoRotateSpeedDegPerSec;
    plot.heatmapOpacity = record.heatmapOpacity;
    Model::normalizePlotSettings(plot);
    return plot;
}

} // namespace

ProjectSession makeProjectSession(const std::vector<Model::Formula>& formulas,
                                  const Core::ViewTransform& view,
                                  const Model::PlotSettings& plot) {
    ProjectSession session;
    session.formulas.reserve(formulas.size());
    for (const Model::Formula& formula : formulas) {
        ProjectFormulaRecord record;
        record.expression = formula.expression;
        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            record.color[channel] = formula.color[channel];
        }
        record.visible = formula.visible;
        record.zSlice =
            (std::isfinite(formula.zSlice) &&
             formula.zSlice >= -static_cast<double>((std::numeric_limits<float>::max)()) &&
             formula.zSlice <= static_cast<double>((std::numeric_limits<float>::max)()))
                ? static_cast<float>(formula.zSlice)
                : 0.0f;
        session.formulas.push_back(std::move(record));
    }

    session.view.centerX = view.state.centerX;
    session.view.centerY = view.state.centerY;
    session.view.scaleX = view.state.scaleX;
    session.view.scaleY = view.state.scaleY;
    session.plot = toProjectPlotRecord(plot);
    return session;
}

ProjectMapResult mapProjectSessionToDocument(const ProjectSession& session) {
    ProjectMapResult result;
    result.document.formulas.clear();
    result.document.formulas.reserve(
        (std::min)(session.formulas.size(), Core::InputLimits::kMaxProjectFormulas));
    for (std::size_t i = 0; i < session.formulas.size(); ++i) {
        if (i >= Core::InputLimits::kMaxProjectFormulas) {
            result.warnings.emplace_back("Skipped remaining formulas: project formula limit reached.");
            break;
        }

        const ProjectFormulaRecord& record = session.formulas[i];
        Model::Formula entry;
        entry.setExpression(record.expression);

        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            entry.color[channel] = record.color[channel];
        }
        entry.visible = record.visible;
        entry.zSlice = record.zSlice;
        entry.compile();
        if (!entry.isValid() && !record.expression.empty()) {
            const Expression::FormulaDiagnostic* diagnostic = entry.compiled.firstDiagnostic();
            result.warnings.emplace_back("Formula " + std::to_string(i + 1) +
                " did not parse: " + (diagnostic ? diagnostic->message : "unknown parse error"));
        }
        result.document.formulas.push_back(std::move(entry));
    }

    result.document.view.state.centerX =
        std::isfinite(session.view.centerX) ? session.view.centerX : 0.0;
    result.document.view.state.centerY =
        std::isfinite(session.view.centerY) ? session.view.centerY : 0.0;
    result.document.view.state.scaleX =
        std::clamp(std::isfinite(session.view.scaleX) ? session.view.scaleX : 60.0,
                   0.1, 100000.0);
    result.document.view.state.scaleY =
        std::clamp(std::isfinite(session.view.scaleY) ? session.view.scaleY : 60.0,
                   0.1, 100000.0);

    result.document.plot = toModelPlotSettings(session.plot);
    return result;
}

void applyProjectSession(const ProjectSession& session,
                         std::vector<Model::Formula>& formulas,
                         Core::ViewTransform& view,
                         Model::PlotSettings& plot,
                         std::vector<std::string>& warnings) {
    ProjectMapResult mapped = mapProjectSessionToDocument(session);
    warnings.insert(warnings.end(), mapped.warnings.begin(), mapped.warnings.end());
    formulas = std::move(mapped.document.formulas);
    view.state = mapped.document.view.state;
    plot = mapped.document.plot;
}

std::string serializeCurrentProjectSession(const std::vector<Model::Formula>& formulas,
                                           const Core::ViewTransform& view,
                                           const Model::PlotSettings& plot) {
    return serializeProjectSession(makeProjectSession(formulas, view, plot));
}

} // namespace XpressFormula::Infrastructure::Persistence
