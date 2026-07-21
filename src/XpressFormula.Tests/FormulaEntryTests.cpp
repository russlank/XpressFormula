// FormulaEntryTests.cpp - Compatibility tests for Model::Formula semantics.
#include "CppUnitTest.h"
#include "../XpressFormula/Model/Formula.h"
#include "../XpressFormula/UI/FormulaExamples.h"
#include "../XpressFormula/UI/FormulaPresentation.h"
#include "../XpressFormula/Core/Evaluator.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFExpression = XpressFormula::Expression;
namespace XFModel = XpressFormula::Model;
namespace XFUI = XpressFormula::UI;

namespace XpressFormulaTests {

static XFModel::Formula compileFormula(const char* text) {
    XFModel::Formula formula;
    formula.setExpression(text ? text : "");
    formula.compile();
    return formula;
}

static XFUI::FormulaRenderKind renderKindOf(const XFModel::Formula& formula) {
    return XFUI::formulaRenderKindFor(formula.compiled.kind);
}

static std::wstring widen(const char* text) {
    if (text == nullptr) {
        return {};
    }
    return std::wstring(text, text + std::strlen(text));
}

static const XFUI::ExamplePattern* findExample(const char* label) {
    for (const XFUI::ExamplePattern& example : XFUI::examplePatterns()) {
        if (std::strcmp(example.label, label) == 0) {
            return &example;
        }
    }
    return nullptr;
}

TEST_CASE(ModelFormula_ExpressionCurve2D) {
    XFModel::Formula formula = compileFormula("sin(x)");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, formula.variableCount());
}

TEST_CASE(ModelFormula_ExpressionSurface3D) {
    XFModel::Formula formula = compileFormula("x^2 + y^2");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, formula.variableCount());
}

TEST_CASE(ModelFormula_DefaultStartupFormulaIsCharacterized) {
    XFModel::Formula formula = compileFormula("sin(sqrt(x^2+y^2))");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, formula.variableCount());
    Assert::AreEqual("z = f(x,y)", XFUI::formulaTypeLabel(formula));
}

TEST_CASE(ModelFormula_EquationImplicit2D) {
    XFModel::Formula formula = compileFormula("x^2 + y^2 = 100");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(formula.compiled.equation);
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Implicit2D);

    XFCore::Evaluator::Variables vars = { { "x", 10.0 }, { "y", 0.0 } };
    double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(ModelFormula_EquationSolvedForZSurface) {
    XFModel::Formula formula = compileFormula("z = x^2 + y^2");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(formula.compiled.equation);
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, formula.variableCount());

    XFCore::Evaluator::Variables vars = { { "x", 3.0 }, { "y", 4.0 } };
    double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

TEST_CASE(ModelFormula_EquationImplicit3DScalarField) {
    XFModel::Formula formula = compileFormula("x^2 + y^2 + z^2 = 4");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(formula.compiled.equation);
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::ScalarField3D);
    Assert::AreEqual(3, formula.variableCount());
    Assert::AreEqual("F(x,y,z) = 0", XFUI::formulaTypeLabel(formula));

    XFCore::Evaluator::Variables vars = { { "x", 2.0 }, { "y", 0.0 }, { "z", 0.0 } };
    double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(ModelFormula_InvalidInputsCarryDiagnostics) {
    XFModel::Formula multipleEquals = compileFormula("x = y = 1");
    XFModel::Formula unsupportedVariable = compileFormula("a + 2");
    XFModel::Formula empty = compileFormula("");
    XFModel::Formula whitespace = compileFormula("   \t  ");

    Assert::IsFalse(multipleEquals.isValid());
    Assert::IsFalse(multipleEquals.diagnosticMessage().empty());
    Assert::IsFalse(unsupportedVariable.isValid());
    Assert::IsFalse(unsupportedVariable.diagnosticMessage().empty());
    Assert::IsFalse(empty.isValid());
    Assert::IsTrue(renderKindOf(empty) == XFUI::FormulaRenderKind::Invalid);
    Assert::AreEqual(0, empty.variableCount());
    Assert::IsFalse(whitespace.isValid());
}

TEST_CASE(ModelFormula_CompileUnchangedTextIsNoOp) {
    XFModel::Formula formula = compileFormula("sin(x)");
    const auto savedAst = formula.compiled.ast;
    const std::uint64_t savedRevision = formula.compilationRevision;

    const bool compiledAgain = formula.compile();

    Assert::IsFalse(compiledAgain);
    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(savedAst == formula.compiled.ast);
    Assert::AreEqual(savedRevision, formula.compilationRevision);
}

TEST_CASE(ModelFormula_CompileChangedTextUpdatesClassification) {
    XFModel::Formula formula = compileFormula("sin(x)");
    const std::uint64_t savedRevision = formula.compilationRevision;

    formula.setExpression("x^2 + y^2");
    const bool compiled = formula.compile();

    Assert::IsTrue(compiled);
    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Surface3D);
    Assert::AreEqual(savedRevision + 1, formula.compilationRevision);
}

