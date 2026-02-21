// FormulaEntryTests.cpp - Tests for formula parsing and render-mode classification.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/FormulaEntry.h"
#include "../XpressFormula/Core/Evaluator.h"
#include <cstring>
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;
using namespace XpressFormula::Core;

namespace XpressFormulaTests {

static FormulaEntry parseFormula(const char* text) {
    FormulaEntry entry;
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), text, _TRUNCATE);
    entry.parse();
    return entry;
}

TEST_CASE(FormulaEntry_ExpressionCurve2D) {
    FormulaEntry entry = parseFormula("sin(x)");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, entry.variableCount);
}

TEST_CASE(FormulaEntry_ExpressionSurface3D) {
    FormulaEntry entry = parseFormula("x^2 + y^2");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, entry.variableCount);
}

TEST_CASE(FormulaEntry_EquationImplicit2D) {
    FormulaEntry entry = parseFormula("x^2 + y^2 = 100");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Implicit2D);

    Evaluator::Variables vars = { { "x", 10.0 }, { "y", 0.0 } };
    double value = Evaluator::evaluate(entry.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(FormulaEntry_EquationSolvedForZSurface) {
    FormulaEntry entry = parseFormula("z = x^2 + y^2");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, entry.variableCount);

    Evaluator::Variables vars = { { "x", 3.0 }, { "y", 4.0 } };
    double value = Evaluator::evaluate(entry.ast, vars);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

TEST_CASE(FormulaEntry_EquationMultipleEqualsRejected) {
    FormulaEntry entry = parseFormula("x = y = 1");
    Assert::IsFalse(entry.isValid());
    Assert::IsFalse(entry.error.empty());
}

TEST_CASE(FormulaEntry_UnsupportedVariableRejected) {
    FormulaEntry entry = parseFormula("a + 2");
    Assert::IsFalse(entry.isValid());
    Assert::IsFalse(entry.error.empty());
}

} // namespace XpressFormulaTests
