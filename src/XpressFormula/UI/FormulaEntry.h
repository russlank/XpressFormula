// FormulaEntry.h - Data structure representing a single user-entered formula.
#pragma once

#include "../Core/ASTNode.h"
#include "../Core/Parser.h"
#include <string>
#include <set>
#include <cstring>

namespace XpressFormula::UI {

/// Holds everything about one formula: the user's text, the parsed AST,
/// detected variable count, display colour, and visibility state.
struct FormulaEntry {
    char        inputBuffer[512] = {};
    std::string lastParsedText;

    // Parsing results
    Core::ASTNodePtr        ast;
    std::string             error;
    std::set<std::string>   variables;
    int                     variableCount = 0; // 1=f(x), 2=f(x,y), 3=f(x,y,z)

    // Display settings
    float color[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool  visible   = true;
    float zSlice    = 0.0f; // For f(x,y,z): z-value of cross-section

    /// Re-parse the input buffer if the text has changed.
    void parse() {
        std::string text(inputBuffer);
        if (text == lastParsedText) return;
        lastParsedText = text;

        if (text.empty()) {
            ast = nullptr;
            error.clear();
            variables.clear();
            variableCount = 0;
            return;
        }

        auto result = Core::Parser::parse(text);
        ast       = result.ast;
        error     = result.error;
        variables = result.variables;

        bool hasX = variables.count("x") > 0;
        bool hasY = variables.count("y") > 0;
        bool hasZ = variables.count("z") > 0;

        // Rendering mode is decided by highest-dimension variable found.
        if (hasZ)      variableCount = 3;
        else if (hasY) variableCount = 2;
        else           variableCount = 1; // constants still render in the f(x) pipeline
    }

    bool isValid() const { return ast != nullptr && error.empty(); }

    const char* typeLabel() const {
        switch (variableCount) {
            case 2:  return "f(x,y)";
            case 3:  return "f(x,y,z)";
            default: return "f(x)";
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
