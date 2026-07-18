// FormulaEntryTests.cpp - Tests for formula parsing and render-mode classification.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/FormulaEntry.h"
#include "../XpressFormula/UI/FormulaExamples.h"
#include "../XpressFormula/Core/Evaluator.h"
#include <cstring>
#include <cmath>
#include <set>
#include <string>

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

static std::wstring widen(const char* text) {
    if (text == nullptr) {
        return {};
    }
    return std::wstring(text, text + std::strlen(text));
}

static const ExamplePattern* findExample(const char* label) {
    for (const ExamplePattern& example : examplePatterns()) {
        if (std::strcmp(example.label, label) == 0) {
            return &example;
        }
    }
    return nullptr;
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

TEST_CASE(FormulaEntry_DefaultStartupFormulaIsCharacterized) {
    FormulaEntry entry = parseFormula("sin(sqrt(x^2+y^2))");

    Assert::IsTrue(entry.isValid());
    Assert::IsTrue(entry.renderKind == FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, entry.variableCount);
    Assert::AreEqual("z = f(x,y)", entry.typeLabel());
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

TEST_CASE(FormulaEntry_AllBuiltinExamplesParse) {
    std::set<std::string> labels;
    std::set<std::string> expressions;
    FormulaEntry bufferProbe;
    const std::size_t inputBufferSize = sizeof(bufferProbe.inputBuffer);

    Evaluator::Variables vars = { {"x", 0.37}, {"y", -0.21}, {"z", 0.43} };

    for (const ExamplePattern& example : examplePatterns()) {
        Assert::IsTrue(example.label != nullptr && example.label[0] != '\0');
        Assert::IsTrue(example.expression != nullptr && example.expression[0] != '\0');
        Assert::IsTrue(example.description != nullptr && example.description[0] != '\0');
        Assert::IsTrue(std::strlen(example.expression) < inputBufferSize,
            (std::wstring(L"Example exceeds FormulaEntry input buffer: ") + widen(example.label)).c_str());
        if (example.includeInPresets) {
            Assert::IsTrue(std::strlen(example.label) <= 40,
                (std::wstring(L"Preset label is too long for a compact button: ") + widen(example.label)).c_str());
        }
        Assert::IsTrue(labels.insert(example.label).second,
            (std::wstring(L"Duplicate example label: ") + widen(example.label)).c_str());
        Assert::IsTrue(expressions.insert(example.expression).second,
            (std::wstring(L"Duplicate example expression: ") + widen(example.expression)).c_str());

        FormulaEntry entry = parseFormula(example.expression);
        Assert::IsTrue(entry.isValid(),
            (std::wstring(L"Failed to parse built-in example: ") + widen(example.label)).c_str());

        const double value = Evaluator::evaluate(entry.ast, vars);
        Assert::IsTrue(std::isfinite(value),
            (std::wstring(L"Built-in example did not evaluate to a finite sample: ") + widen(example.label)).c_str());
    }
}

TEST_CASE(FormulaEntry_NewDecorativeExamplesAreImplicit3D) {
    const char* labels[] = {
        "Gyroid",
        "Twisted gyroid",
        "Schwarz P surface",
        "Rounded cube with tunnels",
        "Three-lobed torus",
        "Heart",
        "Metaball molecule",
        "Wavy superellipsoid",
        "Spiral seed pod",
        "Symmetric cage",
        "Asteroid sphere",
        "Smooth double blob",
        "Box with round tunnel",
        "Wavy torus",
        "Morphed sphere cube",
    };

    for (const char* label : labels) {
        const ExamplePattern* example = findExample(label);
        Assert::IsTrue(example != nullptr,
            (std::wstring(L"Missing decorative example: ") + widen(label)).c_str());

        FormulaEntry entry = parseFormula(example->expression);
        Assert::IsTrue(entry.isValid(),
            (std::wstring(L"Invalid decorative example: ") + widen(label)).c_str());
        Assert::IsTrue(entry.isEquation,
            (std::wstring(L"Decorative example is not an equation: ") + widen(label)).c_str());
        Assert::IsTrue(entry.renderKind == FormulaRenderKind::ScalarField3D,
            (std::wstring(L"Decorative example is not implicit 3D: ") + widen(label)).c_str());
        Assert::AreEqual(3, entry.variableCount,
            (std::wstring(L"Decorative example does not use x, y, and z: ") + widen(label)).c_str());
    }
}

TEST_CASE(FormulaEntry_NewHelperExamplesUseExpectedRenderKinds) {
    struct ExpectedExample {
        const char* label;
        FormulaRenderKind renderKind;
        int variableCount;
        bool isEquation;
    };

    const ExpectedExample expected[] = {
        { "Wavy radial surface", FormulaRenderKind::Surface3D, 2, true },
        { "Noisy terrain", FormulaRenderKind::Surface3D, 2, true },
        { "Repeated cell pattern", FormulaRenderKind::Implicit2D, 2, true },
        { "Asteroid sphere", FormulaRenderKind::ScalarField3D, 3, true },
        { "Smooth double blob", FormulaRenderKind::ScalarField3D, 3, true },
        { "Box with round tunnel", FormulaRenderKind::ScalarField3D, 3, true },
        { "Wavy torus", FormulaRenderKind::ScalarField3D, 3, true },
        { "Morphed sphere cube", FormulaRenderKind::ScalarField3D, 3, true },
    };

    for (const ExpectedExample& item : expected) {
        const ExamplePattern* example = findExample(item.label);
        Assert::IsTrue(example != nullptr,
            (std::wstring(L"Missing helper example: ") + widen(item.label)).c_str());

        FormulaEntry entry = parseFormula(example->expression);
        Assert::IsTrue(entry.isValid(),
            (std::wstring(L"Invalid helper example: ") + widen(item.label)).c_str());
        Assert::IsTrue(entry.renderKind == item.renderKind,
            (std::wstring(L"Unexpected render kind for helper example: ") + widen(item.label)).c_str());
        Assert::AreEqual(item.variableCount, entry.variableCount,
            (std::wstring(L"Unexpected variable count for helper example: ") + widen(item.label)).c_str());
        Assert::AreEqual(item.isEquation, entry.isEquation,
            (std::wstring(L"Unexpected equation flag for helper example: ") + widen(item.label)).c_str());
    }
}

} // namespace XpressFormulaTests
