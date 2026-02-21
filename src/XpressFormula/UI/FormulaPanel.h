// FormulaPanel.h - ImGui panel for managing the list of formulas.
#pragma once

#include "FormulaEntry.h"
#include <vector>

namespace XpressFormula::UI {

/// Renders the formula-list sidebar where users can add, edit, and remove
/// mathematical expressions.
class FormulaPanel {
public:
    /// Draw the panel contents (call between ImGui::Begin / End).
    void render(std::vector<FormulaEntry>& formulas);

private:
    int m_nextColorIndex = 0;
};

} // namespace XpressFormula::UI
