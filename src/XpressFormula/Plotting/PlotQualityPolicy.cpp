// PlotQualityPolicy.cpp - Pure quality decisions for plot rendering.
#include "PlotQualityPolicy.h"

#include <algorithm>

namespace XpressFormula::Plotting {

PlotQualityPlan PlotQualityPolicy::resolve(const PlotQualityPolicyInput& input,
                                           const Model::PlotRenderOverrides& overrides) {
    Model::PlotQualityDecision decision = input.requestedQuality
        ? *input.requestedQuality
        : Model::PlotQualityDecision{};

    if (input.purpose == PlotQualityPurpose::Preview && !decision.overrideQuality) {
        decision.overrideQuality = true;
        decision.surfaceResolution =
            Model::clampSurfaceResolution(std::min(input.settings.surfaceResolution, 40));
        decision.implicitSurfaceResolution =
            Model::clampImplicitSurfaceResolution(std::min(input.settings.implicitSurfaceResolution, 48));
        decision.wireThicknessScale = 0.75f;
    }

    const bool canThrottle =
        input.purpose == PlotQualityPurpose::Interactive &&
        input.settings.optimizeRendering &&
        input.scene.hasVisible3D() &&
        input.requestedRenderMode == Model::XYRenderMode::Surface3D;
    decision.interactiveThrottle =
        decision.interactiveThrottle ||
        (canThrottle && (input.interaction.draggingLeft || input.interaction.zoomingView));

    PlotQualityPlan plan;
    plan.decision = decision;
    plan.effective = Model::resolveEffectivePlotSettings(
        input.settings,
        overrides,
        decision,
        input.scene);
    return plan;
}

} // namespace XpressFormula::Plotting
