// FormulaListActionsTests.cpp - Tests for formula-list management helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/FormulaListActions.h"

#include <cstring>
#include <optional>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFModel = XpressFormula::Model;
namespace XFUI = XpressFormula::UI;

namespace XpressFormulaTests {

static XFModel::Formula makeFormula(const char* expression) {
    XFModel::Formula formula;
    formula.setExpression(expression ? expression : "");
    formula.compile();
    return formula;
}

static void assertStringEquals(const char* expected, const char* actual) {
    Assert::IsTrue(std::strcmp(expected, actual) == 0);
}

TEST_CASE(FormulaListActions_DuplicateCreatesIndependentEntryById) {
    std::vector<XFModel::Formula> formulas;
    XFModel::Formula source = makeFormula("sin(x)");
    source.visible = false;
    source.zSlice = 2.5;
    source.color[0] = 0.25f;
    source.color[1] = 0.50f;
    formulas.push_back(source);

    const auto originalAst = formulas[0].compiled.ast;
    const auto originalId = formulas[0].id;
    const bool duplicated = XFUI::FormulaListActions::duplicateFormula(formulas, originalId);

    Assert::IsTrue(duplicated);
    Assert::AreEqual(2, static_cast<int>(formulas.size()));
    assertStringEquals("sin(x)", XFUI::FormulaListActions::formulaExpression(formulas[1]).c_str());
    Assert::IsTrue(originalId != formulas[1].id);
    Assert::IsFalse(formulas[1].visible);
    Assert::AreEqual(2.5, formulas[1].zSlice);
    Assert::AreEqual(0.25f, formulas[1].color[0]);
    Assert::AreEqual(0.50f, formulas[1].color[1]);
    Assert::IsTrue(formulas[1].isValid());
    Assert::IsTrue(formulas[1].compiled.ast != nullptr);
    Assert::IsTrue(originalAst != formulas[1].compiled.ast);

    formulas[1].setExpression("cos(x)");
    formulas[1].compile();
    assertStringEquals("sin(x)", formulas[0].lastCompiledExpression.c_str());
    assertStringEquals("cos(x)", formulas[1].lastCompiledExpression.c_str());
}

TEST_CASE(FormulaListActions_DuplicateLeavesSelectedIdStable) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    std::optional<XFModel::FormulaId> selectedId = formulas[2].id;

    const bool duplicated =
        XFUI::FormulaListActions::duplicateFormula(formulas, formulas[0].id, &selectedId);

    Assert::IsTrue(duplicated);
    Assert::AreEqual(4, static_cast<int>(formulas.size()));
    Assert::IsTrue(selectedId.has_value());
    Assert::AreEqual(formulas[3].id, *selectedId);
}

TEST_CASE(FormulaListActions_ReorderPreservesStateAndSelectionById) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("x^2 + y^2"),
        makeFormula("x + y + z")
    };
    formulas[2].visible = false;
    formulas[2].zSlice = 4.0;
    formulas[2].color[2] = 0.33f;
    const auto movedId = formulas[2].id;
    std::optional<XFModel::FormulaId> selectedId = movedId;

    const bool moved = XFUI::FormulaListActions::moveFormula(formulas, movedId, 0, &selectedId);

    Assert::IsTrue(moved);
    Assert::AreEqual(movedId, formulas[0].id);
    assertStringEquals("x + y + z", formulas[0].lastCompiledExpression.c_str());
    Assert::IsFalse(formulas[0].visible);
    Assert::AreEqual(4.0, formulas[0].zSlice);
    Assert::AreEqual(0.33f, formulas[0].color[2]);
    Assert::IsTrue(selectedId.has_value());
    Assert::AreEqual(movedId, *selectedId);
    assertStringEquals("sin(x)", formulas[1].lastCompiledExpression.c_str());
    assertStringEquals("x^2 + y^2", formulas[2].lastCompiledExpression.c_str());
}

TEST_CASE(FormulaListActions_DeleteClearsSelectedIdOnlyForDeletedFormula) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    std::optional<XFModel::FormulaId> selectedId = formulas[2].id;
    const XFModel::FormulaId selectedBefore = *selectedId;

    const bool deletedBeforeSelection =
        XFUI::FormulaListActions::deleteFormula(formulas, formulas[1].id, &selectedId);

    Assert::IsTrue(deletedBeforeSelection);
    Assert::AreEqual(2, static_cast<int>(formulas.size()));
    Assert::IsTrue(selectedId.has_value());
    Assert::AreEqual(selectedBefore, *selectedId);

    const bool deletedSelection =
        XFUI::FormulaListActions::deleteFormula(formulas, selectedBefore, &selectedId);

    Assert::IsTrue(deletedSelection);
    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::IsFalse(selectedId.has_value());
}

TEST_CASE(FormulaListActions_HideOthersChangesOnlyVisibility) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    formulas[0].zSlice = 1.0;
    formulas[1].zSlice = 2.0;
    formulas[2].zSlice = 3.0;
    const XFModel::FormulaId targetId = formulas[1].id;

    const bool changed = XFUI::FormulaListActions::hideOtherFormulas(formulas, targetId);

    Assert::IsTrue(changed);
    Assert::IsFalse(formulas[0].visible);
    Assert::IsTrue(formulas[1].visible);
    Assert::IsFalse(formulas[2].visible);
    Assert::AreEqual(1.0, formulas[0].zSlice);
    Assert::AreEqual(2.0, formulas[1].zSlice);
    Assert::AreEqual(3.0, formulas[2].zSlice);
}

TEST_CASE(FormulaListActions_FormulaExpressionPreservesExactText) {
    XFModel::Formula formula;
    const char* expression = "  sin(x) + cos(y)  ";
    formula.setExpression(expression);
    formula.compile();

    const std::string copied = XFUI::FormulaListActions::formulaExpression(formula);

    assertStringEquals(expression, copied.c_str());
}

TEST_CASE(FormulaListActions_FindFormulaIndexUsesStableId) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)")
    };

    const std::optional<std::size_t> first =
        XFUI::FormulaListActions::findFormulaIndex(formulas, formulas[0].id);
    const std::optional<std::size_t> second =
        XFUI::FormulaListActions::findFormulaIndex(formulas, formulas[1].id);

    Assert::IsTrue(first.has_value());
    Assert::IsTrue(second.has_value());
    Assert::AreEqual(static_cast<std::size_t>(0), *first);
    Assert::AreEqual(static_cast<std::size_t>(1), *second);
}

TEST_CASE(FormulaListActions_InvalidIdsReturnFalse) {
    std::vector<XFModel::Formula> formulas = { makeFormula("sin(x)") };
    std::optional<XFModel::FormulaId> selectedId = formulas[0].id;
    const XFModel::FormulaId missingId = formulas[0].id + 1000;

    Assert::IsFalse(XFUI::FormulaListActions::duplicateFormula(formulas, missingId, &selectedId));
    Assert::IsFalse(XFUI::FormulaListActions::moveFormula(formulas, missingId, 0, &selectedId));
    Assert::IsFalse(XFUI::FormulaListActions::moveFormula(formulas, formulas[0].id, 5, &selectedId));
    Assert::IsFalse(XFUI::FormulaListActions::deleteFormula(formulas, missingId, &selectedId));
    Assert::IsFalse(XFUI::FormulaListActions::hideOtherFormulas(formulas, missingId));
    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::IsTrue(selectedId.has_value());
    Assert::AreEqual(formulas[0].id, *selectedId);
}

} // namespace XpressFormulaTests
