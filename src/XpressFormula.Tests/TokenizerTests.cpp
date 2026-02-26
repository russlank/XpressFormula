// TokenizerTests.cpp - Unit tests for the expression tokenizer.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/Tokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::Core;

namespace XpressFormulaTests {

TEST_CASE(Tokenize_SimpleNumber) {
        Tokenizer t("42");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::AreEqual(size_t(2), tokens.size()); // Number + End
        Assert::IsTrue(tokens[0].type == TokenType::Number);
        Assert::AreEqual(std::string("42"), tokens[0].value);
    }

    TEST_CASE(Tokenize_DecimalNumber) {
        Tokenizer t("3.14");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::IsTrue(tokens[0].type == TokenType::Number);
        Assert::AreEqual(std::string("3.14"), tokens[0].value);
    }

    TEST_CASE(Tokenize_ScientificNotation) {
        Tokenizer t("1.5e-3");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::IsTrue(tokens[0].type == TokenType::Number);
        Assert::AreEqual(std::string("1.5e-3"), tokens[0].value);
    }

    TEST_CASE(Tokenize_Identifier) {
        Tokenizer t("sin");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::IsTrue(tokens[0].type == TokenType::Identifier);
        Assert::AreEqual(std::string("sin"), tokens[0].value);
    }

    TEST_CASE(Tokenize_Operators) {
        Tokenizer t("+-*/^");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::AreEqual(size_t(6), tokens.size()); // 5 ops + End
        Assert::IsTrue(tokens[0].type == TokenType::Plus);
        Assert::IsTrue(tokens[1].type == TokenType::Minus);
        Assert::IsTrue(tokens[2].type == TokenType::Star);
        Assert::IsTrue(tokens[3].type == TokenType::Slash);
        Assert::IsTrue(tokens[4].type == TokenType::Caret);
    }

    TEST_CASE(Tokenize_Parentheses) {
        Tokenizer t("(x)");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::IsTrue(tokens[0].type == TokenType::LeftParen);
        Assert::IsTrue(tokens[1].type == TokenType::Identifier);
        Assert::IsTrue(tokens[2].type == TokenType::RightParen);
    }

    TEST_CASE(Tokenize_Comma) {
        Tokenizer t("atan2(y,x)");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        // atan2 ( y , x ) End = 7 tokens
        Assert::AreEqual(size_t(7), tokens.size());
        Assert::IsTrue(tokens[3].type == TokenType::Comma);
    }

    TEST_CASE(Tokenize_Whitespace) {
        Tokenizer t("  x  +  y  ");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::AreEqual(size_t(4), tokens.size()); // x + y End
    }

    TEST_CASE(Tokenize_Expression) {
        Tokenizer t("sin(x) + 2.0 * y");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        // sin ( x ) + 2.0 * y End = 9 tokens
        Assert::AreEqual(size_t(9), tokens.size());
    }

    TEST_CASE(Tokenize_UnexpectedCharacter) {
        Tokenizer t("x @ y");
        auto tokens = t.tokenize();
        Assert::IsTrue(t.hasError());
    }

    TEST_CASE(Tokenize_EmptyInput) {
        Tokenizer t("");
        auto tokens = t.tokenize();
        Assert::IsFalse(t.hasError());
        Assert::AreEqual(size_t(1), tokens.size()); // Just End
        Assert::IsTrue(tokens[0].type == TokenType::End);
    }

TEST_CASE(Tokenize_Position) {
        Tokenizer t("x + y");
        auto tokens = t.tokenize();
        Assert::AreEqual(size_t(0), tokens[0].position); // x at 0
        Assert::AreEqual(size_t(2), tokens[1].position); // + at 2
        Assert::AreEqual(size_t(4), tokens[2].position); // y at 4
}

// ----- Edge-case tests -----

TEST_CASE(Tokenize_LeadingDotNumber) {
    Tokenizer t(".5");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string(".5"), tokens[0].value);
}

TEST_CASE(Tokenize_LoneDot) {
    // A single '.' should tokenize as a number token containing just "."
    Tokenizer t(".");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("."), tokens[0].value);
}

TEST_CASE(Tokenize_MalformedScientific_NoDigitAfterE) {
    // "1e" — the 'e' should NOT be consumed; token should be "1"
    Tokenizer t("1e");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("1"), tokens[0].value);
    Assert::IsTrue(tokens[1].type == TokenType::Identifier);
    Assert::AreEqual(std::string("e"), tokens[1].value);
}

