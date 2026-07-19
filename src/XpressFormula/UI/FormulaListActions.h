// FormulaListActions.h - Testable helpers for formula-list management.
#pragma once

#include "../Model/Formula.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace XpressFormula::UI::FormulaListActions {

inline bool isValidFormulaIndex(std::span<const Model::Formula> formulas, std::size_t index) {
    return index < formulas.size();
}

inline std::optional<std::size_t> findFormulaIndex(std::span<const Model::Formula> formulas,
                                                   Model::FormulaId id) {
    for (std::size_t index = 0; index < formulas.size(); ++index) {
        if (formulas[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
}

inline std::optional<std::size_t> findFormulaIndex(const std::vector<Model::Formula>& formulas,
                                                   Model::FormulaId id) {
    return findFormulaIndex(std::span<const Model::Formula>(formulas.data(), formulas.size()), id);
}

inline std::string formulaExpression(const Model::Formula& formula) {
    return formula.expression;
}

inline Model::Formula duplicateFormulaEntry(const Model::Formula& source) {
    Model::Formula duplicate = source;

    duplicate.assignNewId();
    duplicate.hasCompiledExpression = false;
    duplicate.lastCompiledExpression.clear();
    duplicate.compile(true);
    return duplicate;
}

inline bool duplicateFormula(std::vector<Model::Formula>& formulas,
                             Model::FormulaId id,
                             std::optional<Model::FormulaId>* selectedId = nullptr) {
    const std::optional<std::size_t> index = findFormulaIndex(formulas, id);
    if (!index.has_value()) {
        return false;
    }

    Model::Formula duplicate = duplicateFormulaEntry(formulas[*index]);
    formulas.insert(formulas.begin() + static_cast<std::ptrdiff_t>(*index + 1),
                    std::move(duplicate));
    (void)selectedId;
    return true;
}

inline bool moveFormula(std::vector<Model::Formula>& formulas,
                        Model::FormulaId id,
                        std::size_t toIndex,
                        std::optional<Model::FormulaId>* selectedId = nullptr) {
    const std::optional<std::size_t> fromIndex = findFormulaIndex(formulas, id);
    if (!fromIndex.has_value() || toIndex >= formulas.size()) {
        return false;
    }
    if (*fromIndex == toIndex) {
        return true;
    }

    Model::Formula moved = std::move(formulas[*fromIndex]);
    formulas.erase(formulas.begin() + static_cast<std::ptrdiff_t>(*fromIndex));
    formulas.insert(formulas.begin() + static_cast<std::ptrdiff_t>(toIndex), std::move(moved));
    (void)selectedId;
    return true;
}

inline bool deleteFormula(std::vector<Model::Formula>& formulas,
                          Model::FormulaId id,
                          std::optional<Model::FormulaId>* selectedId = nullptr) {
    const std::optional<std::size_t> index = findFormulaIndex(formulas, id);
    if (!index.has_value()) {
        return false;
    }

    formulas.erase(formulas.begin() + static_cast<std::ptrdiff_t>(*index));
    if (selectedId && selectedId->has_value() && **selectedId == id) {
        selectedId->reset();
    }
    return true;
}

inline bool hideOtherFormulas(std::vector<Model::Formula>& formulas, Model::FormulaId id) {
    if (!findFormulaIndex(formulas, id).has_value()) {
        return false;
    }

    for (Model::Formula& formula : formulas) {
        formula.visible = (formula.id == id);
    }
    return true;
}

} // namespace XpressFormula::UI::FormulaListActions
