// FormulaSceneAdapter.h - UI formula list adapter for model scene analysis.
#pragma once

#include "FormulaEntry.h"
#include "../Model/SceneSummary.h"

#include <vector>

namespace XpressFormula::UI {

[[nodiscard]] inline Model::FormulaSceneItem sceneItemFor(const FormulaEntry& formula) {
    return Model::FormulaSceneItem{
        formula.visible,
        formula.isValid(),
        formula.compiled.kind
    };
}

[[nodiscard]] inline Model::SceneSummary analyzeFormulaScene(
    const std::vector<FormulaEntry>& formulas) {
    Model::SceneSummary summary;
    for (const FormulaEntry& formula : formulas) {
        Model::includeFormula(summary, sceneItemFor(formula));
    }
    return summary;
}

} // namespace XpressFormula::UI
