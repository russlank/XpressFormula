// SPDX-License-Identifier: MIT
// ControlPanel.h - ImGui sidebar panel for view controls, rendering settings, and export actions.
#pragma once

#include "../Core/ViewTransform.h"
#include "PlotSettings.h"
#include <string>

namespace XpressFormula::UI {

struct ControlPanelActions {
    bool requestOpenExportDialog = false;
};

/// Renders view controls, plot rendering settings, and export actions.
class ControlPanel {
public:
    ControlPanelActions render(Core::ViewTransform& vt, PlotSettings& settings,
                               bool hasSurfaceFormula,
                               const std::string& exportStatus);

private:
    bool m_displaySectionExpanded = false;
};

} // namespace XpressFormula::UI
