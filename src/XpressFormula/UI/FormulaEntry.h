// FormulaEntry.h - UI compatibility adapter for a domain formula.
#pragma once

#include "FormulaPresentation.h"
#include "../Core/ASTNode.h"
#include "../Expression/FormulaCompiler.h"
#include "../Model/Formula.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <set>
#include <string>
#include <utility>

namespace XpressFormula::UI {

inline constexpr std::size_t kFormulaEntryInputBufferMirrorSize = 512;

/// Holds UI-facing formula state while delegating parsing and classification
/// to the expression compiler. `expression` is the stored source of truth;
/// `inputBuffer` remains only as a temporary compatibility mirror for older
/// UI/tests that still write directly to the struct.
struct FormulaEntry {
    Model::FormulaId id = Model::nextFormulaId();
    std::string expression;
    char inputBuffer[kFormulaEntryInputBufferMirrorSize] = {};
    std::string lastParsedText;

    // Parsing results
    Core::ASTNodePtr ast;
    Core::ASTNodePtr leftAst;
    Core::ASTNodePtr rightAst;
    std::string error;
    std::set<std::string> variables;
    int variableCount = 0; // 1=curve, 2=xy surface/implicit, 3=xyz field
    bool isEquation = false;
    FormulaRenderKind renderKind = FormulaRenderKind::Invalid;
    Expression::CompiledFormula compiled;

    // Display settings
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool visible = true;
    float zSlice = 0.0f; // For f(x,y,z): z-value of cross-section

    std::string inputBufferMirror;

    void setExpression(std::string value) {
        expression = std::move(value);
        syncInputBufferFromExpression();
    }

    const std::string& expressionText() const {
        return expression;
    }

    void assignNewId() {
        id = Model::nextFormulaId();
    }

    /// Re-parse the stored expression if the text has changed.
    void parse() {
        captureLegacyInputBufferChange();

        const std::string text = Expression::trimFormulaText(expression);
        if (text == lastParsedText) {
            return;
        }
        lastParsedText = text;

        compiled = Expression::compileFormula(expression);
        ast = compiled.ast;
        leftAst = compiled.leftAst;
        rightAst = compiled.rightAst;
        variables = compiled.variables;
        variableCount = Expression::variableCountForKind(compiled.kind);
        isEquation = compiled.equation;
        renderKind = formulaRenderKindFor(compiled.kind);

        error.clear();
        if (!compiled.valid()) {
            ast = nullptr;
            variableCount = 0;
            renderKind = FormulaRenderKind::Invalid;
            if (const Expression::FormulaDiagnostic* first = compiled.firstDiagnostic()) {
                if (first->code != Expression::DiagnosticCode::EmptyExpression) {
                    error = first->message;
                }
            }
        }
    }

    bool isValid() const {
        return ast != nullptr && error.empty();
    }

    bool uses3DSurface() const {
        return renderKind == FormulaRenderKind::Surface3D ||
               (renderKind == FormulaRenderKind::ScalarField3D && isEquation);
    }

    const char* typeLabel() const {
        return formulaTypeLabel(renderKind, isEquation);
    }

private:
    void syncInputBufferFromExpression() {
        const std::size_t copyLength =
            (std::min)(expression.size(), sizeof(inputBuffer) - 1);
        if (copyLength > 0) {
            std::memcpy(inputBuffer, expression.data(), copyLength);
        }
        inputBuffer[copyLength] = '\0';
        inputBufferMirror.assign(inputBuffer);
        lastParsedText = "\x01";
    }

    void captureLegacyInputBufferChange() {
        const std::string bufferText(inputBuffer);
        if (bufferText != inputBufferMirror) {
            expression = bufferText;
            inputBufferMirror = bufferText;
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
