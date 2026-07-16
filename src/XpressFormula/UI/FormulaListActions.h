// FormulaListActions.h - Testable helpers for formula-list management.
#pragma once

#include "FormulaEntry.h"

#include <string>
#include <utility>
#include <vector>

namespace XpressFormula::UI::FormulaListActions {

inline bool isValidFormulaIndex(const std::vector<FormulaEntry>& formulas, int index) {
    return index >= 0 && index < static_cast<int>(formulas.size());
}

inline std::string formulaExpression(const FormulaEntry& formula) {
    return std::string(formula.inputBuffer);
}

inline FormulaEntry duplicateFormulaEntry(const FormulaEntry& source) {
    FormulaEntry duplicate = source;

    // Re-parse from text so the duplicate has its own AST objects instead of sharing parse trees.
    duplicate.ast = nullptr;
    duplicate.leftAst = nullptr;
    duplicate.rightAst = nullptr;
    duplicate.lastParsedText = "\x01";
    duplicate.parse();
    return duplicate;
}

inline bool duplicateFormula(std::vector<FormulaEntry>& formulas,
                             int index,
                             int* selectedIndex = nullptr) {
    if (!isValidFormulaIndex(formulas, index)) {
        return false;
    }

    FormulaEntry duplicate = duplicateFormulaEntry(formulas[index]);
    formulas.insert(formulas.begin() + index + 1, std::move(duplicate));
    if (selectedIndex && *selectedIndex > index) {
        ++(*selectedIndex);
    }
    return true;
}

inline bool moveFormula(std::vector<FormulaEntry>& formulas,
                        int fromIndex,
                        int toIndex,
                        int* selectedIndex = nullptr) {
    if (!isValidFormulaIndex(formulas, fromIndex) ||
        !isValidFormulaIndex(formulas, toIndex)) {
        return false;
    }
    if (fromIndex == toIndex) {
        return true;
    }

    FormulaEntry moved = std::move(formulas[fromIndex]);
    formulas.erase(formulas.begin() + fromIndex);
    formulas.insert(formulas.begin() + toIndex, std::move(moved));

    if (selectedIndex) {
        if (*selectedIndex == fromIndex) {
            *selectedIndex = toIndex;
        } else if (fromIndex < toIndex &&
                   *selectedIndex > fromIndex &&
                   *selectedIndex <= toIndex) {
            --(*selectedIndex);
        } else if (toIndex < fromIndex &&
                   *selectedIndex >= toIndex &&
                   *selectedIndex < fromIndex) {
            ++(*selectedIndex);
        }
    }
    return true;
}

inline bool deleteFormula(std::vector<FormulaEntry>& formulas,
                          int index,
                          int* selectedIndex = nullptr) {
    if (!isValidFormulaIndex(formulas, index)) {
        return false;
    }

    formulas.erase(formulas.begin() + index);
    if (selectedIndex) {
        if (*selectedIndex == index) {
            *selectedIndex = -1;
        } else if (*selectedIndex > index) {
            --(*selectedIndex);
        }
        if (*selectedIndex >= static_cast<int>(formulas.size())) {
            *selectedIndex = formulas.empty() ? -1 : static_cast<int>(formulas.size()) - 1;
        }
    }
    return true;
}

inline bool hideOtherFormulas(std::vector<FormulaEntry>& formulas, int index) {
    if (!isValidFormulaIndex(formulas, index)) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(formulas.size()); ++i) {
        formulas[static_cast<std::size_t>(i)].visible = (i == index);
    }
    return true;
}

} // namespace XpressFormula::UI::FormulaListActions
