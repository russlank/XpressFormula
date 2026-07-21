// Parser.cpp - Recursive-descent expression parser implementation.
#include "Parser.h"
#include "InputLimits.h"
#include "Tokenizer.h"
#include "ConstantRegistry.h"
#include "FunctionRegistry.h"
#include "../Expression/AstQueries.h"

#include <charconv>
#include <cmath>
#include <string_view>
#include <system_error>

namespace XpressFormula::Core {

namespace {

struct NumberLiteralParseResult {
    double value = 0.0;
    std::string error;

    [[nodiscard]] bool success() const {
        return error.empty();
    }
};

bool containsDecimalDigit(std::string_view text) noexcept {
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            return true;
        }
    }
    return false;
}

bool significandContainsNonZeroDigit(std::string_view text) noexcept {
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
        text.remove_prefix(1);
    }

    for (char ch : text) {
        if (ch == 'e' || ch == 'E') {
            break;
        }
        if (ch == '.') {
            continue;
        }
        if (ch >= '1' && ch <= '9') {
            return true;
        }
    }
    return false;
}

NumberLiteralParseResult parseNumberLiteral(std::string_view text) {
    NumberLiteralParseResult result;

    if (!containsDecimalDigit(text)) {
        result.error = "Invalid numeric literal";
        return result;
    }

    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result parsed =
        std::from_chars(begin, end, result.value, std::chars_format::general);
    const bool nonZeroSignificand = significandContainsNonZeroDigit(text);

    if (parsed.ec == std::errc::invalid_argument || parsed.ptr != end) {
        result.error = "Invalid numeric literal";
        return result;
    }

    if ((parsed.ec == std::errc::result_out_of_range &&
         (result.value != 0.0 || nonZeroSignificand)) ||
        !std::isfinite(result.value) ||
        (result.value == 0.0 && nonZeroSignificand)) {
        result.error = "Numeric literal is outside the supported range";
        return result;
    }

    return result;
}

std::string arityText(int minArity, int maxArity) {
    if (minArity == maxArity) {
        std::string text = std::to_string(minArity);
        text += (minArity == 1) ? " argument" : " arguments";
        return text;
    }

    return std::to_string(minArity) + " to " + std::to_string(maxArity) + " arguments";
}

std::string functionArityError(const FunctionInfo& function,
                               std::size_t actualArity,
                               std::size_t position) {
    std::string message = "Function '";
    message += function.name;
    message += "' expects ";
    message += arityText(function.minArity, function.maxArity);
    message += ", but ";
    message += std::to_string(actualArity);
    message += (actualArity == 1) ? " was provided" : " were provided";
    message += " at position ";
    message += std::to_string(position);
    return message;
}

} // namespace

// ---- construction -----------------------------------------------------------
Parser::Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {}

struct Parser::RecursionScope {
    explicit RecursionScope(Parser& parser)
        : parser(parser) {
        if (parser.m_recursionDepth >= InputLimits::kMaxExpressionNesting) {
            if (parser.m_error.empty()) {
                parser.m_error = "Expression nesting is too deep to parse safely";
            }
            return;
        }
        ++parser.m_recursionDepth;
        entered = true;
    }

    RecursionScope(const RecursionScope&) = delete;
    RecursionScope& operator=(const RecursionScope&) = delete;

    ~RecursionScope() {
        if (entered && parser.m_recursionDepth > 0) {
            --parser.m_recursionDepth;
        }
    }

    [[nodiscard]] bool ok() const noexcept {
        return entered;
    }

    Parser& parser;
    bool entered = false;
};

// ---- public entry point -----------------------------------------------------
Parser::Result Parser::parse(const std::string& expression) {
    Result result;

    if (expression.empty()) {
        result.error = "Empty expression";
        return result;
    }

    Tokenizer tokenizer(expression);
    auto tokens = tokenizer.tokenize();

    if (tokenizer.hasError()) {
        result.error = tokenizer.error();
        return result;
    }

    Parser parser(tokens);
    result.ast = parser.parseExpression();

    if (!parser.m_error.empty()) {
        result.error = parser.m_error;
        result.ast   = nullptr;
        return result;
    }

    if (parser.current().type != TokenType::End) {
        result.error = "Unexpected token '" + parser.current().value +
                       "' at position " + std::to_string(parser.current().position);
        result.ast = nullptr;
        return result;
    }

    result.variables = Expression::collectVariables(result.ast);
    return result;
}

