// ControlPanel.h - ImGui panel for zoom, pan, and reset controls.
#pragma once

#include "../Core/ViewTransform.h"

namespace XpressFormula::UI {

/// Renders zoom / pan / reset controls for the plot viewport.
class ControlPanel {
public:
    void render(Core::ViewTransform& vt);
};

} // namespace XpressFormula::UI
