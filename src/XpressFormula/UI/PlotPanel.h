// PlotPanel.h - ImGui panel that displays the interactive plot area.
#pragma once

#include "PlotSettings.h"
#include "../Core/ViewTransform.h"
#include "../Model/Formula.h"
#include "../Model/SceneSummary.h"
#include "../Plotting/Meshing/ImplicitMeshCache.h"
#include <vector>

namespace XpressFormula::UI {

/// Renders the main plot canvas with mouse interaction (pan & zoom).
class PlotPanel {
public:
    void render(const std::vector<Model::Formula>& formulas, Core::ViewTransform& vt,
                PlotSettings& settings,
                const Model::SceneSummary& scene,
                const PlotRenderOverrides* overrides = nullptr,
                const PlotQualityDecision* qualityDecision = nullptr);

private:
    double m_lastHudInteractionTime = -1000.0;
    Plotting::Meshing::ImplicitMeshCache m_implicitMeshCache;
};

} // namespace XpressFormula::UI
