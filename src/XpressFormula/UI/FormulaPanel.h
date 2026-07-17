// FormulaPanel.h - ImGui panel for managing the list of formulas.
#pragma once

#include "FormulaEntry.h"
#include <string>
#include <vector>

namespace XpressFormula::Core {
struct FunctionInfo;
}

namespace XpressFormula::UI {

/// Renders the formula-list sidebar where users can add, edit, and remove
/// mathematical expressions.
class FormulaPanel {
public:
    /// Draw the panel contents (call between ImGui::Begin / End).
    void render(std::vector<FormulaEntry>& formulas);

    void resetColorCycle(int nextIndex = 0) { m_nextColorIndex = nextIndex; }

private:
    void openEditor(const FormulaEntry& formula, int formulaIndex);
    void loadEditorFormula(const char* expression);
    void renderEditorDialog(std::vector<FormulaEntry>& formulas);
    void renderFunctionHelpDialog();
    void renderExampleHelpDialog();
    void requestDeleteFormula(int formulaIndex);
    void renderDeleteConfirmationDialog(std::vector<FormulaEntry>& formulas);

    int m_nextColorIndex = 0;
    bool m_openEditorPopupNextFrame = false;
    bool m_focusEditorInput = false;
    int  m_editorFormulaIndex = -1;
    char m_editorBuffer[2048] = {};
    int  m_pendingDeleteFormulaIndex = -1;
    bool m_openDeleteConfirmPopupNextFrame = false;

    const Core::FunctionInfo* m_selectedFunctionHelp = nullptr;
    bool m_openFunctionHelpPopupNextFrame = false;
    int  m_selectedExampleHelpIndex = -1;
    bool m_openExampleHelpPopupNextFrame = false;

    // Cached live-validation preview so we don't parse every frame.
    FormulaEntry m_editorPreview;
    std::string  m_editorPreviousText;
};

} // namespace XpressFormula::UI
