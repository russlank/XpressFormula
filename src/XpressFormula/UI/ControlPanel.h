// ControlPanel.h - ImGui panel for zoom, pan, and reset controls.
#pragma once

#include "../Core/ViewTransform.h"
#include "PlotSettings.h"
#include <string>

namespace XpressFormula::UI {

struct ControlPanelActions {
    bool requestSavePlotImage = false;
    bool requestCopyPlotImage = false;
};

/// Renders zoom / pan / reset controls for the plot viewport.
class ControlPanel {
public:
    ControlPanelActions render(Core::ViewTransform& vt, PlotSettings& settings,
                               bool hasSurfaceFormula,
                               const std::string& exportStatus);
};

} // namespace XpressFormula::UI