TEST_CASE(Tokenize_MalformedScientific_SignNoDigit) {
    // "1e-" — neither 'e' nor '-' should be consumed into the number
    Tokenizer t("1e-");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("1"), tokens[0].value);
    Assert::IsTrue(tokens[1].type == TokenType::Identifier);
    Assert::AreEqual(std::string("e"), tokens[1].value);
    Assert::IsTrue(tokens[2].type == TokenType::Minus);
}

TEST_CASE(Tokenize_MalformedScientific_PlusNoDigit) {
    // "1e+" — same behavior as "1e-"
    Tokenizer t("1e+");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("1"), tokens[0].value);
    Assert::IsTrue(tokens[1].type == TokenType::Identifier);
    Assert::AreEqual(std::string("e"), tokens[1].value);
    Assert::IsTrue(tokens[2].type == TokenType::Plus);
}

TEST_CASE(Tokenize_UppercaseScientificNotation) {
    Tokenizer t("2E3");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("2E3"), tokens[0].value);
}

TEST_CASE(Tokenize_ScientificWithPlusSign) {
    Tokenizer t("1.5e+3");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("1.5e+3"), tokens[0].value);
}

TEST_CASE(Tokenize_UnderscoreIdentifier) {
    Tokenizer t("_x");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Identifier);
    Assert::AreEqual(std::string("_x"), tokens[0].value);
}

TEST_CASE(Tokenize_IdentifierWithUnderscore) {
    Tokenizer t("x_1");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Identifier);
    Assert::AreEqual(std::string("x_1"), tokens[0].value);
}

TEST_CASE(Tokenize_MultipleDots) {
    // "1.2.3" should be "1.2" then ".3"
    Tokenizer t("1.2.3");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("1.2"), tokens[0].value);
    Assert::IsTrue(tokens[1].type == TokenType::Number);
    Assert::AreEqual(std::string(".3"), tokens[1].value);
}

TEST_CASE(Tokenize_WhitespaceOnly) {
    Tokenizer t("   \t  ");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::AreEqual(size_t(1), tokens.size()); // Just End
    Assert::IsTrue(tokens[0].type == TokenType::End);
}

TEST_CASE(Tokenize_EqualsIsUnexpected) {
    Tokenizer t("x = y");
    auto tokens = t.tokenize();
    Assert::IsTrue(t.hasError());
    Assert::IsTrue(tokens.back().type == TokenType::Error);
}

TEST_CASE(Tokenize_LargeNumber) {
    Tokenizer t("123456789.987654321");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::IsTrue(tokens[0].type == TokenType::Number);
    Assert::AreEqual(std::string("123456789.987654321"), tokens[0].value);
}

TEST_CASE(Tokenize_ConsecutiveOperators) {
    Tokenizer t("+-*/");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::AreEqual(size_t(5), tokens.size()); // 4 ops + End
    Assert::IsTrue(tokens[0].type == TokenType::Plus);
    Assert::IsTrue(tokens[1].type == TokenType::Minus);
    Assert::IsTrue(tokens[2].type == TokenType::Star);
    Assert::IsTrue(tokens[3].type == TokenType::Slash);
}

TEST_CASE(Tokenize_NestedParentheses) {
    Tokenizer t("((()))");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    Assert::AreEqual(size_t(7), tokens.size()); // 3 left + 3 right + End
}

TEST_CASE(Tokenize_HashIsUnexpected) {
    Tokenizer t("x # 1");
    auto tokens = t.tokenize();
    Assert::IsTrue(t.hasError());
}

TEST_CASE(Tokenize_PositionTracking_Complex) {
    Tokenizer t("sin(x + 2.5)");
    auto tokens = t.tokenize();
    Assert::IsFalse(t.hasError());
    // sin=0, (=3, x=4, +=6, 2.5=8, )=11, End=12
    Assert::AreEqual(size_t(0), tokens[0].position);  // "sin"
    Assert::AreEqual(size_t(3), tokens[1].position);  // "("
    Assert::AreEqual(size_t(4), tokens[2].position);  // "x"
    Assert::AreEqual(size_t(6), tokens[3].position);  // "+"
    Assert::AreEqual(size_t(8), tokens[4].position);  // "2.5"
    Assert::AreEqual(size_t(11), tokens[5].position); // ")"
}

} // namespace XpressFormulaTests

