// Tokenizer.cpp - Implementation of the expression tokenizer.
#include "Tokenizer.h"
#include <cctype>

namespace XpressFormula::Core {

Tokenizer::Tokenizer(const std::string& input) : m_input(input) {}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (m_pos < m_input.size()) {
        skipWhitespace();
        if (m_pos >= m_input.size()) break;

        char c = m_input[m_pos];

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            tokens.push_back(readNumber());
        }
        else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifier());
        }
        else {
            TokenType type;
            switch (c) {
                case '+': type = TokenType::Plus;       break;
                case '-': type = TokenType::Minus;      break;
                case '*': type = TokenType::Star;       break;
                case '/': type = TokenType::Slash;      break;
                case '^': type = TokenType::Caret;      break;
                case '(': type = TokenType::LeftParen;  break;
                case ')': type = TokenType::RightParen; break;
                case ',': type = TokenType::Comma;      break;
                default:
                    m_error = "Unexpected character '" + std::string(1, c) +
                              "' at position " + std::to_string(m_pos);
                    tokens.emplace_back(TokenType::Error, std::string(1, c), m_pos);
                    m_pos++;
                    return tokens;
            }
            tokens.emplace_back(type, std::string(1, c), m_pos);
            m_pos++;
        }
    }

    tokens.emplace_back(TokenType::End, "", m_pos);
    return tokens;
}

Token Tokenizer::readNumber() {
    size_t start = m_pos;
    bool hasDot = false;

    while (m_pos < m_input.size()) {
        char c = m_input[m_pos];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            m_pos++;
        } else if (c == '.' && !hasDot) {
            hasDot = true;
            m_pos++;
        } else {
            break;
        }
    }

    // Scientific notation: 1e5, 2.3e-4
    // Only consume the 'e'/'E' (and optional sign) if at least one digit follows,
    // so that inputs like "1e" or "1e-" remain valid number tokens (the trailing
    // letter will be tokenized separately, producing a clear parse error later).
    if (m_pos < m_input.size() && (m_input[m_pos] == 'e' || m_input[m_pos] == 'E')) {
        size_t peek = m_pos + 1;
        if (peek < m_input.size() && (m_input[peek] == '+' || m_input[peek] == '-'))
            peek++;
        if (peek < m_input.size() && std::isdigit(static_cast<unsigned char>(m_input[peek]))) {
            m_pos = peek;
            while (m_pos < m_input.size() && std::isdigit(static_cast<unsigned char>(m_input[m_pos])))
                m_pos++;
        }
    }

    return Token(TokenType::Number, m_input.substr(start, m_pos - start), start);
}

Token Tokenizer::readIdentifier() {
    size_t start = m_pos;
    while (m_pos < m_input.size() &&
           (std::isalnum(static_cast<unsigned char>(m_input[m_pos])) || m_input[m_pos] == '_'))
        m_pos++;
    return Token(TokenType::Identifier, m_input.substr(start, m_pos - start), start);
}

void Tokenizer::skipWhitespace() {
    while (m_pos < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_pos])))
        m_pos++;
}

} // namespace XpressFormula::Core
