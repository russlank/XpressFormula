// PlotToolbar.h - Domain component for the plot toolbar.
#pragma once

#include "../PlotSettings.h"
#include "../UiKit/ResponsiveLayout.h"
#include "imgui.h"

namespace XpressFormula::UI::Components {

struct PlotToolbarContext {
    bool is3DMode = false;
    XYRenderMode effectiveRenderMode = XYRenderMode::Heatmap2D;
    ImVec2 availableSize{};
};

struct PlotToolbarActions {
    bool requestFit = false;
    bool requestReset = false;
    bool requestExport = false;
    bool requestRedraw = false;
    bool applyCameraPreset = false;
    float cameraAzimuthDeg = 0.0f;
    float cameraElevationDeg = 0.0f;
};

PlotToolbarActions renderPlotToolbar(PlotSettings& settings,
                                     const PlotToolbarContext& context);

} // namespace XpressFormula::UI::Components
