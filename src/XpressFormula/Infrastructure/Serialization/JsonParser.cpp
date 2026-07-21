// SPDX-License-Identifier: MIT
// JsonParser.cpp - Strict internal JSON parser implementation.
#include "JsonParser.h"
#include "../../Core/InputLimits.h"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace XpressFormula::Infrastructure::Serialization {

JsonParser::JsonParser(std::string_view input)
    : m_input(input) {
}

bool JsonParser::parse(JsonValue& value, std::string& error) {
    m_pos = 0;
    m_depth = 0;
    m_valueCount = 0;
    m_error.clear();
    error.clear();
    value = JsonValue{};

    skipWhitespace();
    if (!parseValue(value)) {
        error = m_error.empty() ? "Invalid JSON." : m_error;
        return false;
    }
    skipWhitespace();
    if (m_pos != m_input.size()) {
        error = "Unexpected trailing data in JSON.";
        return false;
    }
    return true;
}

void JsonParser::skipWhitespace() {
    while (m_pos < m_input.size() &&
           std::isspace(static_cast<unsigned char>(m_input[m_pos])) != 0) {
        ++m_pos;
    }
}

bool JsonParser::consume(char expected) {
    skipWhitespace();
    if (m_pos >= m_input.size() || m_input[m_pos] != expected) {
        return false;
    }
    ++m_pos;
    return true;
}

bool JsonParser::parseValue(JsonValue& value) {
    skipWhitespace();
    if (m_pos >= m_input.size()) {
        m_error = "Unexpected end of JSON.";
        return false;
    }
    if (!trackValue()) {
        return false;
    }

    const char ch = m_input[m_pos];
    if (ch == '{') {
        return parseObject(value);
    }
    if (ch == '[') {
        return parseArray(value);
    }
    if (ch == '"') {
        value.type = JsonValue::Type::String;
        return parseString(value.text);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        value.type = JsonValue::Type::Number;
        return parseNumber(value.number);
    }
    if (matchLiteral("true")) {
        value.type = JsonValue::Type::Bool;
        value.boolean = true;
        return true;
    }
    if (matchLiteral("false")) {
        value.type = JsonValue::Type::Bool;
        value.boolean = false;
        return true;
    }
    if (matchLiteral("null")) {
        value.type = JsonValue::Type::Null;
        return true;
    }

    m_error = "Unexpected JSON token.";
    return false;
}

bool JsonParser::parseObject(JsonValue& value) {
    if (!enterContainer()) {
        return false;
    }
    if (!consume('{')) {
        m_error = "Expected object.";
        leaveContainer();
        return false;
    }
    value.type = JsonValue::Type::Object;
    value.object.clear();
    skipWhitespace();
    if (m_pos < m_input.size() && m_input[m_pos] == '}') {
        ++m_pos;
        leaveContainer();
        return true;
    }

    for (;;) {
        std::string key;
        if (!parseString(key)) {
            m_error = "Expected object key.";
            leaveContainer();
            return false;
        }
        if (!consume(':')) {
            m_error = "Expected ':' after object key.";
            leaveContainer();
            return false;
        }
        if (value.object.find(key) != value.object.end()) {
            m_error = "Duplicate JSON object key.";
            leaveContainer();
            return false;
        }
        JsonValue member;
        if (!parseValue(member)) {
            leaveContainer();
            return false;
        }
        value.object[std::move(key)] = std::move(member);

        skipWhitespace();
        if (m_pos < m_input.size() && m_input[m_pos] == '}') {
            ++m_pos;
            leaveContainer();
            return true;
        }
        if (!consume(',')) {
            m_error = "Expected ',' or '}' in object.";
            leaveContainer();
            return false;
        }
    }
}

bool JsonParser::parseArray(JsonValue& value) {
    if (!enterContainer()) {
        return false;
    }
    if (!consume('[')) {
        m_error = "Expected array.";
        leaveContainer();
        return false;
    }
    value.type = JsonValue::Type::Array;
    value.array.clear();
    skipWhitespace();
    if (m_pos < m_input.size() && m_input[m_pos] == ']') {
        ++m_pos;
        leaveContainer();
        return true;
    }

    for (;;) {
        JsonValue item;
        if (!parseValue(item)) {
            leaveContainer();
            return false;
        }
        value.array.push_back(std::move(item));

        skipWhitespace();
        if (m_pos < m_input.size() && m_input[m_pos] == ']') {
            ++m_pos;
            leaveContainer();
            return true;
        }
        if (!consume(',')) {
            m_error = "Expected ',' or ']' in array.";
            leaveContainer();
            return false;
        }
    }
}

