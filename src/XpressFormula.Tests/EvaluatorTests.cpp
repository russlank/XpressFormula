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

// ----- Edge-case tests -----

// --- Constants ---
TEST_CASE(Eval_Tau) {
    assertClose(TAU, eval("tau"));
}

// --- Hyperbolic functions ---
TEST_CASE(Eval_Sinh) {
    assertClose(std::sinh(1.0), eval("sinh(1)"));
}

TEST_CASE(Eval_Cosh) {
    assertClose(std::cosh(0.0), eval("cosh(0)"));
    assertClose(std::cosh(1.0), eval("cosh(1)"));
}

TEST_CASE(Eval_Tanh) {
    assertClose(std::tanh(1.0), eval("tanh(1)"));
    assertClose(0.0, eval("tanh(0)"));
}

// --- Inverse trig ---
TEST_CASE(Eval_Acos) {
    assertClose(0.0, eval("acos(1)"));
    assertClose(PI / 2.0, eval("acos(0)"));
}

TEST_CASE(Eval_Atan) {
    assertClose(0.0, eval("atan(0)"));
    assertClose(PI / 4.0, eval("atan(1)"));
}

// --- Log variants ---
TEST_CASE(Eval_Log2) {
    assertClose(3.0, eval("log2(8)"));
    assertClose(0.0, eval("log2(1)"));
}

TEST_CASE(Eval_Log10) {
    assertClose(2.0, eval("log10(100)"));
    assertClose(0.0, eval("log10(1)"));
}

TEST_CASE(Eval_Log2Negative) {
    double result = eval("log2(-1)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_Log10Negative) {
    double result = eval("log10(-1)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_LogZero) {
    double result = eval("log(0)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_TwoArgLog) {
    // log(base, value) = log(value) / log(base)
    assertClose(3.0, eval("log(2, 8)"));   // log_2(8) = 3
    assertClose(2.0, eval("log(10, 100)")); // log_10(100) = 2
}

TEST_CASE(Eval_TwoArgLog_InvalidBase) {
    // log(1, x) is undefined (division by zero in log)
    double result = eval("log(1, 8)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_TwoArgLog_NegativeBase) {
    double result = eval("log(-2, 8)");
    Assert::IsTrue(std::isnan(result));
}

// --- Mod edge cases ---
TEST_CASE(Eval_ModByZero) {
    double result = eval("mod(5, 0)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_ModNegative) {
    // fmod preserves sign of dividend
    assertClose(std::fmod(-7.0, 3.0), eval("mod(-7, 3)"));
}

// --- Power edge cases ---
TEST_CASE(Eval_PowZeroZero) {
    // std::pow(0, 0) = 1.0 per IEEE
    assertClose(1.0, eval("pow(0, 0)"));
}

TEST_CASE(Eval_PowNegativeFractional) {
    // pow(-1, 0.5) = NaN (square root of negative)
    double result = eval("pow(-1, 0.5)");
    Assert::IsTrue(std::isnan(result));
}

TEST_CASE(Eval_CaretPowZeroZero) {
    // Also test with ^ operator
    assertClose(1.0, eval("0 ^ 0"));
}

// --- Cbrt edge cases ---
TEST_CASE(Eval_CbrtNegative) {
    // cbrt(-8) should be -2 (cube root of negative is negative)
    assertClose(-2.0, eval("cbrt(-8)"));
}

TEST_CASE(Eval_CbrtZero) {
    assertClose(0.0, eval("cbrt(0)"));
}

// --- Scientific notation in evaluation ---
TEST_CASE(Eval_ScientificNotation) {
    assertClose(150.0, eval("1.5e2"));
    assertClose(0.0015, eval("1.5e-3"));
}

// --- Arity edge cases (recent fix) ---
TEST_CASE(Eval_SingleArgFuncWithTwoArgs) {
    // sin(x, y) with 2 args should fall back to sin(x) per recent fix
    // Parser allows arbitrary args; evaluator handles gracefully
    auto r = Parser::parse("sin(x, y)");
    Assert::IsTrue(r.success());
    Evaluator::Variables vars = { {"x", 0.0}, {"y", 999.0} };
    double result = Evaluator::evaluate(r.ast, vars);
    assertClose(0.0, result); // sin(0) = 0, y is ignored
}

TEST_CASE(Eval_SingleArgFuncWithThreeArgs) {
    auto r = Parser::parse("abs(x, y, z)");
    Assert::IsTrue(r.success());
    Evaluator::Variables vars = { {"x", -5.0}, {"y", 1.0}, {"z", 2.0} };
    double result = Evaluator::evaluate(r.ast, vars);
    assertClose(5.0, result); // abs(-5) = 5, y and z ignored
}

TEST_CASE(Eval_EmptyArgFunction) {
    // sin() with no args → NaN (args.empty() returns NaN)
    auto r = Parser::parse("sin()");
    Assert::IsTrue(r.success());
    double result = Evaluator::evaluate(r.ast, {});
    Assert::IsTrue(std::isnan(result));
}

// --- Unary edge cases ---
TEST_CASE(Eval_UnaryPlus) {
    assertClose(5.0, eval("+5"));
}

TEST_CASE(Eval_DoubleNegation) {
    assertClose(5.0, eval("--5"));
}

TEST_CASE(Eval_UnaryPlusNegate) {
    assertClose(-5.0, eval("+-5"));
}

// --- Deep nesting ---
TEST_CASE(Eval_DeeplyNestedParens) {
    assertClose(42.0, eval("((((((42))))))"));
}

// --- Chained operations ---
TEST_CASE(Eval_ChainedAddition) {
    assertClose(15.0, eval("1 + 2 + 3 + 4 + 5"));
}

TEST_CASE(Eval_ChainedMultiplication) {
    assertClose(120.0, eval("1 * 2 * 3 * 4 * 5"));
}

// --- Exp edge cases ---
TEST_CASE(Eval_ExpZero) {
    assertClose(1.0, eval("exp(0)"));
}

// --- Rounding functions ---
TEST_CASE(Eval_CeilNegative) {
    assertClose(-2.0, eval("ceil(-2.3)"));
}

TEST_CASE(Eval_FloorNegative) {
    assertClose(-3.0, eval("floor(-2.3)"));
}

TEST_CASE(Eval_RoundHalfDown) {
    // std::round rounds away from zero for .5
    assertClose(4.0, eval("round(3.5)"));
    assertClose(-4.0, eval("round(-3.5)"));
}

// --- Sign edge cases ---
TEST_CASE(Eval_SignNegative) {
    assertClose(-1.0, eval("sign(-42)"));
}

// --- Nested function calls ---
TEST_CASE(Eval_NestedFunctions) {
    // sin(cos(0)) = sin(1)
    assertClose(std::sin(1.0), eval("sin(cos(0))"));
}

TEST_CASE(Eval_FunctionOfExpression) {
    // sqrt(3^2 + 4^2) = sqrt(25) = 5
    assertClose(5.0, eval("sqrt(3^2 + 4^2)"));
}

// --- Multiple variables ---
TEST_CASE(Eval_ThreeVarPartiallyDefined) {
    // When 'z' is missing, result should be NaN
    double result = eval("x + y + z", { {"x", 1.0}, {"y", 2.0} });
    Assert::IsTrue(std::isnan(result));
}

// --- Edge precision ---
TEST_CASE(Eval_PiPrecision) {
    // Verify pi has sufficient precision
    assertClose(3.14159265358979323846, eval("pi"), 1e-15);
}

TEST_CASE(Eval_EulerPrecision) {
    assertClose(2.71828182845904523536, eval("e"), 1e-15);
}

} // namespace XpressFormulaTests