TEST_CASE(ModelFormula_MoreClassificationEdges) {
    XFModel::Formula minimalCurve = compileFormula("x");
    XFModel::Formula constantOnly = compileFormula("pi + e");
    XFModel::Formula scalarField = compileFormula("x + y + z");
    XFModel::Formula yEqualsX = compileFormula("y = x");
    XFModel::Formula zEqualsX = compileFormula("z = x");

    Assert::IsTrue(minimalCurve.isValid());
    Assert::IsTrue(renderKindOf(minimalCurve) == XFUI::FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, minimalCurve.variableCount());
    Assert::IsTrue(constantOnly.isValid());
    Assert::IsTrue(renderKindOf(constantOnly) == XFUI::FormulaRenderKind::Curve2D);
    Assert::AreEqual(1, constantOnly.variableCount());
    Assert::IsTrue(scalarField.isValid());
    Assert::IsTrue(renderKindOf(scalarField) == XFUI::FormulaRenderKind::ScalarField3D);
    Assert::AreEqual(3, scalarField.variableCount());
    Assert::IsTrue(yEqualsX.isValid());
    Assert::IsTrue(yEqualsX.compiled.equation);
    Assert::IsTrue(renderKindOf(yEqualsX) == XFUI::FormulaRenderKind::Implicit2D);
    Assert::IsTrue(zEqualsX.isValid());
    Assert::IsTrue(renderKindOf(zEqualsX) == XFUI::FormulaRenderKind::Surface3D);
    Assert::AreEqual(2, zEqualsX.variableCount());
}

TEST_CASE(ModelFormula_EquationSyntaxErrorsRemainInvalid) {
    const XFModel::Formula emptyLeft = compileFormula("= x");
    const XFModel::Formula emptyRight = compileFormula("x =");
    const XFModel::Formula badLeft = compileFormula("x + = y");
    const XFModel::Formula badRight = compileFormula("x = + *");

    Assert::IsFalse(emptyLeft.isValid());
    Assert::IsTrue(emptyLeft.compiled.equation);
    Assert::IsFalse(emptyLeft.diagnosticMessage().empty());
    Assert::IsFalse(emptyRight.isValid());
    Assert::IsTrue(emptyRight.compiled.equation);
    Assert::IsFalse(emptyRight.diagnosticMessage().empty());
    Assert::IsFalse(badLeft.isValid());
    Assert::IsTrue(badLeft.compiled.equation);
    Assert::IsFalse(badRight.isValid());
    Assert::IsTrue(badRight.compiled.equation);
}

TEST_CASE(ModelFormula_TypeLabelsAndDefaults) {
    Assert::AreEqual("y = f(x)", XFUI::formulaTypeLabel(compileFormula("sin(x)")));
    Assert::AreEqual("z = f(x,y)", XFUI::formulaTypeLabel(compileFormula("x^2 + y^2")));
    Assert::AreEqual("F(x,y) = 0", XFUI::formulaTypeLabel(compileFormula("x^2 + y^2 = 1")));
    Assert::AreEqual("f(x,y,z)", XFUI::formulaTypeLabel(compileFormula("x + y + z")));
    Assert::AreEqual("invalid", XFUI::formulaTypeLabel(compileFormula("")));

    XFModel::Formula formula;
    Assert::IsFalse(formula.isValid());
    Assert::IsTrue(formula.color[0] == 1.0f);
    Assert::IsTrue(formula.color[1] == 1.0f);
    Assert::IsTrue(formula.color[2] == 1.0f);
    Assert::IsTrue(formula.color[3] == 1.0f);
    Assert::IsTrue(formula.visible);
}

TEST_CASE(ModelFormula_Uses3DSurface) {
    XFModel::Formula surface = compileFormula("x^2 + y^2");
    XFModel::Formula curve = compileFormula("sin(x)");
    XFModel::Formula scalarEq = compileFormula("x^2 + y^2 + z^2 = 4");

    Assert::IsTrue(surface.uses3DSurface());
    Assert::IsFalse(curve.uses3DSurface());
    Assert::IsTrue(scalarEq.uses3DSurface());
}

