// FormulaPanel.h - ImGui panel for managing the list of formulas.
#pragma once

#include "FormulaEditorState.h"
#include "FormulaEntry.h"
#include "../Model/Formula.h"

#include <optional>
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
    void render(std::vector<Model::Formula>& formulas);

    void resetColorCycle(int nextIndex = 0) { m_nextColorIndex = nextIndex; }

private:
    void openEditor(const Model::Formula& formula);
    void loadEditorFormula(const char* expression);
    void renderEditorDialog(std::vector<Model::Formula>& formulas);
    void renderFunctionHelpDialog();
    void renderExampleHelpDialog();
    void requestDeleteFormula(Model::FormulaId formulaId);
    void renderDeleteConfirmationDialog(std::vector<Model::Formula>& formulas);

    int m_nextColorIndex = 0;
    bool m_openEditorPopupNextFrame = false;
    bool m_focusEditorInput = false;
    FormulaEditorState m_editorState;
    std::optional<Model::FormulaId> m_pendingDeleteFormulaId;
    bool m_openDeleteConfirmPopupNextFrame = false;

    const Core::FunctionInfo* m_selectedFunctionHelp = nullptr;
    bool m_openFunctionHelpPopupNextFrame = false;
    int  m_selectedExampleHelpIndex = -1;
    bool m_openExampleHelpPopupNextFrame = false;

};

} // namespace XpressFormula::UI
