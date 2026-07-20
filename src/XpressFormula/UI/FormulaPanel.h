// FormulaPanel.h - ImGui panel for managing the list of formulas.
#pragma once

#include "FormulaEditorState.h"
#include "FormulaEntry.h"
#include "../Model/Formula.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace XpressFormula::Core {
struct FunctionInfo;
}

namespace XpressFormula::UI {

enum class FormulaPanelCommandType {
    AddFormula,
    UpdateFormula,
    RemoveFormula,
    DuplicateFormula,
    MoveFormula,
    SetVisibility,
    SetColor,
    SetZSlice,
    HideOtherFormulas
};

struct FormulaPanelCommand {
    FormulaPanelCommandType type = FormulaPanelCommandType::AddFormula;
    Model::FormulaId formulaId = 0;
    Model::Formula formula;
    std::size_t toIndex = 0;
    bool visible = true;
    Model::ColorRgba color;
    double zSlice = 0.0;
};

struct FormulaPanelActions {
    std::vector<FormulaPanelCommand> commands;

    [[nodiscard]] bool hasDocumentCommands() const noexcept {
        return !commands.empty();
    }
};

    /// Renders the formula-list sidebar where users can add, edit, and remove
    /// mathematical expressions.
class FormulaPanel {
public:
    /// Draw the panel contents (call between ImGui::Begin / End).
    [[nodiscard]] FormulaPanelActions render(std::span<const Model::Formula> formulas);

    void resetColorCycle(int nextIndex = 0) { m_nextColorIndex = nextIndex; }

private:
    void openEditor(const Model::Formula& formula);
    void loadEditorFormula(const char* expression);
    void renderEditorDialog(std::span<const Model::Formula> formulas,
                            FormulaPanelActions& actions);
    void renderFunctionHelpDialog();
    void renderExampleHelpDialog();
    void requestDeleteFormula(Model::FormulaId formulaId);
    void renderDeleteConfirmationDialog(std::span<const Model::Formula> formulas,
                                        FormulaPanelActions& actions);

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
