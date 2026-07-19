// SPDX-License-Identifier: MIT
// JsonParser.h - Strict internal JSON parser for app-owned persistence files.
#pragma once

#include "JsonValue.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace XpressFormula::Infrastructure::Serialization {

class JsonParser {
public:
    explicit JsonParser(std::string_view input);

    bool parse(JsonValue& value, std::string& error);

private:
    void skipWhitespace();
    bool consume(char expected);
    bool parseValue(JsonValue& value);
    bool parseObject(JsonValue& value);
    bool parseArray(JsonValue& value);
    static int hexValue(char ch);
    bool parseHexQuad(std::uint32_t& codeUnit);
    bool appendUtf8(std::string& text, std::uint32_t codePoint);
    bool parseUnicodeEscape(std::string& text);
    bool parseString(std::string& text);
    bool parseNumber(double& number);
    bool matchLiteral(std::string_view literal);

    std::string_view m_input;
    std::size_t m_pos = 0;
    std::string m_error;
};

} // namespace XpressFormula::Infrastructure::Serialization
