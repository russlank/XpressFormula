// FormulaCompiler.cpp - Formula parsing, equation normalization, and classification.
#include "FormulaCompiler.h"

#include "../Core/InputLimits.h"
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
        if (Core::variableSlotFromName(name) == Core::VariableSlot::Unknown) {
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

std::size_t maxParenthesisNesting(std::string_view text) noexcept {
    std::size_t current = 0;
    std::size_t maximum = 0;
    for (char ch : text) {
        if (ch == '(') {
            ++current;
            maximum = std::max(maximum, current);
        } else if (ch == ')' && current > 0) {
            --current;
        }
    }
    return maximum;
}

struct AstLimitState {
    std::size_t nodes = 0;
    std::size_t maxDepth = 0;
    bool exceeded = false;
};

void accumulateAstLimits(const Core::ASTNodePtr& node,
                         std::size_t depth,
                         AstLimitState& state) {
    if (!node || state.exceeded) {
        return;
    }

    ++state.nodes;
    state.maxDepth = std::max(state.maxDepth, depth);
    if (state.nodes > Core::InputLimits::kMaxAstNodes ||
        depth > Core::InputLimits::kMaxAstDepth) {
        state.exceeded = true;
        return;
    }

    switch (node->type()) {
        case Core::NodeType::BinaryOp: {
            const auto* binary = static_cast<Core::BinaryOpNode*>(node.get());
            accumulateAstLimits(binary->left, depth + 1, state);
            accumulateAstLimits(binary->right, depth + 1, state);
            break;
        }
        case Core::NodeType::UnaryOp: {
            const auto* unary = static_cast<Core::UnaryOpNode*>(node.get());
            accumulateAstLimits(unary->operand, depth + 1, state);
            break;
        }
        case Core::NodeType::FunctionCall: {
            const auto* call = static_cast<Core::FunctionCallNode*>(node.get());
            for (const Core::ASTNodePtr& argument : call->arguments) {
                accumulateAstLimits(argument, depth + 1, state);
                if (state.exceeded) {
                    return;
                }
            }
            break;
        }
        case Core::NodeType::Number:
        case Core::NodeType::Variable:
        default:
            break;
    }
}

bool appendAstLimitDiagnostic(const Core::ASTNodePtr& ast,
                              CompiledFormula& formula) {
    AstLimitState state;
    accumulateAstLimits(ast, 1, state);
    if (!state.exceeded) {
        return false;
    }

    formula.ast = nullptr;
    formula.leftAst = nullptr;
    formula.rightAst = nullptr;
    formula.diagnostics.push_back(diagnostic(
        DiagnosticCode::ExpressionTooComplex,
        "Expression is too complex to compile safely."));
    return true;
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
    if (text.size() > Core::InputLimits::kMaxFormulaLength) {
        formula.diagnostics.push_back(diagnostic(
            DiagnosticCode::ExpressionTooLong,
            "Expression is too long to compile safely."));
        return formula;
    }
    if (maxParenthesisNesting(text) > Core::InputLimits::kMaxExpressionNesting) {
        formula.diagnostics.push_back(diagnostic(
            DiagnosticCode::ExpressionTooComplex,
            "Expression nesting is too deep to compile safely."));
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
        if (appendAstLimitDiagnostic(formula.ast, formula)) {
            return formula;
        }
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
    if (appendAstLimitDiagnostic(formula.ast, formula)) {
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
