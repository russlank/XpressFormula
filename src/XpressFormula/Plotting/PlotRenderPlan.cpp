// PlotRenderPlan.cpp - Pure plot render dispatch planning.
#include "PlotRenderPlan.h"

namespace XpressFormula::Plotting {

namespace {

PlotFormulaDispatchKind dispatchKindFor(const Model::Formula& formula,
                                        bool is3DMode) noexcept {
    if (!formula.visible || !formula.isValid()) {
        return PlotFormulaDispatchKind::None;
    }

    switch (formula.compiled.kind) {
        case Expression::FormulaKind::Curve2D:
            return is3DMode
                ? PlotFormulaDispatchKind::None
                : PlotFormulaDispatchKind::Curve2D;
        case Expression::FormulaKind::ExplicitSurface3D:
            return is3DMode
                ? PlotFormulaDispatchKind::ExplicitSurface3D
                : PlotFormulaDispatchKind::Heatmap2D;
        case Expression::FormulaKind::ImplicitContour2D:
            return is3DMode
                ? PlotFormulaDispatchKind::None
                : PlotFormulaDispatchKind::ImplicitContour2D;
        case Expression::FormulaKind::ScalarField3D:
        case Expression::FormulaKind::ImplicitSurface3D:
            if (is3DMode && formula.compiled.equation) {
                return PlotFormulaDispatchKind::ImplicitSurface3D;
            }
            return is3DMode
                ? PlotFormulaDispatchKind::None
                : PlotFormulaDispatchKind::CrossSection2D;
        default:
            return PlotFormulaDispatchKind::None;
    }
}

} // namespace

PlotRenderPlan buildPlotRenderPlan(const PlotRenderPlanInput& input) {
    const Model::PlotRenderOverrides overrides =
        input.overrides ? *input.overrides : Model::PlotRenderOverrides{};
    const Model::XYRenderMode requestedRenderMode =
        Model::resolveXYRenderMode(input.settings.xyRenderModePreference, input.scene);
    const PlotQualityPolicyInput qualityInput{
        input.settings,
        input.scene,
        requestedRenderMode,
        input.interaction,
        input.qualityPurpose,
        input.quality
    };
    const PlotQualityPlan qualityPlan =
        PlotQualityPolicy::resolve(qualityInput, overrides);

    PlotRenderPlan plan;
    plan.requestedRenderMode = requestedRenderMode;
    plan.effective = qualityPlan.effective;
    plan.qualityDecision = qualityPlan.decision;
    plan.camera = Camera3D{
        input.runtimeAzimuthDeg.value_or(plan.effective.azimuthDeg),
        plan.effective.elevationDeg,
        plan.effective.zScale
    };
    plan.is3DMode = plan.effective.is3DMode;
    plan.gridPlaneInterleave = plan.is3DMode && plan.effective.showGrid;
    plan.draw2DGrid = !plan.is3DMode && plan.effective.showGrid;
    plan.draw2DAxes = !plan.is3DMode && plan.effective.showCoordinates;

    if (plan.is3DMode) {
        if (plan.gridPlaneInterleave) {
            plan.passes.push_back(PlotRenderPassKind::Surface3DBelowGrid);
            plan.passes.push_back(PlotRenderPassKind::Grid3D);
            plan.passes.push_back(PlotRenderPassKind::Surface3DAboveGrid);
        } else {
            plan.passes.push_back(PlotRenderPassKind::Surface3DAll);
            if (plan.effective.showGrid) {
                plan.passes.push_back(PlotRenderPassKind::Grid3D);
            }
        }
        if (plan.effective.showCoordinates) {
            plan.passes.push_back(PlotRenderPassKind::Axes3D);
        }
    } else {
        if (plan.draw2DGrid) {
            plan.passes.push_back(PlotRenderPassKind::Grid2D);
        }
        if (plan.draw2DAxes) {
            plan.passes.push_back(PlotRenderPassKind::Axes2D);
        }
        plan.passes.push_back(PlotRenderPassKind::Formula2D);
    }

    plan.formulas.reserve(input.formulas.size());
    for (std::size_t i = 0; i < input.formulas.size(); ++i) {
        const Model::Formula& formula = input.formulas[i];
        plan.formulas.push_back(PlotFormulaDispatch{
            i,
            formula.id,
            dispatchKindFor(formula, plan.is3DMode)
        });
    }

    return plan;
}

} // namespace XpressFormula::Plotting
