// FormulaEditorState.h - Testable state for the formula editor popup.
#pragma once

#include "../Model/Formula.h"

#include <optional>
#include <string>
#include <utility>

namespace XpressFormula::UI {

struct FormulaEditorState {
    std::optional<Model::FormulaId> targetId;
    std::string text;
    Model::Formula preview;
    bool previewAvailable = false;

    void open(const Model::Formula& formula) {
        targetId = formula.id;
        text = formula.expression;
        resetPreview();
    }

    void loadText(std::string value) {
        text = std::move(value);
        resetPreview();
    }

    void close() {
        targetId.reset();
        text.clear();
        resetPreview();
    }

    [[nodiscard]] bool editing() const {
        return targetId.has_value();
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

private:
    void resetPreview() {
        preview = Model::Formula{};
        previewAvailable = false;
    }
};

} // namespace XpressFormula::UI
