// FormulaKind.h - Domain classification for compiled formulas.
#pragma once

namespace XpressFormula::Expression {

enum class FormulaKind {
    Curve2D,
    ExplicitSurface3D,
    ImplicitContour2D,
    ScalarField3D,
    ImplicitSurface3D,
    Invalid
};

} // namespace XpressFormula::Expression
