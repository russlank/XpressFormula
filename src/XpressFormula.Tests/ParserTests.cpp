// ParserTests.cpp - Unit tests for the expression parser.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/Parser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::Core;

namespace XpressFormulaTests {

TEST_CASE(Parse_Number) {
        auto r = Parser::parse("42");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::Number);
    }

    TEST_CASE(Parse_Variable) {
        auto r = Parser::parse("x");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::Variable);
        Assert::IsTrue(r.variables.count("x") == 1);
    }

    TEST_CASE(Parse_Addition) {
        auto r = Parser::parse("x + y");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::BinaryOp);
        Assert::IsTrue(r.variables.count("x") == 1);
        Assert::IsTrue(r.variables.count("y") == 1);
    }

    TEST_CASE(Parse_Multiplication) {
        auto r = Parser::parse("2 * x");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::BinaryOp);
    }

    TEST_CASE(Parse_Power) {
        auto r = Parser::parse("x ^ 2");
        Assert::IsTrue(r.success());
        auto* bin = static_cast<BinaryOpNode*>(r.ast.get());
        Assert::IsTrue(bin->op == BinaryOperator::Power);
    }

    TEST_CASE(Parse_PowerRightAssociative) {
        auto r = Parser::parse("2 ^ 3 ^ 4");
        Assert::IsTrue(r.success());
        // Should be 2 ^ (3 ^ 4), not (2 ^ 3) ^ 4
        auto* outer = static_cast<BinaryOpNode*>(r.ast.get());
        Assert::IsTrue(outer->op == BinaryOperator::Power);
        Assert::IsTrue(outer->right->type() == NodeType::BinaryOp);
    }

    TEST_CASE(Parse_UnaryNegation) {
        auto r = Parser::parse("-x");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::UnaryOp);
    }

    TEST_CASE(Parse_Parentheses) {
        auto r = Parser::parse("(x + y) * z");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::BinaryOp);
        auto* bin = static_cast<BinaryOpNode*>(r.ast.get());
        Assert::IsTrue(bin->op == BinaryOperator::Multiply);
        Assert::IsTrue(bin->left->type() == NodeType::BinaryOp);
    }

    TEST_CASE(Parse_FunctionCall_Sin) {
        auto r = Parser::parse("sin(x)");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::FunctionCall);
        auto* fn = static_cast<FunctionCallNode*>(r.ast.get());
        Assert::AreEqual(std::string("sin"), fn->name);
        Assert::AreEqual(size_t(1), fn->arguments.size());
    }

    TEST_CASE(Parse_FunctionCall_TwoArgs) {
        auto r = Parser::parse("atan2(y, x)");
        Assert::IsTrue(r.success());
        auto* fn = static_cast<FunctionCallNode*>(r.ast.get());
        Assert::AreEqual(std::string("atan2"), fn->name);
        Assert::AreEqual(size_t(2), fn->arguments.size());
    }

    TEST_CASE(Parse_NestedFunctions) {
        auto r = Parser::parse("sin(cos(x))");
        Assert::IsTrue(r.success());
        auto* outer = static_cast<FunctionCallNode*>(r.ast.get());
        Assert::AreEqual(std::string("sin"), outer->name);
        Assert::IsTrue(outer->arguments[0]->type() == NodeType::FunctionCall);
    }

    TEST_CASE(Parse_Constants) {
        auto r = Parser::parse("pi");
        Assert::IsTrue(r.success());
        Assert::IsTrue(r.ast->type() == NodeType::Number);
        Assert::IsTrue(r.variables.empty());
    }

    TEST_CASE(Parse_ComplexExpression) {
        auto r = Parser::parse("sin(x) * cos(y) + exp(-x^2 / 2)");
        Assert::IsTrue(r.success());
        Assert::AreEqual(size_t(2), r.variables.size());
    }

    TEST_CASE(Parse_ThreeVariables) {
        auto r = Parser::parse("x^2 + y^2 + z^2 - 4");
        Assert::IsTrue(r.success());
        Assert::AreEqual(size_t(3), r.variables.size());
        Assert::IsTrue(r.variables.count("x") == 1);
        Assert::IsTrue(r.variables.count("y") == 1);
        Assert::IsTrue(r.variables.count("z") == 1);
    }

    TEST_CASE(Parse_EmptyExpression) {
        auto r = Parser::parse("");
        Assert::IsFalse(r.success());
    }

    TEST_CASE(Parse_UnknownFunction) {
        auto r = Parser::parse("foo(x)");
        Assert::IsFalse(r.success());
        Assert::IsFalse(r.error.empty());
    }

    TEST_CASE(Parse_MismatchedParens) {
        auto r = Parser::parse("(x + y");
        Assert::IsFalse(r.success());
    }

    TEST_CASE(Parse_TrailingOperator) {
        auto r = Parser::parse("x +");
        Assert::IsFalse(r.success());
    }

