// FormulaPanel.h - ImGui panel for managing the list of formulas.
#pragma once

#include "FormulaEntry.h"
#include <string>
#include <vector>

namespace XpressFormula::UI {

/// Renders the formula-list sidebar where users can add, edit, and remove
/// mathematical expressions.
class FormulaPanel {
public:
    /// Draw the panel contents (call between ImGui::Begin / End).
    void render(std::vector<FormulaEntry>& formulas);

private:
    void openEditor(const FormulaEntry& formula, int formulaIndex);
    void renderEditorDialog(std::vector<FormulaEntry>& formulas);

    int m_nextColorIndex = 0;
    bool m_openEditorPopupNextFrame = false;
    bool m_focusEditorInput = false;
    int  m_editorFormulaIndex = -1;
    char m_editorBuffer[2048] = {};

    // Cached live-validation preview so we don't parse every frame.
    FormulaEntry m_editorPreview;
    std::string  m_editorPreviousText;
};

} // namespace XpressFormula::UI
