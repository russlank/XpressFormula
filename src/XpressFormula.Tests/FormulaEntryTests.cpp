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

TEST_CASE(FormulaEntry_EquationImplicit3DScalarField) {
    FormulaEntry entry = parseFormula("x^2 + y^2 + z^2 = 4");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::ScalarField3D);
    Assert::AreEqual(3, entry.variableCount);
    Assert::AreEqual("F(x,y,z) = 0", entry.typeLabel());

    Evaluator::Variables vars = { { "x", 2.0 }, { "y", 0.0 }, { "z", 0.0 } };
    double value = Evaluator::evaluate(entry.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
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

// ----- Edge-case tests -----

TEST_CASE(FormulaEntry_EmptyInput) {
    FormulaEntry entry = parseFormula("");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Invalid);
    Assert::AreEqual(0, entry.variableCount);
}

TEST_CASE(FormulaEntry_WhitespaceOnly) {
    FormulaEntry entry = parseFormula("   \t  ");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Invalid);
}

TEST_CASE(FormulaEntry_ReparseUnchangedText) {
    FormulaEntry entry;
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), "sin(x)", _TRUNCATE);
    entry.parse();
    Assert::IsTrue(entry.isValid());

    // Parse again with the same text — should be a no-op (caching)
    auto savedAst = entry.ast;
    entry.parse();
    Assert::IsTrue(entry.isValid());
    // The AST pointer should be the same object (not re-parsed)
    Assert::IsTrue(savedAst == entry.ast);
}

TEST_CASE(FormulaEntry_ReparseChangedText) {
    FormulaEntry entry;
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), "sin(x)", _TRUNCATE);
    entry.parse();
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Curve2D);

    // Change to a different formula
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), "x^2 + y^2", _TRUNCATE);
    entry.parse();
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
}

TEST_CASE(FormulaEntry_MinimalCurve) {
    FormulaEntry entry = parseFormula("x");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, entry.variableCount);
}

TEST_CASE(FormulaEntry_ConstantOnlyExpression) {
    FormulaEntry entry = parseFormula("pi + e");
    Assert::IsTrue(entry.isValid());
    // No variables, but the Curve2D path always sets variableCount = 1
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, entry.variableCount);
}

TEST_CASE(FormulaEntry_ExpressionWithZ) {
    // Expression (not equation) with z → ScalarField3D
    FormulaEntry entry = parseFormula("x + y + z");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::ScalarField3D);
    Assert::AreEqual(3, entry.variableCount);
}

TEST_CASE(FormulaEntry_EquationYEqualsX) {
    // "y = x" — equation with x and y but not z (and not solved for z)
    // → Implicit2D
    FormulaEntry entry = parseFormula("y = x");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Implicit2D);
}

TEST_CASE(FormulaEntry_EquationZEqualsX) {
    // "z = x" — solved for z, Surface3D
    FormulaEntry entry = parseFormula("z = x");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, entry.variableCount);
}

TEST_CASE(FormulaEntry_EquationEmptyLeftSide) {
    FormulaEntry entry = parseFormula("= x");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsFalse(entry.error.empty());
}

TEST_CASE(FormulaEntry_EquationEmptyRightSide) {
    FormulaEntry entry = parseFormula("x =");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.isEquation);
    Assert::IsFalse(entry.error.empty());
}

TEST_CASE(FormulaEntry_EquationSyntaxErrorLeft) {
    FormulaEntry entry = parseFormula("x + = y");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.isEquation);
}

TEST_CASE(FormulaEntry_EquationSyntaxErrorRight) {
    FormulaEntry entry = parseFormula("x = + *");
    Assert::IsFalse(entry.isValid());
    Assert::IsTrue(entry.isEquation);
}

TEST_CASE(FormulaEntry_TypeLabel_Curve2D) {
    FormulaEntry entry = parseFormula("sin(x)");
    Assert::AreEqual("y = f(x)", entry.typeLabel());
}

TEST_CASE(FormulaEntry_TypeLabel_Surface3D) {
    FormulaEntry entry = parseFormula("x^2 + y^2");
    Assert::AreEqual("z = f(x,y)", entry.typeLabel());
}

TEST_CASE(FormulaEntry_TypeLabel_Implicit2D) {
    FormulaEntry entry = parseFormula("x^2 + y^2 = 1");
    Assert::AreEqual("F(x,y) = 0", entry.typeLabel());
}

TEST_CASE(FormulaEntry_TypeLabel_ScalarField3D_Expression) {
    FormulaEntry entry = parseFormula("x + y + z");
    Assert::AreEqual("f(x,y,z)", entry.typeLabel());
}

TEST_CASE(FormulaEntry_TypeLabel_Invalid) {
    FormulaEntry entry = parseFormula("");
    Assert::AreEqual("invalid", entry.typeLabel());
}

TEST_CASE(FormulaEntry_IsValid_NullAST) {
    FormulaEntry entry;
    Assert::IsFalse(entry.isValid());
}

TEST_CASE(FormulaEntry_DefaultColor) {
    FormulaEntry entry;
    // Default color is white (1,1,1,1)
    Assert::IsTrue(entry.color[0] == 1.0f);
    Assert::IsTrue(entry.color[1] == 1.0f);
    Assert::IsTrue(entry.color[2] == 1.0f);
    Assert::IsTrue(entry.color[3] == 1.0f);
}

TEST_CASE(FormulaEntry_DefaultVisible) {
    FormulaEntry entry;
    Assert::IsTrue(entry.visible);
}

TEST_CASE(FormulaEntry_Uses3DSurface) {
    FormulaEntry surface = parseFormula("x^2 + y^2");
    Assert::IsTrue(surface.uses3DSurface());

    FormulaEntry curve = parseFormula("sin(x)");
    Assert::IsFalse(curve.uses3DSurface());

    FormulaEntry scalarEq = parseFormula("x^2 + y^2 + z^2 = 4");
    Assert::IsTrue(scalarEq.uses3DSurface());
}

TEST_CASE(FormulaEntry_UnsupportedVarInEquation) {
    FormulaEntry entry = parseFormula("a = x + y");
    Assert::IsFalse(entry.isValid());
    Assert::IsFalse(entry.error.empty());
}

TEST_CASE(FormulaEntry_EquationEvaluation_Implicit2D) {
    // x^2 + y^2 = 1 → ast = left - right → x^2 + y^2 - 1
    // At (1, 0): 1 + 0 - 1 = 0
    FormulaEntry entry = parseFormula("x^2 + y^2 = 1");
    Assert::IsTrue(entry.isValid());
    Evaluator::Variables vars = { {"x", 1.0}, {"y", 0.0} };
    double value = Evaluator::evaluate(entry.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(FormulaEntry_EquationSolvedForZ_RightSide) {
    // "x^2 + y^2 = z" — solved for z on right side
    FormulaEntry entry = parseFormula("x^2 + y^2 = z");
    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
    // ast should be the left side (x^2 + y^2)
    Evaluator::Variables vars = { {"x", 3.0}, {"y", 4.0} };
    double value = Evaluator::evaluate(entry.ast, vars);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

} // namespace XpressFormulaTests