TEST_CASE(Parse_OperatorPrecedence) {
        // 2 + 3 * 4 should parse as 2 + (3 * 4)
        auto r = Parser::parse("2 + 3 * 4");
        Assert::IsTrue(r.success());
        auto* add = static_cast<BinaryOpNode*>(r.ast.get());
        Assert::IsTrue(add->op == BinaryOperator::Add);
        Assert::IsTrue(add->right->type() == NodeType::BinaryOp);
        auto* mul = static_cast<BinaryOpNode*>(add->right.get());
        Assert::IsTrue(mul->op == BinaryOperator::Multiply);
}

// ----- Edge-case tests -----

TEST_CASE(Parse_UnaryPlus) {
    auto r = Parser::parse("+x");
    Assert::IsTrue(r.success());
    // Unary plus is parsed but transparent — the grammar discards it
    Assert::IsTrue(r.variables.count("x") == 1);
}

TEST_CASE(Parse_DoubleNegation) {
    auto r = Parser::parse("--x");
    Assert::IsTrue(r.success());
    // Outer is Negate, inner is Negate
    Assert::IsTrue(r.ast->type() == NodeType::UnaryOp);
    auto* outer = static_cast<UnaryOpNode*>(r.ast.get());
    Assert::IsTrue(outer->op == UnaryOperator::Negate);
    Assert::IsTrue(outer->operand->type() == NodeType::UnaryOp);
}

TEST_CASE(Parse_MixedUnary_PlusNegar) {
    // "+-x" should parse as +(-(x))
    auto r = Parser::parse("+-x");
    Assert::IsTrue(r.success());
    Assert::IsTrue(r.variables.count("x") == 1);
}

TEST_CASE(Parse_EmptyFunctionArgs) {
    // "sin()" — the parser accepts empty arg lists
    auto r = Parser::parse("sin()");
    Assert::IsTrue(r.success());
    auto* fn = static_cast<FunctionCallNode*>(r.ast.get());
    Assert::AreEqual(std::string("sin"), fn->name);
    Assert::AreEqual(size_t(0), fn->arguments.size());
}

TEST_CASE(Parse_FunctionThreeArgs) {
    // Parser allows any number of args; evaluator decides validity
    auto r = Parser::parse("min(x, y, z)");
    Assert::IsTrue(r.success());
    auto* fn = static_cast<FunctionCallNode*>(r.ast.get());
    Assert::AreEqual(size_t(3), fn->arguments.size());
}

TEST_CASE(Parse_DeeplyNestedParens) {
    auto r = Parser::parse("((((x))))");
    Assert::IsTrue(r.success());
    Assert::IsTrue(r.ast->type() == NodeType::Variable);
    Assert::IsTrue(r.variables.count("x") == 1);
}

TEST_CASE(Parse_ConstantE) {
    auto r = Parser::parse("e");
    Assert::IsTrue(r.success());
    Assert::IsTrue(r.ast->type() == NodeType::Number);
    Assert::IsTrue(r.variables.empty());
}

TEST_CASE(Parse_ConstantTau) {
    auto r = Parser::parse("tau");
    Assert::IsTrue(r.success());
    Assert::IsTrue(r.ast->type() == NodeType::Number);
    Assert::IsTrue(r.variables.empty());
}

