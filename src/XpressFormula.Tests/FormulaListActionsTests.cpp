// FormulaListActionsTests.cpp - Tests for formula-list management helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/FormulaListActions.h"

#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;

namespace XpressFormulaTests {

static FormulaEntry makeFormula(const char* expression) {
    FormulaEntry entry;
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), expression, _TRUNCATE);
    entry.parse();
    return entry;
}

static void assertStringEquals(const char* expected, const char* actual) {
    Assert::IsTrue(std::strcmp(expected, actual) == 0);
}

TEST_CASE(FormulaListActions_DuplicateCreatesIndependentEntry) {
    std::vector<FormulaEntry> formulas;
    FormulaEntry source = makeFormula("sin(x)");
    source.visible = false;
    source.zSlice = 2.5f;
    source.color[0] = 0.25f;
    source.color[1] = 0.50f;
    formulas.push_back(source);

    const auto originalAst = formulas[0].ast;
    const bool duplicated = FormulaListActions::duplicateFormula(formulas, 0);

    Assert::IsTrue(duplicated);
    Assert::AreEqual(2, static_cast<int>(formulas.size()));
    assertStringEquals("sin(x)", formulas[1].inputBuffer);
    Assert::IsFalse(formulas[1].visible);
    Assert::AreEqual(2.5f, formulas[1].zSlice);
    Assert::AreEqual(0.25f, formulas[1].color[0]);
    Assert::AreEqual(0.50f, formulas[1].color[1]);
    Assert::IsTrue(formulas[1].isValid());
    Assert::IsTrue(formulas[1].ast != nullptr);
    Assert::IsTrue(originalAst != formulas[1].ast);

    strncpy_s(formulas[1].inputBuffer, sizeof(formulas[1].inputBuffer), "cos(x)", _TRUNCATE);
    formulas[1].parse();
    assertStringEquals("sin(x)", formulas[0].lastParsedText.c_str());
    assertStringEquals("cos(x)", formulas[1].lastParsedText.c_str());
}

TEST_CASE(FormulaListActions_DuplicateAdjustsSelectedIndexAfterInsertion) {
    std::vector<FormulaEntry> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    int selectedIndex = 2;

    const bool duplicated = FormulaListActions::duplicateFormula(formulas, 0, &selectedIndex);

    Assert::IsTrue(duplicated);
    Assert::AreEqual(4, static_cast<int>(formulas.size()));
    Assert::AreEqual(3, selectedIndex);
}

TEST_CASE(FormulaListActions_ReorderPreservesStateAndSelection) {
    std::vector<FormulaEntry> formulas = {
        makeFormula("sin(x)"),
        makeFormula("x^2 + y^2"),
        makeFormula("x + y + z")
    };
    formulas[2].visible = false;
    formulas[2].zSlice = 4.0f;
    formulas[2].color[2] = 0.33f;
    int selectedIndex = 2;

    const bool moved = FormulaListActions::moveFormula(formulas, 2, 0, &selectedIndex);

    Assert::IsTrue(moved);
    assertStringEquals("x + y + z", formulas[0].lastParsedText.c_str());
    Assert::IsFalse(formulas[0].visible);
    Assert::AreEqual(4.0f, formulas[0].zSlice);
    Assert::AreEqual(0.33f, formulas[0].color[2]);
    Assert::AreEqual(0, selectedIndex);
    assertStringEquals("sin(x)", formulas[1].lastParsedText.c_str());
    assertStringEquals("x^2 + y^2", formulas[2].lastParsedText.c_str());
}

TEST_CASE(FormulaListActions_DeleteUpdatesSelectedIndexSafely) {
    std::vector<FormulaEntry> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    int selectedIndex = 2;

    const bool deletedBeforeSelection =
        FormulaListActions::deleteFormula(formulas, 1, &selectedIndex);

    Assert::IsTrue(deletedBeforeSelection);
    Assert::AreEqual(2, static_cast<int>(formulas.size()));
    Assert::AreEqual(1, selectedIndex);

    const bool deletedSelection =
        FormulaListActions::deleteFormula(formulas, selectedIndex, &selectedIndex);

    Assert::IsTrue(deletedSelection);
    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::AreEqual(-1, selectedIndex);
}

TEST_CASE(FormulaListActions_HideOthersChangesOnlyVisibility) {
    std::vector<FormulaEntry> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)"),
        makeFormula("tan(x)")
    };
    formulas[0].zSlice = 1.0f;
    formulas[1].zSlice = 2.0f;
    formulas[2].zSlice = 3.0f;

    const bool changed = FormulaListActions::hideOtherFormulas(formulas, 1);

    Assert::IsTrue(changed);
    Assert::IsFalse(formulas[0].visible);
    Assert::IsTrue(formulas[1].visible);
    Assert::IsFalse(formulas[2].visible);
    Assert::AreEqual(1.0f, formulas[0].zSlice);
    Assert::AreEqual(2.0f, formulas[1].zSlice);
    Assert::AreEqual(3.0f, formulas[2].zSlice);
}

TEST_CASE(FormulaListActions_FormulaExpressionPreservesExactBufferText) {
    FormulaEntry entry;
    const char* expression = "  sin(x) + cos(y)  ";
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), expression, _TRUNCATE);
    entry.parse();

    const std::string copied = FormulaListActions::formulaExpression(entry);

    assertStringEquals(expression, copied.c_str());
}

TEST_CASE(FormulaListActions_InvalidIndicesReturnFalse) {
    std::vector<FormulaEntry> formulas = { makeFormula("sin(x)") };
    int selectedIndex = 0;

    Assert::IsFalse(FormulaListActions::duplicateFormula(formulas, 5, &selectedIndex));
    Assert::IsFalse(FormulaListActions::moveFormula(formulas, 0, 5, &selectedIndex));
    Assert::IsFalse(FormulaListActions::deleteFormula(formulas, -1, &selectedIndex));
    Assert::IsFalse(FormulaListActions::hideOtherFormulas(formulas, 7));
    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::AreEqual(0, selectedIndex);
}

} // namespace XpressFormulaTests
