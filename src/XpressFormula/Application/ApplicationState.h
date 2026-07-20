// ApplicationState.h - Host-level state and render-loop policy helpers.
#pragma once

#include "../Model/Document.h"
#include "../Model/PlotPolicy.h"
#include "../Model/SceneSummary.h"

namespace XpressFormula::Application {

struct ApplicationState {
    float sidebarWidth = 360.0f;
    Model::SceneSummary sceneSummary;
    Model::Document::Revision observedDocumentRevision = 0;
    bool redrawRequested = true;
    bool closeRequestedAfterFrame = false;
};

struct RenderFramePolicyInput {
    bool optimizeRendering = false;
    bool redrawRequested = false;
    bool autoRotate = false;
    bool sceneHasVisible3D = false;
    Model::XYRenderMode effectiveRenderMode = Model::XYRenderMode::Heatmap2D;
};

[[nodiscard]] inline bool isContinuousRenderRequired(const RenderFramePolicyInput& input) noexcept {
    return input.autoRotate &&
           input.sceneHasVisible3D &&
           input.effectiveRenderMode == Model::XYRenderMode::Surface3D;
}

[[nodiscard]] inline bool shouldWaitForNextMessage(const RenderFramePolicyInput& input) noexcept {
    return input.optimizeRendering &&
           !input.redrawRequested &&
           !isContinuousRenderRequired(input);
}

[[nodiscard]] inline bool redrawRequestedAfterPresentedFrame(
    const RenderFramePolicyInput& input) noexcept {
    return input.optimizeRendering ? isContinuousRenderRequired(input) : true;
}

[[nodiscard]] inline int occludedSwapChainSleepMilliseconds(
    const RenderFramePolicyInput& input) noexcept {
    return isContinuousRenderRequired(input) ? 10 : 30;
}

} // namespace XpressFormula::Application
