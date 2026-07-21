// FormulaCompilerTests.cpp - Tests for formula compilation and classification.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/Evaluator.h"
#include "../XpressFormula/Expression/FormulaCompiler.h"

#include <cmath>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFExpression = XpressFormula::Expression;

namespace XpressFormulaTests {

static XFExpression::CompiledFormula compile(const char* expression) {
    return XFExpression::compileFormula(expression ? expression : "");
}

TEST_CASE(FormulaCompiler_EmptyFormulaIsInvalidWithDiagnostic) {
    const auto formula = compile("   ");

    Assert::IsFalse(formula.valid());
    Assert::AreEqual(1, static_cast<int>(formula.diagnostics.size()));
    Assert::IsTrue(formula.diagnostics[0].code == XFExpression::DiagnosticCode::EmptyExpression);
}

TEST_CASE(FormulaCompiler_ClassifiesPlainXCurve) {
    const auto formula = compile("sin(x)");

    Assert::IsTrue(formula.valid());
    Assert::IsFalse(formula.equation);
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::Curve2D);
    Assert::AreEqual(1, XFExpression::variableCountForKind(formula.kind));
}

TEST_CASE(FormulaCompiler_ClassifiesExplicitXySurface) {
    const auto formula = compile("x^2 + y^2");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ExplicitSurface3D);
    Assert::AreEqual(2, XFExpression::variableCountForKind(formula.kind));
}

TEST_CASE(FormulaCompiler_ClassifiesScalarXyzField) {
    const auto formula = compile("x + y + z");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ScalarField3D);
    Assert::AreEqual(3, XFExpression::variableCountForKind(formula.kind));
}

TEST_CASE(FormulaCompiler_ClassifiesImplicit2DEquation) {
    const auto formula = compile("x^2 + y^2 = 1");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.equation);
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ImplicitContour2D);

    XFCore::Evaluator::Variables variables = { { "x", 1.0 }, { "y", 0.0 } };
    const double value = XFCore::Evaluator::evaluate(formula.ast, variables);
    Assert::IsTrue(std::abs(value) < 1e-9);
}

TEST_CASE(FormulaCompiler_ClassifiesImplicit3DEquation) {
    const auto formula = compile("x^2 + y^2 + z^2 = 4");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.equation);
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ImplicitSurface3D);
    Assert::AreEqual(3, XFExpression::variableCountForKind(formula.kind));
}

TEST_CASE(FormulaCompiler_SolvedForZOnLeftUsesRightAst) {
    const auto formula = compile("z = x^2 + y^2");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.equation);
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ExplicitSurface3D);

    XFCore::Evaluator::Variables variables = { { "x", 3.0 }, { "y", 4.0 } };
    const double value = XFCore::Evaluator::evaluate(formula.ast, variables);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

TEST_CASE(FormulaCompiler_SolvedForZOnRightUsesLeftAst) {
    const auto formula = compile("x^2 + y^2 = z");

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.equation);
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::ExplicitSurface3D);

    XFCore::Evaluator::Variables variables = { { "x", 3.0 }, { "y", 4.0 } };
    const double value = XFCore::Evaluator::evaluate(formula.ast, variables);
    Assert::IsTrue(std::abs(value - 25.0) < 1e-9);
}

TEST_CASE(FormulaCompiler_RejectsMultipleEquals) {
    const auto formula = compile("x = y = 1");

    Assert::IsFalse(formula.valid());
    Assert::IsTrue(formula.equation);
    Assert::IsTrue(formula.diagnostics[0].code == XFExpression::DiagnosticCode::MultipleEquals);
}

TEST_CASE(FormulaCompiler_RejectsMissingEquationSide) {
    const auto leftMissing = compile("= x");
    const auto rightMissing = compile("x = ");

    Assert::IsFalse(leftMissing.valid());
    Assert::IsFalse(rightMissing.valid());
    Assert::IsTrue(leftMissing.diagnostics[0].code == XFExpression::DiagnosticCode::MissingEquationSide);
    Assert::IsTrue(rightMissing.diagnostics[0].code == XFExpression::DiagnosticCode::MissingEquationSide);
}

