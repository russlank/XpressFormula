// Parser.h - Recursive-descent parser that builds an AST from tokens.
#pragma once

#include "Token.h"
#include "ASTNode.h"
#include <vector>
#include <string>
#include <set>

namespace XpressFormula::Core {

/// Parses a mathematical expression string into an Abstract Syntax Tree.
class Parser {
public:
    /// The result of a parse attempt.
    struct Result {
        ASTNodePtr            ast;       // Root of the AST (nullptr on failure)
        std::string           error;     // Error message (empty on success)
        std::set<std::string> variables; // Variable names found in the expression
        bool success() const { return ast != nullptr && error.empty(); }
    };

    /// Parse the given expression string and return a Result.
    static Result parse(const std::string& expression);

private:
    explicit Parser(const std::vector<Token>& tokens);

    // Grammar rules (in order of increasing precedence)
    ASTNodePtr              parseExpression();
    ASTNodePtr              parseTerm();
    ASTNodePtr              parsePower();
    ASTNodePtr              parseUnary();
    ASTNodePtr              parsePrimary();
    std::vector<ASTNodePtr> parseArgList();

    // Token stream helpers
    const Token& current() const;
    const Token& advance();
    bool         match(TokenType type);
    bool         expect(TokenType type, const std::string& context);

    // Utility: walk the AST and collect variable names
    static void collectVariables(const ASTNodePtr& node, std::set<std::string>& vars);

    std::vector<Token> m_tokens;
    size_t             m_pos = 0;
    std::string        m_error;

    static const std::set<std::string> s_builtinFunctions;
    static const std::set<std::string> s_constants;
};

} // namespace XpressFormula::Core
