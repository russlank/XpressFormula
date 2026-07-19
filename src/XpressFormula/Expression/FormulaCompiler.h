// FormulaCompiler.h - Formula parsing, equation normalization, and classification.
#pragma once

#include "AstQueries.h"
#include "FormulaKind.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Expression {

enum class DiagnosticCode {
    EmptyExpression,
    MultipleEquals,
    MissingEquationSide,
    ParseError,
    UnsupportedVariable,
    UnsupportedEquation
};

struct FormulaDiagnostic {
    DiagnosticCode code = DiagnosticCode::ParseError;
    std::string message;
    std::size_t position = 0;
};

struct CompiledFormula {
    Core::ASTNodePtr ast;
    Core::ASTNodePtr leftAst;
    Core::ASTNodePtr rightAst;
    VariableSet variables;
    FormulaKind kind = FormulaKind::Invalid;
    bool equation = false;
    std::vector<FormulaDiagnostic> diagnostics;

    bool valid() const { return ast != nullptr && diagnostics.empty(); }
    const FormulaDiagnostic* firstDiagnostic() const {
        return diagnostics.empty() ? nullptr : &diagnostics.front();
    }
};

std::string trimFormulaText(std::string_view value);
CompiledFormula compileFormula(std::string_view expression);
int variableCountForKind(FormulaKind kind);

class FormulaCompiler {
public:
    static CompiledFormula compile(std::string_view expression) {
        return compileFormula(expression);
    }
};

} // namespace XpressFormula::Expression
