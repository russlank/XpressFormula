// FormulaPresentation.h - UI presentation helpers for compiled formula kinds.
#pragma once

#include "../Expression/FormulaKind.h"

namespace XpressFormula::UI {

enum class FormulaRenderKind {
    Curve2D,
    Surface3D,
    Implicit2D,
    ScalarField3D,
    Invalid
};

inline FormulaRenderKind formulaRenderKindFor(Expression::FormulaKind kind) {
    switch (kind) {
        case Expression::FormulaKind::Curve2D:
            return FormulaRenderKind::Curve2D;
        case Expression::FormulaKind::ExplicitSurface3D:
            return FormulaRenderKind::Surface3D;
        case Expression::FormulaKind::ImplicitContour2D:
            return FormulaRenderKind::Implicit2D;
        case Expression::FormulaKind::ScalarField3D:
        case Expression::FormulaKind::ImplicitSurface3D:
            return FormulaRenderKind::ScalarField3D;
        default:
            return FormulaRenderKind::Invalid;
    }
}

inline const char* formulaTypeLabel(FormulaRenderKind renderKind, bool isEquation) {
    switch (renderKind) {
        case FormulaRenderKind::Curve2D:
            return "y = f(x)";
        case FormulaRenderKind::Surface3D:
            return "z = f(x,y)";
        case FormulaRenderKind::Implicit2D:
            return "F(x,y) = 0";
        case FormulaRenderKind::ScalarField3D:
            return isEquation ? "F(x,y,z) = 0" : "f(x,y,z)";
        default:
            return "invalid";
    }
}

} // namespace XpressFormula::UI