TEST_CASE(Parse_Division) {
    auto r = Parser::parse("a / b");
    Assert::IsTrue(r.success());
    auto* bin = static_cast<BinaryOpNode*>(r.ast.get());
    Assert::IsTrue(bin->op == BinaryOperator::Divide);
}

TEST_CASE(Parse_SubtractionPrecedence) {
    // "2 - 3 + 4" should be left-to-right: (2-3)+4
    auto r = Parser::parse("2 - 3 + 4");
    Assert::IsTrue(r.success());
    auto* outer = static_cast<BinaryOpNode*>(r.ast.get());
    Assert::IsTrue(outer->op == BinaryOperator::Add);
    Assert::IsTrue(outer->left->type() == NodeType::BinaryOp);
    auto* inner = static_cast<BinaryOpNode*>(outer->left.get());
    Assert::IsTrue(inner->op == BinaryOperator::Subtract);
}

TEST_CASE(Parse_FunctionInExpression) {
    auto r = Parser::parse("1 + sin(x)");
    Assert::IsTrue(r.success());
    auto* bin = static_cast<BinaryOpNode*>(r.ast.get());
    Assert::IsTrue(bin->op == BinaryOperator::Add);
    Assert::IsTrue(bin->right->type() == NodeType::FunctionCall);
}

TEST_CASE(Parse_ExtraRightParen) {
    auto r = Parser::parse("(x + y))");
    Assert::IsFalse(r.success());
}

TEST_CASE(Parse_LeadingOperator_Star) {
    auto r = Parser::parse("* x");
    Assert::IsFalse(r.success());
}

TEST_CASE(Parse_ConsecutiveBinary) {
    auto r = Parser::parse("x + * y");
    Assert::IsFalse(r.success());
}

TEST_CASE(Parse_OnlyOperator) {
    auto r = Parser::parse("+");
    Assert::IsFalse(r.success());
}

TEST_CASE(Parse_NegatedExpression) {
    auto r = Parser::parse("-(x + y)");
    Assert::IsTrue(r.success());
    Assert::IsTrue(r.ast->type() == NodeType::UnaryOp);
    auto* un = static_cast<UnaryOpNode*>(r.ast.get());
    Assert::IsTrue(un->operand->type() == NodeType::BinaryOp);
}

TEST_CASE(Parse_AllBuiltinFunctions) {
    // Ensure all 25 builtin functions are accepted by the parser
    const char* fns[] = {
        "sin(x)", "cos(x)", "tan(x)", "asin(x)", "acos(x)", "atan(x)",
        "sinh(x)", "cosh(x)", "tanh(x)", "sqrt(x)", "cbrt(x)",
        "abs(x)", "ceil(x)", "floor(x)", "round(x)",
        "log(x)", "log2(x)", "log10(x)", "exp(x)", "sign(x)",
        "atan2(x,y)", "pow(x,y)", "min(x,y)", "max(x,y)", "mod(x,y)"
    };
    for (const char* fn : fns) {
        auto r = Parser::parse(fn);
        Assert::IsTrue(r.success(),
            (std::wstring(L"Failed to parse: ") + std::wstring(fn, fn + strlen(fn))).c_str());
    }
}

TEST_CASE(Parse_VariableCollection) {
    auto r = Parser::parse("sin(x) + cos(y) + z");
    Assert::IsTrue(r.success());
    Assert::AreEqual(size_t(3), r.variables.size());
    Assert::IsTrue(r.variables.count("x") == 1);
    Assert::IsTrue(r.variables.count("y") == 1);
    Assert::IsTrue(r.variables.count("z") == 1);
}

TEST_CASE(Parse_DuplicateVariables) {
    auto r = Parser::parse("x + x + x");
    Assert::IsTrue(r.success());
    Assert::AreEqual(size_t(1), r.variables.size()); // "x" only once
}

TEST_CASE(Parse_ConstantTimesVariable) {
    auto r = Parser::parse("pi * x");
    Assert::IsTrue(r.success());
    Assert::AreEqual(size_t(1), r.variables.size()); // pi is not a variable
    Assert::IsTrue(r.variables.count("x") == 1);
}

} // namespace XpressFormulaTests