// ---- grammar rules ----------------------------------------------------------
// expression := term (('+' | '-') term)*
ASTNodePtr Parser::parseExpression() {
    RecursionScope scope(*this);
    if (!scope.ok()) return nullptr;

    auto left = parseTerm();
    if (!left) return nullptr;

    while (current().type == TokenType::Plus || current().type == TokenType::Minus) {
        BinaryOperator op = (current().type == TokenType::Plus)
                                ? BinaryOperator::Add
                                : BinaryOperator::Subtract;
        advance();
        auto right = parseTerm();
        if (!right) return nullptr;
        left = std::make_shared<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

// term := power (('*' | '/') power)*
ASTNodePtr Parser::parseTerm() {
    RecursionScope scope(*this);
    if (!scope.ok()) return nullptr;

    auto left = parsePower();
    if (!left) return nullptr;

    while (current().type == TokenType::Star || current().type == TokenType::Slash) {
        BinaryOperator op = (current().type == TokenType::Star)
                                ? BinaryOperator::Multiply
                                : BinaryOperator::Divide;
        advance();
        auto right = parsePower();
        if (!right) return nullptr;
        left = std::make_shared<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

// power := unary ('^' power)?          -- right-associative
ASTNodePtr Parser::parsePower() {
    RecursionScope scope(*this);
    if (!scope.ok()) return nullptr;

    auto base = parseUnary();
    if (!base) return nullptr;

    if (current().type == TokenType::Caret) {
        advance();
        auto exponent = parsePower();          // right-associative recursion
        if (!exponent) return nullptr;
        return std::make_shared<BinaryOpNode>(BinaryOperator::Power,
                                              std::move(base), std::move(exponent));
    }
    return base;
}

// unary := ('-' | '+')? primary
ASTNodePtr Parser::parseUnary() {
    RecursionScope scope(*this);
    if (!scope.ok()) return nullptr;

    if (current().type == TokenType::Minus) {
        advance();
        auto operand = parseUnary();
        if (!operand) return nullptr;
        return std::make_shared<UnaryOpNode>(UnaryOperator::Negate, std::move(operand));
    }
    if (current().type == TokenType::Plus) {
        advance();
        return parseUnary();
    }
    return parsePrimary();
}

// primary := NUMBER
//          | IDENTIFIER '(' arglist ')'   -- function call
//          | IDENTIFIER                   -- constant or variable
//          | '(' expression ')'
ASTNodePtr Parser::parsePrimary() {
    RecursionScope scope(*this);
    if (!scope.ok()) return nullptr;

    const Token& tok = current();

    // Numeric literal
    if (tok.type == TokenType::Number) {
        const NumberLiteralParseResult parsed = parseNumberLiteral(tok.value);
        if (!parsed.success()) {
            m_error = parsed.error + " '" + tok.value +
                      "' at position " + std::to_string(tok.position);
            return nullptr;
        }
        advance();
        return std::make_shared<NumberNode>(parsed.value);
    }

    // Identifier: function call, constant, or variable
    if (tok.type == TokenType::Identifier) {
        std::string name = tok.value;
        size_t pos = tok.position;
        advance();

        // Function call?
        if (current().type == TokenType::LeftParen) {
            const FunctionInfo* function = findFunctionInfo(name);
            if (!function) {
                m_error = "Unknown function '" + name +
                          "' at position " + std::to_string(pos);
                return nullptr;
            }
            advance(); // skip '('
            auto args = parseArgList();
            if (!m_error.empty()) return nullptr;
            if (!expect(TokenType::RightParen, "function call")) return nullptr;
            if (!functionAcceptsArity(*function, args.size())) {
                m_error = functionArityError(*function, args.size(), pos);
                return nullptr;
            }
            return std::make_shared<FunctionCallNode>(
                std::move(name), std::move(args), function);
        }

        // Known constant?
        if (const ConstantInfo* constant = findConstantInfo(name)) {
            return std::make_shared<NumberNode>(constant->value);
        }

        // Variable
        return std::make_shared<VariableNode>(std::move(name));
    }

    // Parenthesized sub-expression
    if (tok.type == TokenType::LeftParen) {
        advance();
        auto expr = parseExpression();
        if (!expr) return nullptr;
        if (!expect(TokenType::RightParen, "parenthesized expression")) return nullptr;
        return expr;
    }

    m_error = "Unexpected token '" + tok.value +
              "' at position " + std::to_string(tok.position);
    return nullptr;
}

// arglist := expression (',' expression)*
std::vector<ASTNodePtr> Parser::parseArgList() {
    RecursionScope scope(*this);
    std::vector<ASTNodePtr> args;
    if (!scope.ok()) return args;

    if (current().type == TokenType::RightParen)
        return args; // empty list

    auto first = parseExpression();
    if (!first) return args;
    args.push_back(std::move(first));

    while (current().type == TokenType::Comma) {
        advance();
        auto arg = parseExpression();
        if (!arg) return args;
        args.push_back(std::move(arg));
    }
    return args;
}

// ---- token stream helpers ---------------------------------------------------
const Token& Parser::current() const { return m_tokens[m_pos]; }

const Token& Parser::advance() {
    const Token& tok = m_tokens[m_pos];
    if (m_pos < m_tokens.size() - 1) m_pos++;
    return tok;
}

bool Parser::match(TokenType type) {
    if (current().type == type) { advance(); return true; }
    return false;
}

bool Parser::expect(TokenType type, const std::string& context) {
    if (current().type == type) { advance(); return true; }
    m_error = "Expected '" + std::string(tokenTypeName(type)) +
              "' in " + context +
              " at position " + std::to_string(current().position) +
              ", got '" + current().value + "'";
    return false;
}

} // namespace XpressFormula::Core
