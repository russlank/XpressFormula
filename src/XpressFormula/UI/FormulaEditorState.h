// FormulaEditorState.h - Testable state for the formula editor popup.
#pragma once

#include "../Model/Formula.h"

#include <optional>
#include <string>
#include <utility>

namespace XpressFormula::UI {

enum class FormulaEditorMode {
    Add,
    Edit
};

struct FormulaEditorState {
    std::optional<FormulaEditorMode> mode;
    std::optional<Model::FormulaId> targetId;
    Model::Formula draft;
    std::string text;
    Model::Formula preview;
    bool previewAvailable = false;

    void open(const Model::Formula& formula) {
        openEdit(formula);
    }

    void openAdd(Model::Formula formula) {
        mode = FormulaEditorMode::Add;
        targetId.reset();
        draft = std::move(formula);
        text = draft.expression;
        resetPreview();
    }

    void openEdit(const Model::Formula& formula) {
        mode = FormulaEditorMode::Edit;
        targetId = formula.id;
        draft = formula;
        text = formula.expression;
        resetPreview();
    }

    void loadText(std::string value) {
        text = std::move(value);
        resetPreview();
    }

    void close() {
        mode.reset();
        targetId.reset();
        draft = Model::Formula{};
        text.clear();
        resetPreview();
    }

    [[nodiscard]] bool active() const {
        return mode.has_value();
    }

    [[nodiscard]] bool adding() const {
        return mode == FormulaEditorMode::Add;
    }

    [[nodiscard]] bool editing() const {
        return mode == FormulaEditorMode::Edit && targetId.has_value();
    }

    bool refreshPreview() {
        if (previewAvailable && preview.expression == text) {
            return false;
        }

        preview = Model::Formula{};
        preview.setExpression(text);
        preview.compile(true);
        previewAvailable = true;
        return true;
    }

    void applyTo(Model::Formula& formula) const {
        formula.setExpression(text);
        formula.compile();
    }

    [[nodiscard]] Model::Formula buildAppliedFormula() const {
        Model::Formula formula = draft;
        applyTo(formula);
        return formula;
    }

private:
    void resetPreview() {
        preview = Model::Formula{};
        previewAvailable = false;
    }
};

} // namespace XpressFormula::UI
