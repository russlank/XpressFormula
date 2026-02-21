// EvaluatorTests.cpp - Unit tests for the expression evaluator.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/Parser.h"
#include "../XpressFormula/Core/Evaluator.h"
#include "../XpressFormula/Core/MathConstants.h"
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::Core;

namespace XpressFormulaTests {

static double eval(const char* expr,
                   const Evaluator::Variables& vars = {}) {
    auto r = Parser::parse(expr);
    Assert::IsTrue(r.success(), L"Parse failed");
    return Evaluator::evaluate(r.ast, vars);
}

static void assertClose(double expected, double actual,
                        double tol = 1e-9) {
    Assert::IsTrue(std::abs(expected - actual) < tol,
        (std::to_wstring(expected) + L" != " + std::to_wstring(actual)).c_str());
}

// --- Literals ---
TEST_CASE(Eval_Number) {
    assertClose(42.0, eval("42"));
}

TEST_CASE(Eval_Decimal) {
    assertClose(3.14, eval("3.14"));
}

TEST_CASE(Eval_Pi) {
    assertClose(PI, eval("pi"));
}

TEST_CASE(Eval_Euler) {
    assertClose(E, eval("e"));
}

// --- Arithmetic ---
TEST_CASE(Eval_Addition) {
    assertClose(5.0, eval("2 + 3"));
}

TEST_CASE(Eval_Subtraction) {
    assertClose(-1.0, eval("2 - 3"));
}

    TEST_CASE(Eval_Multiplication) {
        assertClose(6.0, eval("2 * 3"));
    }

    TEST_CASE(Eval_Division) {
        assertClose(2.5, eval("5 / 2"));
    }

    TEST_CASE(Eval_DivisionByZero) {
        double result = eval("1 / 0");
        Assert::IsTrue(std::isnan(result));
    }

    TEST_CASE(Eval_Power) {
        assertClose(8.0, eval("2 ^ 3"));
    }

    TEST_CASE(Eval_UnaryNegate) {
        assertClose(-5.0, eval("-5"));
    }

    TEST_CASE(Eval_Precedence) {
        assertClose(14.0, eval("2 + 3 * 4"));
    }

    TEST_CASE(Eval_Parentheses) {
        assertClose(20.0, eval("(2 + 3) * 4"));
    }

    TEST_CASE(Eval_NestedOps) {
        assertClose(7.0, eval("1 + 2 * 3"));
    }

    // --- Variables ---
    TEST_CASE(Eval_Variable) {
        assertClose(5.0, eval("x", { {"x", 5.0} }));
    }

    TEST_CASE(Eval_TwoVariables) {
        assertClose(7.0, eval("x + y", { {"x", 3.0}, {"y", 4.0} }));
    }

    TEST_CASE(Eval_UnknownVariable) {
        double result = eval("x");
        Assert::IsTrue(std::isnan(result));
    }

    // --- Trig functions ---
    TEST_CASE(Eval_Sin) {
        assertClose(0.0, eval("sin(0)"));
    }

    TEST_CASE(Eval_SinPi) {
        assertClose(0.0, eval("sin(pi)"), 1e-12);
    }

    TEST_CASE(Eval_Cos) {
        assertClose(1.0, eval("cos(0)"));
    }

    TEST_CASE(Eval_Tan) {
        assertClose(0.0, eval("tan(0)"));
    }

    TEST_CASE(Eval_Asin) {
        assertClose(PI / 2.0, eval("asin(1)"));
    }

    TEST_CASE(Eval_Atan2) {
        assertClose(PI / 4.0, eval("atan2(1, 1)"));
    }

    // --- Other math functions ---
    TEST_CASE(Eval_Sqrt) {
        assertClose(3.0, eval("sqrt(9)"));
    }

    TEST_CASE(Eval_SqrtNegative) {
        double result = eval("sqrt(-1)");
        Assert::IsTrue(std::isnan(result));
    }

    TEST_CASE(Eval_Abs) {
        assertClose(5.0, eval("abs(-5)"));
    }

    TEST_CASE(Eval_Log) {
        assertClose(0.0, eval("log(1)"));
    }

    TEST_CASE(Eval_LogNegative) {
        double result = eval("log(-1)");
        Assert::IsTrue(std::isnan(result));
    }

    TEST_CASE(Eval_Exp) {
        assertClose(E, eval("exp(1)"));
    }

    TEST_CASE(Eval_Ceil) {
        assertClose(3.0, eval("ceil(2.3)"));
    }

    TEST_CASE(Eval_Floor) {
        assertClose(2.0, eval("floor(2.7)"));
    }

    TEST_CASE(Eval_Round) {
        assertClose(3.0, eval("round(2.5)"));
    }

    TEST_CASE(Eval_Min) {
        assertClose(2.0, eval("min(2, 5)"));
    }

    TEST_CASE(Eval_Max) {
        assertClose(5.0, eval("max(2, 5)"));
    }

    TEST_CASE(Eval_Pow) {
        assertClose(8.0, eval("pow(2, 3)"));
    }

    TEST_CASE(Eval_Mod) {
        assertClose(1.0, eval("mod(7, 3)"));
    }

    TEST_CASE(Eval_Sign) {
        assertClose(1.0, eval("sign(42)"));
        assertClose(-1.0, eval("sign(-3)"));
        assertClose(0.0, eval("sign(0)"));
    }

    TEST_CASE(Eval_Cbrt) {
        assertClose(3.0, eval("cbrt(27)"));
    }

    // --- Complex expressions ---
    TEST_CASE(Eval_ComplexFormula) {
        // f(x) = sin(x) * cos(x) at x = pi/4
        double x = PI / 4.0;
        double expected = std::sin(x) * std::cos(x);
        assertClose(expected, eval("sin(x) * cos(x)", { {"x", x} }));
    }

TEST_CASE(Eval_Gaussian) {
    // Parentheses make the intended precedence explicit.
    double expected = std::exp(-0.5);
    assertClose(expected, eval("exp(-(x^2) / 2)", { {"x", 1.0} }));
}

    TEST_CASE(Eval_TwoVarFormula) {
        // f(x,y) = x^2 + y^2 at (3,4) = 25
        assertClose(25.0, eval("x^2 + y^2",
                               { {"x", 3.0}, {"y", 4.0} }));
    }

    TEST_CASE(Eval_ThreeVarFormula) {
        // f(x,y,z) = x^2 + y^2 + z^2 - 1 at (1,0,0) = 0
        assertClose(0.0, eval("x^2 + y^2 + z^2 - 1",
                               { {"x", 1.0}, {"y", 0.0}, {"z", 0.0} }));
    }

TEST_CASE(Eval_NullAST) {
    double result = Evaluator::evaluate(nullptr, {});
    Assert::IsTrue(std::isnan(result));
}

} // namespace XpressFormulaTests

