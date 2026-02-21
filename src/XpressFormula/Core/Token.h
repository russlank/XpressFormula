// Token.h - Defines token types used by the expression tokenizer and parser.
#pragma once

#include <string>

namespace XpressFormula::Core {

/// All token types recognized by the expression tokenizer.
enum class TokenType {
    Number,      // Numeric literal (e.g. 3.14, 1e-5)
    Identifier,  // Variable or function name (e.g. x, sin)
    Plus,        // +
    Minus,       // -
    Star,        // *
    Slash,       // /
    Caret,       // ^
    LeftParen,   // (
    RightParen,  // )
    Comma,       // ,
    End,         // End of input
    Error        // Tokenization error
};

/// A single token produced by the tokenizer.
struct Token {
    TokenType   type;
    std::string value;
    size_t      position; // Character offset in original input

    Token(TokenType t, std::string v, size_t pos)
        : type(t), value(std::move(v)), position(pos) {}
};

/// Returns a human-readable name for a TokenType (used in error messages).
inline const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::Number:     return "Number";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Plus:       return "+";
        case TokenType::Minus:      return "-";
        case TokenType::Star:       return "*";
        case TokenType::Slash:      return "/";
        case TokenType::Caret:      return "^";
        case TokenType::LeftParen:  return "(";
        case TokenType::RightParen: return ")";
        case TokenType::Comma:      return ",";
        case TokenType::End:        return "End";
        case TokenType::Error:      return "Error";
        default:                    return "Unknown";
    }
}

} // namespace XpressFormula::Core
