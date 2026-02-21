// FormulaEntry.h - Data structure representing a single user-entered formula.
#pragma once

#include "../Core/ASTNode.h"
#include "../Core/Parser.h"
#include <string>
#include <set>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <vector>

namespace XpressFormula::UI {

enum class FormulaRenderKind {
    Curve2D,
    Surface3D,
    Implicit2D,
    ScalarField3D,
    Invalid
};

namespace Detail {

inline std::string trim(std::string value) {
    auto begin = std::find_if_not(value.begin(), value.end(),
        [](unsigned char ch) { return std::isspace(ch) != 0; });
    auto end = std::find_if_not(value.rbegin(), value.rend(),
        [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

inline void collectVariables(const Core::ASTNodePtr& node, std::set<std::string>& vars) {
    if (!node) {
        return;
    }

    switch (node->type()) {
        case Core::NodeType::Variable:
            vars.insert(static_cast<Core::VariableNode*>(node.get())->name);
            break;
        case Core::NodeType::BinaryOp: {
            const auto* binary = static_cast<Core::BinaryOpNode*>(node.get());
            collectVariables(binary->left, vars);
            collectVariables(binary->right, vars);
            break;
        }
        case Core::NodeType::UnaryOp:
            collectVariables(static_cast<Core::UnaryOpNode*>(node.get())->operand, vars);
            break;
        case Core::NodeType::FunctionCall: {
            const auto* fn = static_cast<Core::FunctionCallNode*>(node.get());
            for (const auto& arg : fn->arguments) {
                collectVariables(arg, vars);
            }
            break;
        }
        default:
            break;
    }
}

inline bool containsVariable(const Core::ASTNodePtr& node, const std::string& name) {
    std::set<std::string> vars;
    collectVariables(node, vars);
    return vars.count(name) > 0;
}

inline bool isVariableNode(const Core::ASTNodePtr& node, const char* name) {
    if (!node || node->type() != Core::NodeType::Variable) {
        return false;
    }
    return static_cast<Core::VariableNode*>(node.get())->name == name;
}

inline std::string unsupportedVariablesError(const std::set<std::string>& vars) {
    std::vector<std::string> unsupported;
    for (const std::string& name : vars) {
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
    for (size_t i = 0; i < unsupported.size(); ++i) {
        if (i > 0) {
            message += ", ";
        }
        message += unsupported[i];
    }
    message += ". Use x, y, and z only.";
    return message;
}

} // namespace Detail

/// Holds everything about one formula: the user's text, the parsed AST,
/// detected variable count, display colour, and visibility state.
struct FormulaEntry {
    char        inputBuffer[512] = {};
    std::string lastParsedText;

    // Parsing results
    Core::ASTNodePtr        ast;
    Core::ASTNodePtr        leftAst;
    Core::ASTNodePtr        rightAst;
    std::string             error;
    std::set<std::string>   variables;
    int                     variableCount = 0; // 1=curve, 2=xy surface/implicit, 3=xyz field
    bool                    isEquation = false;
    FormulaRenderKind       renderKind = FormulaRenderKind::Invalid;

    // Display settings
    float color[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool  visible   = true;
    float zSlice    = 0.0f; // For f(x,y,z): z-value of cross-section

    /// Re-parse the input buffer if the text has changed.
    void parse() {
        std::string text = Detail::trim(std::string(inputBuffer));
        if (text == lastParsedText) return;
        lastParsedText = text;

        if (text.empty()) {
            ast = nullptr;
            leftAst = nullptr;
            rightAst = nullptr;
            error.clear();
            variables.clear();
            variableCount = 0;
            isEquation = false;
            renderKind = FormulaRenderKind::Invalid;
            return;
        }

        auto applyRenderKind = [this]() {
            const bool hasX = variables.count("x") > 0;
            const bool hasY = variables.count("y") > 0;
            const bool hasZ = variables.count("z") > 0;

            if (isEquation) {
                const bool solvedForZLeft = Detail::isVariableNode(leftAst, "z") &&
                    !Detail::containsVariable(rightAst, "z");
                const bool solvedForZRight = Detail::isVariableNode(rightAst, "z") &&
                    !Detail::containsVariable(leftAst, "z");
                if (solvedForZLeft || solvedForZRight) {
                    renderKind = FormulaRenderKind::Surface3D;
                    variableCount = 2;
                    ast = solvedForZLeft ? rightAst : leftAst;
                    return;
                }
                if (hasX && hasY && !hasZ) {
                    renderKind = FormulaRenderKind::Implicit2D;
                    variableCount = 2;
                    return;
                }
                if (hasX && hasY && hasZ) {
                    renderKind = FormulaRenderKind::ScalarField3D;
                    variableCount = 3;
                    return;
                }
                renderKind = FormulaRenderKind::Invalid;
                variableCount = 0;
                error = "Equation rendering supports F(x,y)=0, z=f(x,y), or F(x,y,z)=0.";
                ast = nullptr;
                return;
            }

            if (hasZ) {
                renderKind = FormulaRenderKind::ScalarField3D;
                variableCount = 3;
            } else if (hasY) {
                renderKind = FormulaRenderKind::Surface3D;
                variableCount = 2;
            } else {
                renderKind = FormulaRenderKind::Curve2D;
                variableCount = 1;
            }
        };

        const size_t equalPos = text.find('=');
        if (equalPos != std::string::npos) {
            if (text.find('=', equalPos + 1) != std::string::npos) {
                error = "Only one '=' is supported in an equation.";
                ast = nullptr;
                leftAst = nullptr;
                rightAst = nullptr;
                variables.clear();
                variableCount = 0;
                isEquation = true;
                renderKind = FormulaRenderKind::Invalid;
                return;
            }

            const std::string leftText = Detail::trim(text.substr(0, equalPos));
            const std::string rightText = Detail::trim(text.substr(equalPos + 1));
            if (leftText.empty() || rightText.empty()) {
                error = "Both sides of an equation are required.";
                ast = nullptr;
                leftAst = nullptr;
                rightAst = nullptr;
                variables.clear();
                variableCount = 0;
                isEquation = true;
                renderKind = FormulaRenderKind::Invalid;
                return;
            }

            auto leftResult = Core::Parser::parse(leftText);
            if (!leftResult.success()) {
                error = "Left side: " + leftResult.error;
                ast = nullptr;
                leftAst = nullptr;
                rightAst = nullptr;
                variables.clear();
                variableCount = 0;
                isEquation = true;
                renderKind = FormulaRenderKind::Invalid;
                return;
            }

            auto rightResult = Core::Parser::parse(rightText);
            if (!rightResult.success()) {
                error = "Right side: " + rightResult.error;
                ast = nullptr;
                leftAst = nullptr;
                rightAst = nullptr;
                variables.clear();
                variableCount = 0;
                isEquation = true;
                renderKind = FormulaRenderKind::Invalid;
                return;
            }

            leftAst = leftResult.ast;
            rightAst = rightResult.ast;
            ast = std::make_shared<Core::BinaryOpNode>(
                Core::BinaryOperator::Subtract, leftAst, rightAst);
            variables = leftResult.variables;
            variables.insert(rightResult.variables.begin(), rightResult.variables.end());
            isEquation = true;
            error.clear();

            const std::string unsupported = Detail::unsupportedVariablesError(variables);
            if (!unsupported.empty()) {
                error = unsupported;
                ast = nullptr;
                renderKind = FormulaRenderKind::Invalid;
                variableCount = 0;
                return;
            }

            applyRenderKind();
            return;
        }

        auto result = Core::Parser::parse(text);
        ast = result.ast;
        leftAst = nullptr;
        rightAst = nullptr;
        error = result.error;
        variables = result.variables;
        isEquation = false;

        if (!result.success()) {
            renderKind = FormulaRenderKind::Invalid;
            variableCount = 0;
            return;
        }

        const std::string unsupported = Detail::unsupportedVariablesError(variables);
        if (!unsupported.empty()) {
            error = unsupported;
            ast = nullptr;
            renderKind = FormulaRenderKind::Invalid;
            variableCount = 0;
            return;
        }

        applyRenderKind();
    }

    bool isValid() const { return ast != nullptr && error.empty(); }
    bool uses3DSurface() const { return renderKind == FormulaRenderKind::Surface3D; }

    const char* typeLabel() const {
        switch (renderKind) {
            case FormulaRenderKind::Curve2D:      return "y = f(x)";
            case FormulaRenderKind::Surface3D:    return "z = f(x,y)";
            case FormulaRenderKind::Implicit2D:   return "F(x,y) = 0";
            case FormulaRenderKind::ScalarField3D:return "f(x,y,z)";
            default:                              return "invalid";
        }
    }
};

/// Default palette used when assigning colours to new formulas.
inline const float kDefaultPalette[][4] = {
    { 0.10f, 0.80f, 0.25f, 1.0f },  // green
    { 0.25f, 0.60f, 1.00f, 1.0f },  // blue
    { 1.00f, 0.30f, 0.30f, 1.0f },  // red
    { 1.00f, 0.80f, 0.10f, 1.0f },  // yellow
    { 0.80f, 0.35f, 1.00f, 1.0f },  // purple
    { 0.10f, 0.80f, 0.80f, 1.0f },  // cyan
    { 1.00f, 0.50f, 0.10f, 1.0f },  // orange
    { 0.60f, 0.80f, 0.25f, 1.0f },  // lime
};
inline constexpr int kPaletteSize = sizeof(kDefaultPalette) / sizeof(kDefaultPalette[0]);

} // namespace XpressFormula::UI
