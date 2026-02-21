// Parser.cpp - Recursive-descent expression parser implementation.
#include "Parser.h"
#include "Tokenizer.h"
#include "MathConstants.h"

namespace XpressFormula::Core {

// ---- built-in names ---------------------------------------------------------
const std::set<std::string> Parser::s_builtinFunctions = {
    "sin",  "cos",  "tan",  "asin", "acos", "atan", "atan2",
    "sinh", "cosh", "tanh",
    "sqrt", "cbrt", "abs",  "ceil", "floor","round",
    "log",  "log2", "log10","exp",
    "min",  "max",  "pow",  "mod",  "sign"
};

const std::set<std::string> Parser::s_constants = { "pi", "e", "tau" };

// ---- construction -----------------------------------------------------------
Parser::Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {}

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

    collectVariables(result.ast, result.variables);
    return result;
}

// ---- grammar rules ----------------------------------------------------------
// expression := term (('+' | '-') term)*
ASTNodePtr Parser::parseExpression() {
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
    const Token& tok = current();

    // Numeric literal
    if (tok.type == TokenType::Number) {
        double value = std::stod(tok.value);
        advance();
        return std::make_shared<NumberNode>(value);
    }

    // Identifier: function call, constant, or variable
    if (tok.type == TokenType::Identifier) {
        std::string name = tok.value;
        size_t pos = tok.position;
        advance();

        // Function call?
        if (current().type == TokenType::LeftParen) {
            if (s_builtinFunctions.find(name) == s_builtinFunctions.end()) {
                m_error = "Unknown function '" + name +
                          "' at position " + std::to_string(pos);
                return nullptr;
            }
            advance(); // skip '('
            auto args = parseArgList();
            if (!m_error.empty()) return nullptr;
            if (!expect(TokenType::RightParen, "function call")) return nullptr;
            return std::make_shared<FunctionCallNode>(std::move(name), std::move(args));
        }

        // Known constant?
        if (s_constants.find(name) != s_constants.end()) {
            double value = 0.0;
            if (name == "pi")  value = PI;
            else if (name == "e")   value = E;
            else if (name == "tau") value = TAU;
            return std::make_shared<NumberNode>(value);
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
    std::vector<ASTNodePtr> args;
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

// ---- variable collection ----------------------------------------------------
void Parser::collectVariables(const ASTNodePtr& node, std::set<std::string>& vars) {
    if (!node) return;
    switch (node->type()) {
        case NodeType::Variable:
            vars.insert(static_cast<VariableNode*>(node.get())->name);
            break;
        case NodeType::BinaryOp: {
            auto* bin = static_cast<BinaryOpNode*>(node.get());
            collectVariables(bin->left, vars);
            collectVariables(bin->right, vars);
            break;
        }
        case NodeType::UnaryOp:
            collectVariables(static_cast<UnaryOpNode*>(node.get())->operand, vars);
            break;
        case NodeType::FunctionCall: {
            auto* fn = static_cast<FunctionCallNode*>(node.get());
            for (auto& arg : fn->arguments)
                collectVariables(arg, vars);
            break;
        }
        default: break;
    }
}

} // namespace XpressFormula::Core