bool JsonParser::enterContainer() {
    if (m_depth >= Core::InputLimits::kMaxJsonDepth) {
        m_error = "JSON nesting depth exceeds the supported limit.";
        return false;
    }
    ++m_depth;
    return true;
}

void JsonParser::leaveContainer() noexcept {
    if (m_depth > 0) {
        --m_depth;
    }
}

bool JsonParser::trackValue() {
    if (m_valueCount >= Core::InputLimits::kMaxJsonValues) {
        m_error = "JSON value count exceeds the supported limit.";
        return false;
    }
    ++m_valueCount;
    return true;
}

int JsonParser::hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

bool JsonParser::parseHexQuad(std::uint32_t& codeUnit) {
    if (m_pos + 4 > m_input.size()) {
        m_error = "Incomplete JSON unicode escape.";
        return false;
    }

    codeUnit = 0;
    for (int i = 0; i < 4; ++i) {
        const int value = hexValue(m_input[m_pos++]);
        if (value < 0) {
            m_error = "Invalid JSON unicode escape.";
            return false;
        }
        codeUnit = (codeUnit << 4) | static_cast<std::uint32_t>(value);
    }
    return true;
}

bool JsonParser::appendUtf8(std::string& text, std::uint32_t codePoint) {
    if (codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        m_error = "Invalid JSON unicode code point.";
        return false;
    }

    if (codePoint <= 0x7F) {
        text.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    return true;
}

bool JsonParser::appendRawUtf8(std::string& text, unsigned char lead) {
    auto fail = [&]() {
        m_error = "Invalid raw UTF-8 in JSON string.";
        return false;
    };

    auto readContinuation = [&]() -> int {
        if (m_pos >= m_input.size()) {
            return -1;
        }
        const unsigned char value = static_cast<unsigned char>(m_input[m_pos]);
        if ((value & 0xC0u) != 0x80u) {
            return -1;
        }
        ++m_pos;
        text.push_back(static_cast<char>(value));
        return static_cast<int>(value);
    };

    text.push_back(static_cast<char>(lead));
    if (lead >= 0xC2u && lead <= 0xDFu) {
        return readContinuation() >= 0 || fail();
    }

    int continuationCount = 0;
    if (lead == 0xE0u) {
        if (m_pos >= m_input.size()) {
            return fail();
        }
        const unsigned char second = static_cast<unsigned char>(m_input[m_pos]);
        if (second < 0xA0u || second > 0xBFu) {
            return fail();
        }
        continuationCount = 2;
    } else if (lead >= 0xE1u && lead <= 0xECu) {
        continuationCount = 2;
    } else if (lead == 0xEDu) {
        if (m_pos >= m_input.size()) {
            return fail();
        }
        const unsigned char second = static_cast<unsigned char>(m_input[m_pos]);
        if (second < 0x80u || second > 0x9Fu) {
            return fail();
        }
        continuationCount = 2;
    } else if (lead >= 0xEEu && lead <= 0xEFu) {
        continuationCount = 2;
    } else if (lead == 0xF0u) {
        if (m_pos >= m_input.size()) {
            return fail();
        }
        const unsigned char second = static_cast<unsigned char>(m_input[m_pos]);
        if (second < 0x90u || second > 0xBFu) {
            return fail();
        }
        continuationCount = 3;
    } else if (lead >= 0xF1u && lead <= 0xF3u) {
        continuationCount = 3;
    } else if (lead == 0xF4u) {
        if (m_pos >= m_input.size()) {
            return fail();
        }
        const unsigned char second = static_cast<unsigned char>(m_input[m_pos]);
        if (second < 0x80u || second > 0x8Fu) {
            return fail();
        }
        continuationCount = 3;
    } else {
        return fail();
    }

    for (int i = 0; i < continuationCount; ++i) {
        if (readContinuation() < 0) {
            return fail();
        }
    }
    return true;
}

bool JsonParser::parseUnicodeEscape(std::string& text) {
    std::uint32_t first = 0;
    if (!parseHexQuad(first)) {
        return false;
    }

    if (first >= 0xD800 && first <= 0xDBFF) {
        if (m_pos + 2 > m_input.size() ||
            m_input[m_pos] != '\\' ||
            m_input[m_pos + 1] != 'u') {
            m_error = "Malformed JSON unicode surrogate pair.";
            return false;
        }
        m_pos += 2;

        std::uint32_t second = 0;
        if (!parseHexQuad(second)) {
            return false;
        }
        if (second < 0xDC00 || second > 0xDFFF) {
            m_error = "Malformed JSON unicode surrogate pair.";
            return false;
        }

        const std::uint32_t codePoint =
            0x10000u + ((first - 0xD800u) << 10) + (second - 0xDC00u);
        return appendUtf8(text, codePoint);
    }

    if (first >= 0xDC00 && first <= 0xDFFF) {
        m_error = "Isolated JSON unicode low surrogate.";
        return false;
    }

    return appendUtf8(text, first);
}

bool JsonParser::parseString(std::string& text) {
    skipWhitespace();
    if (m_pos >= m_input.size() || m_input[m_pos] != '"') {
        return false;
    }
    ++m_pos;
    text.clear();

    while (m_pos < m_input.size()) {
        const char ch = m_input[m_pos++];
        if (ch == '"') {
            return true;
        }
        if (ch != '\\') {
            const unsigned char byte = static_cast<unsigned char>(ch);
            if (byte < 0x20) {
                m_error = "Invalid unescaped control character in JSON string.";
                return false;
            }
            if (byte >= 0x80u) {
                if (!appendRawUtf8(text, byte)) {
                    return false;
                }
                continue;
            }
            text.push_back(ch);
            continue;
        }
        if (m_pos >= m_input.size()) {
            m_error = "Unterminated JSON escape.";
            return false;
        }
        const char escape = m_input[m_pos++];
        switch (escape) {
            case '"': text.push_back('"'); break;
            case '\\': text.push_back('\\'); break;
            case '/': text.push_back('/'); break;
            case 'b': text.push_back('\b'); break;
            case 'f': text.push_back('\f'); break;
            case 'n': text.push_back('\n'); break;
            case 'r': text.push_back('\r'); break;
            case 't': text.push_back('\t'); break;
            case 'u':
                if (!parseUnicodeEscape(text)) {
                    return false;
                }
                break;
            default:
                m_error = "Invalid JSON escape.";
                return false;
        }
    }

    m_error = "Unterminated JSON string.";
    return false;
}

bool JsonParser::parseNumber(double& number) {
    const std::size_t start = m_pos;
    if (m_pos < m_input.size() && m_input[m_pos] == '-') {
        ++m_pos;
    }

    if (m_pos >= m_input.size() ||
        std::isdigit(static_cast<unsigned char>(m_input[m_pos])) == 0) {
        m_error = "Invalid JSON number.";
        return false;
    }

    if (m_input[m_pos] == '0') {
        ++m_pos;
        if (m_pos < m_input.size() &&
            std::isdigit(static_cast<unsigned char>(m_input[m_pos])) != 0) {
            m_error = "Invalid JSON number: leading zero.";
            return false;
        }
    } else {
        while (m_pos < m_input.size() &&
               std::isdigit(static_cast<unsigned char>(m_input[m_pos])) != 0) {
            ++m_pos;
        }
    }

    if (m_pos < m_input.size() && m_input[m_pos] == '.') {
        ++m_pos;
        const std::size_t digitsStart = m_pos;
        while (m_pos < m_input.size() &&
               std::isdigit(static_cast<unsigned char>(m_input[m_pos])) != 0) {
            ++m_pos;
        }
        if (digitsStart == m_pos) {
            m_error = "Invalid JSON number: expected digit after decimal point.";
            return false;
        }
    }

    if (m_pos < m_input.size() &&
        (m_input[m_pos] == 'e' || m_input[m_pos] == 'E')) {
        ++m_pos;
        if (m_pos < m_input.size() &&
            (m_input[m_pos] == '+' || m_input[m_pos] == '-')) {
            ++m_pos;
        }
        const std::size_t digitsStart = m_pos;
        while (m_pos < m_input.size() &&
               std::isdigit(static_cast<unsigned char>(m_input[m_pos])) != 0) {
            ++m_pos;
        }
        if (digitsStart == m_pos) {
            m_error = "Invalid JSON number: expected exponent digit.";
            return false;
        }
    }

    const std::string token(m_input.substr(start, m_pos - start));
    char* end = nullptr;
    errno = 0;
    number = std::strtod(token.c_str(), &end);
    if (end != token.c_str() + token.size() || errno == ERANGE || !std::isfinite(number)) {
        m_error = "Invalid JSON number.";
        return false;
    }
    return true;
}

bool JsonParser::matchLiteral(std::string_view literal) {
    if (m_input.substr(m_pos, literal.size()) != literal) {
        return false;
    }
    m_pos += literal.size();
    return true;
}

} // namespace XpressFormula::Infrastructure::Serialization
