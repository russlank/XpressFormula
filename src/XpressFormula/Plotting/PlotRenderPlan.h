// PlotRenderPlan.h - Pure plot render dispatch planning.
#pragma once

#include "Camera3D.h"
#include "PlotQualityPolicy.h"
#include "../Model/Formula.h"
#include "../Model/PlotPolicy.h"
#include "../Model/SceneSummary.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace XpressFormula::Plotting {

enum class PlotFormulaDispatchKind {
    None,
    Curve2D,
    Heatmap2D,
    ImplicitContour2D,
    ExplicitSurface3D,
    ImplicitSurface3D,
    CrossSection2D
};

enum class PlotRenderPassKind {
    Grid2D,
    Axes2D,
    Formula2D,
    Surface3DBelowGrid,
    Grid3D,
    Surface3DAboveGrid,
    Surface3DAll,
    Axes3D
};

struct PlotFormulaDispatch {
    std::size_t formulaIndex = 0;
    Model::FormulaId formulaId = 0;
    PlotFormulaDispatchKind kind = PlotFormulaDispatchKind::None;
};

struct PlotRenderPlan {
    Model::XYRenderMode requestedRenderMode = Model::XYRenderMode::Heatmap2D;
    Model::EffectivePlotSettings effective;
    Model::PlotQualityDecision qualityDecision;
    Camera3D camera;
    bool is3DMode = false;
    bool gridPlaneInterleave = false;
    bool draw2DGrid = false;
    bool draw2DAxes = false;
    std::vector<PlotRenderPassKind> passes;
    std::vector<PlotFormulaDispatch> formulas;
};

struct PlotRenderPlanInput {
    std::span<const Model::Formula> formulas;
    const Model::PlotSettings& settings;
    const Model::SceneSummary& scene;
    PlotInteractionState interaction;
    const Model::PlotRenderOverrides* overrides = nullptr;
    const Model::PlotQualityDecision* quality = nullptr;
    PlotQualityPurpose qualityPurpose = PlotQualityPurpose::Interactive;
    std::optional<float> runtimeAzimuthDeg;
};

[[nodiscard]] PlotRenderPlan buildPlotRenderPlan(const PlotRenderPlanInput& input);

} // namespace XpressFormula::Plotting
