// SceneSummary.cpp - Pure visible-scene capability analysis.
#include "SceneSummary.h"

namespace XpressFormula::Model {

bool SceneSummary::hasVisible2D() const {
    return hasVisibleCurve2D || hasVisibleImplicit2D || hasVisibleScalarField;
}

bool SceneSummary::hasVisible3D() const {
    return hasVisibleExplicitSurface3D || hasVisibleImplicitSurface3D;
}

bool SceneSummary::empty() const {
    return !hasVisible2D() && !hasVisible3D();
}

void includeFormula(SceneSummary& summary, const FormulaSceneItem& formula) {
    if (!formula.visible || !formula.valid) {
        return;
    }

    switch (formula.kind) {
        case Expression::FormulaKind::Curve2D:
            summary.hasVisibleCurve2D = true;
            break;
        case Expression::FormulaKind::ImplicitContour2D:
            summary.hasVisibleImplicit2D = true;
            break;
        case Expression::FormulaKind::ScalarField3D:
            summary.hasVisibleScalarField = true;
            break;
        case Expression::FormulaKind::ExplicitSurface3D:
            summary.hasVisibleExplicitSurface3D = true;
            break;
        case Expression::FormulaKind::ImplicitSurface3D:
            summary.hasVisibleImplicitSurface3D = true;
            break;
        case Expression::FormulaKind::Invalid:
        default:
            break;
    }
}

SceneSummary analyzeScene(std::span<const FormulaSceneItem> formulas) {
    SceneSummary summary;
    for (const FormulaSceneItem& formula : formulas) {
        includeFormula(summary, formula);
    }
    return summary;
}

SceneSummary analyzeScene(std::span<const Formula> formulas) {
    SceneSummary summary;
    for (const Formula& formula : formulas) {
        includeFormula(summary, FormulaSceneItem{
            formula.visible,
            formula.compiled.valid(),
            formula.compiled.kind
        });
    }
    return summary;
}

} // namespace XpressFormula::Model
