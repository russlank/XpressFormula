// AstQueriesTests.cpp - Tests for shared AST query helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/Parser.h"
#include "../XpressFormula/Expression/AstQueries.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFExpression = XpressFormula::Expression;

namespace XpressFormulaTests {

TEST_CASE(AstQueries_CollectsVariablesOnce) {
    const auto parsed = XFCore::Parser::parse("sin(x) + x*y + z^2");
    Assert::IsTrue(parsed.success());

    const XFExpression::VariableSet variables =
        XFExpression::collectVariables(parsed.ast);

    Assert::AreEqual(3, static_cast<int>(variables.size()));
    Assert::IsTrue(variables.count("x") == 1);
    Assert::IsTrue(variables.count("y") == 1);
    Assert::IsTrue(variables.count("z") == 1);
}

TEST_CASE(AstQueries_ContainsVariableSearchesNestedNodes) {
    const auto parsed = XFCore::Parser::parse("sin(x + y^2)");
    Assert::IsTrue(parsed.success());

    Assert::IsTrue(XFExpression::containsVariable(parsed.ast, "x"));
    Assert::IsTrue(XFExpression::containsVariable(parsed.ast, "y"));
    Assert::IsFalse(XFExpression::containsVariable(parsed.ast, "z"));
}

TEST_CASE(AstQueries_IsVariableNodeChecksOnlySingleVariableNodes) {
    const auto variable = XFCore::Parser::parse("z");
    const auto expression = XFCore::Parser::parse("z + x");
    Assert::IsTrue(variable.success());
    Assert::IsTrue(expression.success());

    Assert::IsTrue(XFExpression::isVariableNode(variable.ast, "z"));
    Assert::IsFalse(XFExpression::isVariableNode(variable.ast, "x"));
    Assert::IsFalse(XFExpression::isVariableNode(expression.ast, "z"));
}

} // namespace XpressFormulaTests
