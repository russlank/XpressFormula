// Tokenizer.h - Converts a raw expression string into a sequence of tokens.
#pragma once

#include "Token.h"
#include <vector>
#include <string>

namespace XpressFormula::Core {

/// Splits a mathematical expression string into a list of Token objects.
class Tokenizer {
public:
    explicit Tokenizer(const std::string& input);

    /// Tokenize the entire input and return the token list (always ends with TokenType::End).
    std::vector<Token> tokenize();

    bool               hasError() const { return !m_error.empty(); }
    const std::string& error()    const { return m_error; }

private:
    Token readNumber();
    Token readIdentifier();
    void  skipWhitespace();

    std::string m_input;
    size_t      m_pos = 0;
    std::string m_error;
};

} // namespace XpressFormula::Core
