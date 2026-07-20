// PlotQualityPolicy.h - Pure quality decisions for plot rendering.
#pragma once

#include "../Model/PlotPolicy.h"

namespace XpressFormula::Plotting {

struct PlotInteractionState {
    bool draggingLeft = false;
    bool zoomingView = false;
};

enum class PlotQualityPurpose {
    Interactive,
    Preview,
    Export
};

struct PlotQualityPolicyInput {
    const Model::PlotSettings& settings;
    const Model::SceneSummary& scene;
    Model::XYRenderMode requestedRenderMode = Model::XYRenderMode::Heatmap2D;
    PlotInteractionState interaction;
    PlotQualityPurpose purpose = PlotQualityPurpose::Interactive;
    const Model::PlotQualityDecision* requestedQuality = nullptr;
};

struct PlotQualityPlan {
    Model::PlotQualityDecision decision;
    Model::EffectivePlotSettings effective;
};

class PlotQualityPolicy {
public:
    [[nodiscard]] static PlotQualityPlan resolve(
        const PlotQualityPolicyInput& input,
        const Model::PlotRenderOverrides& overrides = Model::PlotRenderOverrides{});
};

} // namespace XpressFormula::Plotting