TEST_CASE(FormulaCompiler_RejectsUnsupportedVariables) {
    const auto formula = compile("a + x");

    Assert::IsFalse(formula.valid());
    Assert::IsTrue(formula.diagnostics[0].code == XFExpression::DiagnosticCode::UnsupportedVariable);
}

TEST_CASE(FormulaCompiler_RejectsInvalidFunctions) {
    const auto formula = compile("notafunc(x)");

    Assert::IsFalse(formula.valid());
    Assert::IsTrue(formula.diagnostics[0].code == XFExpression::DiagnosticCode::ParseError);
}

TEST_CASE(FormulaCompiler_RejectsMalformedNumbersWithoutThrowing) {
    std::string veryLongLiteral(4096, '9');
    const std::string expressions[] = {
        ".",
        "1e309",
        "1e-9999",
        veryLongLiteral
    };

    for (const std::string& expression : expressions) {
        bool threw = false;
        XFExpression::CompiledFormula formula;
        try {
            formula = XFExpression::compileFormula(expression);
        } catch (...) {
            threw = true;
        }

        Assert::IsFalse(threw);
        Assert::IsFalse(formula.valid());
        Assert::IsFalse(formula.diagnostics.empty());
        Assert::IsTrue(formula.diagnostics[0].code == XFExpression::DiagnosticCode::ParseError);
    }
}

TEST_CASE(FormulaCompiler_AcceptsZeroScientificLiteralsButRejectsTrueUnderflow) {
    const char* validZeros[] = {
        "0",
        "-0",
        "0.",
        ".0",
        "0e999",
        "0e-9999",
        "-0e999",
        "0.000e123",
        "0.0000E-99999"
    };
    for (const char* expression : validZeros) {
        const auto formula = compile(expression);
        Assert::IsTrue(formula.valid());
    }

    const char* invalidNumbers[] = {
        "1e-9999",
        "0.1e-999999",
        "1e309"
    };
    for (const char* expression : invalidNumbers) {
        bool threw = false;
        XFExpression::CompiledFormula formula;
        try {
            formula = compile(expression);
        } catch (...) {
            threw = true;
        }

        Assert::IsFalse(threw);
        Assert::IsFalse(formula.valid());
        Assert::IsFalse(formula.diagnostics.empty());
        Assert::IsTrue(
            formula.diagnostics[0].message.find("outside the supported range") != std::string::npos);
    }
}

TEST_CASE(FormulaCompiler_RejectsWrongFunctionArity) {
    const char* expressions[] = {
        "sin()",
        "sin(x,y)",
        "min(x)",
        "min(x,y,z)",
        "log()",
        "log(2,x,y)",
        "sin(cos())"
    };

    for (const char* expression : expressions) {
        const auto formula = compile(expression);
        Assert::IsFalse(formula.valid());
        Assert::IsFalse(formula.diagnostics.empty());
        Assert::IsTrue(
            formula.diagnostics[0].message.find("expects") != std::string::npos);
    }
}

TEST_CASE(FormulaCompiler_LogAcceptsSupportedArities) {
    Assert::IsTrue(compile("log(x)").valid());
    Assert::IsTrue(compile("log(2,x)").valid());
}

TEST_CASE(FormulaCompiler_LongExpressionsAreNotStorageLimited) {
    std::string expression = "x";
    for (int i = 0; i < 260; ++i) {
        expression += "+x";
    }

    const auto formula = XFExpression::compileFormula(expression);

    Assert::IsTrue(formula.valid());
    Assert::IsTrue(formula.kind == XFExpression::FormulaKind::Curve2D);
    Assert::IsTrue(expression.size() > 512);
}

} // namespace XpressFormulaTests
