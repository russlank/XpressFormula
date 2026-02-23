// PlotPanel.h - ImGui panel that displays the interactive plot area.
#pragma once

#include "FormulaEntry.h"
#include "PlotSettings.h"
#include "../Core/ViewTransform.h"
#include <array>
#include <vector>

namespace XpressFormula::UI {

struct PlotRenderOverrides {
    bool active = false;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showEnvelope = true;
    std::array<float, 4> backgroundColor = { 0.098f, 0.098f, 0.118f, 1.0f };
};

/// Renders the main plot canvas with mouse interaction (pan & zoom).
class PlotPanel {
public:
    void render(std::vector<FormulaEntry>& formulas, Core::ViewTransform& vt,
                PlotSettings& settings,
                const PlotRenderOverrides* overrides = nullptr);
};

} // namespace XpressFormula::UI
