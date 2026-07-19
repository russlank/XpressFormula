// SceneSummary.h - Pure visible-scene capability analysis.
#pragma once

#include "Formula.h"

#include <span>

namespace XpressFormula::Model {

struct FormulaSceneItem {
    bool visible = false;
    bool valid = false;
    Expression::FormulaKind kind = Expression::FormulaKind::Invalid;
};

struct SceneSummary {
    bool hasVisibleCurve2D = false;
    bool hasVisibleImplicit2D = false;
    bool hasVisibleScalarField = false;
    bool hasVisibleExplicitSurface3D = false;
    bool hasVisibleImplicitSurface3D = false;

    [[nodiscard]] bool hasVisible2D() const;
    [[nodiscard]] bool hasVisible3D() const;
    [[nodiscard]] bool empty() const;
};

void includeFormula(SceneSummary& summary, const FormulaSceneItem& formula);

[[nodiscard]] SceneSummary analyzeScene(std::span<const FormulaSceneItem> formulas);
[[nodiscard]] SceneSummary analyzeScene(std::span<const Formula> formulas);

} // namespace XpressFormula::Model
