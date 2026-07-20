// ApplicationState.h - Host-level state and render-loop policy helpers.
#pragma once

#include "../Model/Document.h"
#include "../Model/PlotPolicy.h"
#include "../Model/SceneSummary.h"

#include <cmath>

namespace XpressFormula::Application {

struct PlotRuntimeState {
    float autoRotationOffsetDeg = 0.0f;

    void resetAutoRotation() noexcept {
        autoRotationOffsetDeg = 0.0f;
    }
};

struct ApplicationState {
    float sidebarWidth = 360.0f;
    Model::SceneSummary sceneSummary;
    Model::Document::Revision observedDocumentRevision = 0;
    PlotRuntimeState plotRuntime;
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

[[nodiscard]] inline float normalizeRuntimeAzimuthDeg(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    while (value > 180.0f) {
        value -= 360.0f;
    }
    while (value < -180.0f) {
        value += 360.0f;
    }
    return value;
}

[[nodiscard]] inline float effectiveRuntimeAzimuthDeg(
    const Model::PlotSettings& settings,
    const PlotRuntimeState& runtime) noexcept {
    return normalizeRuntimeAzimuthDeg(settings.azimuthDeg + runtime.autoRotationOffsetDeg);
}

[[nodiscard]] inline bool advanceAutoRotation(
    PlotRuntimeState& runtime,
    const Model::PlotSettings& settings,
    float deltaSeconds) noexcept {
    if (!(deltaSeconds > 0.0f) ||
        !std::isfinite(deltaSeconds) ||
        !settings.autoRotate) {
        return false;
    }

    const float before = runtime.autoRotationOffsetDeg;
    runtime.autoRotationOffsetDeg = normalizeRuntimeAzimuthDeg(
        runtime.autoRotationOffsetDeg +
        deltaSeconds * settings.autoRotateSpeedDegPerSec);
    return runtime.autoRotationOffsetDeg != before;
}

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
