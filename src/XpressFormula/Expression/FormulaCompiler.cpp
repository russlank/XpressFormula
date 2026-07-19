// FormulaCompiler.cpp - Formula parsing, equation normalization, and classification.
#include "FormulaCompiler.h"

#include "../Core/Parser.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace XpressFormula::Expression {

namespace {

FormulaDiagnostic diagnostic(DiagnosticCode code, std::string message, std::size_t position = 0) {
    return FormulaDiagnostic{ code, std::move(message), position };
}

std::string unsupportedVariablesError(const VariableSet& variables) {
    std::vector<std::string> unsupported;
    for (const std::string& name : variables) {
        if (name != "x" && name != "y" && name != "z") {
            unsupported.push_back(name);
        }
    }
    if (unsupported.empty()) {
        return {};
    }

    std::string message = "Unsupported variable";
    if (unsupported.size() > 1) {
        message += "s";
    }
    message += ": ";
    for (std::size_t i = 0; i < unsupported.size(); ++i) {
        if (i > 0) {
            message += ", ";
        }
        message += unsupported[i];
    }
    message += ". Use x, y, and z only.";
    return message;
}

void appendVariables(VariableSet& destination, const VariableSet& source) {
    destination.insert(source.begin(), source.end());
}

FormulaKind classifyExpression(const VariableSet& variables) {
    const bool hasY = variables.count("y") > 0;
    const bool hasZ = variables.count("z") > 0;

    if (hasZ) {
        return FormulaKind::ScalarField3D;
    }
    if (hasY) {
        return FormulaKind::ExplicitSurface3D;
    }
    return FormulaKind::Curve2D;
}

FormulaKind classifyEquation(const CompiledFormula& formula) {
    const bool hasX = formula.variables.count("x") > 0;
    const bool hasY = formula.variables.count("y") > 0;
    const bool hasZ = formula.variables.count("z") > 0;

    const bool solvedForZLeft = isVariableNode(formula.leftAst, "z") &&
        !containsVariable(formula.rightAst, "z");
    const bool solvedForZRight = isVariableNode(formula.rightAst, "z") &&
        !containsVariable(formula.leftAst, "z");

    if (solvedForZLeft || solvedForZRight) {
        return FormulaKind::ExplicitSurface3D;
    }
    if (hasX && hasY && !hasZ) {
        return FormulaKind::ImplicitContour2D;
    }
    if (hasX && hasY && hasZ) {
        return FormulaKind::ImplicitSurface3D;
    }
    return FormulaKind::Invalid;
}

} // namespace

std::string trimFormulaText(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();

    begin = std::find_if_not(begin, end, [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto reverseEnd = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (begin >= reverseEnd) {
        return {};
    }
    return std::string(begin, reverseEnd);
}

CompiledFormula compileFormula(std::string_view expression) {
    CompiledFormula formula;
    const std::string text = trimFormulaText(expression);

    if (text.empty()) {
        formula.diagnostics.push_back(diagnostic(
            DiagnosticCode::EmptyExpression, "Empty expression"));
        return formula;
    }

    const std::size_t equalPos = text.find('=');
    if (equalPos != std::string::npos) {
        formula.equation = true;
        if (text.find('=', equalPos + 1) != std::string::npos) {
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::MultipleEquals,
                "Only one '=' is supported in an equation.",
                equalPos));
            return formula;
        }

        const std::string leftText = trimFormulaText(std::string_view(text).substr(0, equalPos));
        const std::string rightText = trimFormulaText(std::string_view(text).substr(equalPos + 1));
        if (leftText.empty() || rightText.empty()) {
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::MissingEquationSide,
                "Both sides of an equation are required.",
                equalPos));
            return formula;
        }

        auto leftResult = Core::Parser::parse(leftText);
        if (!leftResult.success()) {
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::ParseError, "Left side: " + leftResult.error));
            return formula;
        }

        auto rightResult = Core::Parser::parse(rightText);
        if (!rightResult.success()) {
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::ParseError, "Right side: " + rightResult.error));
            return formula;
        }

        formula.leftAst = leftResult.ast;
        formula.rightAst = rightResult.ast;
        formula.ast = std::make_shared<Core::BinaryOpNode>(
            Core::BinaryOperator::Subtract, formula.leftAst, formula.rightAst);
        formula.variables = leftResult.variables;
        appendVariables(formula.variables, rightResult.variables);

        const std::string unsupported = unsupportedVariablesError(formula.variables);
        if (!unsupported.empty()) {
            formula.ast = nullptr;
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::UnsupportedVariable, unsupported));
            return formula;
        }

        formula.kind = classifyEquation(formula);
        if (formula.kind == FormulaKind::ExplicitSurface3D) {
            const bool solvedForZLeft = isVariableNode(formula.leftAst, "z") &&
                !containsVariable(formula.rightAst, "z");
            formula.ast = solvedForZLeft ? formula.rightAst : formula.leftAst;
        } else if (formula.kind == FormulaKind::Invalid) {
            formula.ast = nullptr;
            formula.diagnostics.push_back(diagnostic(
                DiagnosticCode::UnsupportedEquation,
                "Equation rendering supports F(x,y)=0, z=f(x,y), or F(x,y,z)=0."));
        }
        return formula;
    }

    auto result = Core::Parser::parse(text);
    formula.ast = result.ast;
    formula.variables = result.variables;
    if (!result.success()) {
        formula.ast = nullptr;
        formula.diagnostics.push_back(diagnostic(DiagnosticCode::ParseError, result.error));
        return formula;
    }

    const std::string unsupported = unsupportedVariablesError(formula.variables);
    if (!unsupported.empty()) {
        formula.ast = nullptr;
        formula.diagnostics.push_back(diagnostic(
            DiagnosticCode::UnsupportedVariable, unsupported));
        return formula;
    }

    formula.kind = classifyExpression(formula.variables);
    return formula;
}

int variableCountForKind(FormulaKind kind) {
    switch (kind) {
        case FormulaKind::Curve2D:
            return 1;
        case FormulaKind::ExplicitSurface3D:
        case FormulaKind::ImplicitContour2D:
            return 2;
        case FormulaKind::ScalarField3D:
        case FormulaKind::ImplicitSurface3D:
            return 3;
        default:
            return 0;
    }
}

} // namespace XpressFormula::Expression
