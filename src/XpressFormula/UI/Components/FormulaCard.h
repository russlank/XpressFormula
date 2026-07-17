// FormulaCard.h - Responsive formula-list card component.
#pragma once

#include "../FormulaEntry.h"

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
    int formulaIndex = -1;
};

FormulaCardAction renderFormulaCard(FormulaEntry& formula,
                                    const FormulaCardContext& context);

} // namespace XpressFormula::UI::Components
