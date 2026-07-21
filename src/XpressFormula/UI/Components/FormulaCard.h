// FormulaCard.h - Responsive formula-list card component.
#pragma once

#include "../../Model/Formula.h"

namespace XpressFormula::UI::Components {

struct FormulaCardContext {
    int index = -1;
    int count = 0;
    float availableWidth = 0.0f;
};

enum class FormulaCardActionType {
    None,
    Edit,
    Duplicate,
    Copy,
    HideOthers,
    MoveUp,
    MoveDown,
    RequestDelete
};

struct FormulaCardAction {
    FormulaCardActionType type = FormulaCardActionType::None;
    Model::FormulaId formulaId = 0;
};

struct FormulaCardResult {
    FormulaCardAction action;
    bool visibilityChanged = false;
    bool visible = true;
    bool colorChanged = false;
    Model::ColorRgba color;
    bool zSliceChanged = false;
    double zSlice = 0.0;
};

FormulaCardResult renderFormulaCard(const Model::Formula& formula,
                                    const FormulaCardContext& context);

} // namespace XpressFormula::UI::Components
