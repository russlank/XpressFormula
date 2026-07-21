// FormulaPresentation.h - UI presentation helpers for compiled formula kinds.
#pragma once

#include "../Expression/FormulaKind.h"
#include "../Expression/FormulaCompiler.h"
#include "../Model/Formula.h"

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

inline const char* formulaTypeLabel(Expression::FormulaKind kind, bool isEquation) {
    return formulaTypeLabel(formulaRenderKindFor(kind), isEquation);
}

inline const char* formulaTypeLabel(const Model::Formula& formula) {
    return formulaTypeLabel(formula.compiled.kind, formula.compiled.equation);
}

inline int displayedVariableCount(Expression::FormulaKind kind) {
    return Expression::variableCountForKind(kind);
}

inline int displayedVariableCount(const Model::Formula& formula) {
    return displayedVariableCount(formula.compiled.kind);
}

inline bool formulaUses3DSurface(const Model::Formula& formula) {
    return formula.compiled.kind == Expression::FormulaKind::ExplicitSurface3D ||
           formula.compiled.kind == Expression::FormulaKind::ImplicitSurface3D;
}

inline const char* formulaDiagnosticText(const Model::Formula& formula) {
    const Expression::FormulaDiagnostic* diagnostic = formula.compiled.firstDiagnostic();
    return diagnostic ? diagnostic->message.c_str() : "";
}

} // namespace XpressFormula::UI
