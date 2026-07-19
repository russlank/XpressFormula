// Formula.h - Domain formula state.
#pragma once

#include "Color.h"
#include "FormulaId.h"
#include "../Expression/FormulaCompiler.h"

#include <cstdint>
#include <string>
#include <utility>

namespace XpressFormula::Model {

struct Formula {
    FormulaId id = nextFormulaId();
    std::string expression;
    ColorRgba color;
    bool visible = true;
    double zSlice = 0.0;
    Expression::CompiledFormula compiled;
    std::uint64_t compilationRevision = 0;
    bool hasCompiledExpression = false;
    std::string lastCompiledExpression;

    void setExpression(std::string value) {
        expression = std::move(value);
    }

    [[nodiscard]] const std::string& expressionText() const {
        return expression;
    }

    void assignNewId() {
        id = nextFormulaId();
    }

    bool compile(bool force = false) {
        if (!force && hasCompiledExpression && expression == lastCompiledExpression) {
            return false;
        }

        compiled = Expression::compileFormula(expression);
        lastCompiledExpression = expression;
        hasCompiledExpression = true;
        ++compilationRevision;
        return true;
    }

    void parse() {
        (void)compile();
    }

    [[nodiscard]] bool valid() const {
        return compiled.valid();
    }

    [[nodiscard]] bool isValid() const {
        return valid();
    }

    [[nodiscard]] bool isEquation() const {
        return compiled.equation;
    }

    [[nodiscard]] int variableCount() const {
        return Expression::variableCountForKind(compiled.kind);
    }

    [[nodiscard]] std::string diagnosticMessage() const {
        const Expression::FormulaDiagnostic* diagnostic = compiled.firstDiagnostic();
        return diagnostic ? diagnostic->message : std::string{};
    }

    [[nodiscard]] bool uses3DSurface() const {
        return compiled.kind == Expression::FormulaKind::ExplicitSurface3D ||
               compiled.kind == Expression::FormulaKind::ImplicitSurface3D;
    }
};

} // namespace XpressFormula::Model
