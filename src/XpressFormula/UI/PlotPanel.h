// PlotPanel.h - ImGui panel that displays the interactive plot area.
#pragma once

#include "FormulaEntry.h"
#include "PlotSettings.h"
#include "../Core/ViewTransform.h"
#include <vector>

namespace XpressFormula::UI {

/// Renders the main plot canvas with mouse interaction (pan & zoom).
class PlotPanel {
public:
    void render(std::vector<FormulaEntry>& formulas, Core::ViewTransform& vt,
                PlotSettings& settings);
};

} // namespace XpressFormula::UI