TEST_CASE(ModelFormula_EquationEvaluation_Implicit2D) {
    XFModel::Formula formula = compileFormula("x^2 + y^2 = 1");

    Assert::IsTrue(formula.isValid());
    XFCore::Evaluator::Variables vars = { {"x", 1.0}, {"y", 0.0} };
    double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(ModelFormula_EquationSolvedForZ_RightSide) {
    XFModel::Formula formula = compileFormula("x^2 + y^2 = z");

    Assert::IsTrue(formula.isValid());
    Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::Surface3D);
    XFCore::Evaluator::Variables vars = { {"x", 3.0}, {"y", 4.0} };
    double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

TEST_CASE(ModelFormula_AllBuiltinExamplesParse) {
    std::set<std::string> labels;
    std::set<std::string> expressions;
    XFCore::Evaluator::Variables vars = { {"x", 0.37}, {"y", -0.21}, {"z", 0.43} };

    for (const XFUI::ExamplePattern& example : XFUI::examplePatterns()) {
        Assert::IsTrue(example.label != nullptr && example.label[0] != '\0');
        Assert::IsTrue(example.expression != nullptr && example.expression[0] != '\0');
        Assert::IsTrue(example.description != nullptr && example.description[0] != '\0');
        if (example.includeInPresets) {
            Assert::IsTrue(std::strlen(example.label) <= 40,
                (std::wstring(L"Preset label is too long for a compact button: ") + widen(example.label)).c_str());
        }
        Assert::IsTrue(labels.insert(example.label).second,
            (std::wstring(L"Duplicate example label: ") + widen(example.label)).c_str());
        Assert::IsTrue(expressions.insert(example.expression).second,
            (std::wstring(L"Duplicate example expression: ") + widen(example.expression)).c_str());

        XFModel::Formula formula = compileFormula(example.expression);
        Assert::IsTrue(formula.isValid(),
            (std::wstring(L"Failed to parse built-in example: ") + widen(example.label)).c_str());

        const double value = XFCore::Evaluator::evaluate(formula.compiled.ast, vars);
        Assert::IsTrue(std::isfinite(value),
            (std::wstring(L"Built-in example did not evaluate to a finite sample: ") + widen(example.label)).c_str());
    }
}

TEST_CASE(ModelFormula_NewDecorativeExamplesAreImplicit3D) {
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
        const XFUI::ExamplePattern* example = findExample(label);
        Assert::IsTrue(example != nullptr,
            (std::wstring(L"Missing decorative example: ") + widen(label)).c_str());

        XFModel::Formula formula = compileFormula(example->expression);
        Assert::IsTrue(formula.isValid(),
            (std::wstring(L"Invalid decorative example: ") + widen(label)).c_str());
        Assert::IsTrue(formula.compiled.equation,
            (std::wstring(L"Decorative example is not an equation: ") + widen(label)).c_str());
        Assert::IsTrue(renderKindOf(formula) == XFUI::FormulaRenderKind::ScalarField3D,
            (std::wstring(L"Decorative example is not implicit 3D: ") + widen(label)).c_str());
        Assert::AreEqual(3, formula.variableCount(),
            (std::wstring(L"Decorative example does not use x, y, and z: ") + widen(label)).c_str());
    }
}

TEST_CASE(ModelFormula_NewHelperExamplesUseExpectedRenderKinds) {
    struct ExpectedExample {
        const char* label;
        XFUI::FormulaRenderKind renderKind;
        int variableCount;
        bool isEquation;
    };

    const ExpectedExample expected[] = {
        { "Wavy radial surface", XFUI::FormulaRenderKind::Surface3D, 2, true },
        { "Noisy terrain", XFUI::FormulaRenderKind::Surface3D, 2, true },
        { "Repeated cell pattern", XFUI::FormulaRenderKind::Implicit2D, 2, true },
        { "Asteroid sphere", XFUI::FormulaRenderKind::ScalarField3D, 3, true },
        { "Smooth double blob", XFUI::FormulaRenderKind::ScalarField3D, 3, true },
        { "Box with round tunnel", XFUI::FormulaRenderKind::ScalarField3D, 3, true },
        { "Wavy torus", XFUI::FormulaRenderKind::ScalarField3D, 3, true },
        { "Morphed sphere cube", XFUI::FormulaRenderKind::ScalarField3D, 3, true },
    };

    for (const ExpectedExample& item : expected) {
        const XFUI::ExamplePattern* example = findExample(item.label);
        Assert::IsTrue(example != nullptr,
            (std::wstring(L"Missing helper example: ") + widen(item.label)).c_str());

        XFModel::Formula formula = compileFormula(example->expression);
        Assert::IsTrue(formula.isValid(),
            (std::wstring(L"Invalid helper example: ") + widen(item.label)).c_str());
        Assert::IsTrue(renderKindOf(formula) == item.renderKind,
            (std::wstring(L"Unexpected render kind for helper example: ") + widen(item.label)).c_str());
        Assert::AreEqual(item.variableCount, formula.variableCount(),
            (std::wstring(L"Unexpected variable count for helper example: ") + widen(item.label)).c_str());
        Assert::AreEqual(item.isEquation, formula.compiled.equation,
            (std::wstring(L"Unexpected equation flag for helper example: ") + widen(item.label)).c_str());
    }
}

} // namespace XpressFormulaTests
