// SPDX-License-Identifier: MIT
// JsonWriter.cpp - Small JSON writer implementation.
#include "JsonWriter.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>

namespace XpressFormula::Infrastructure::Serialization {

std::string jsonEscape(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    static constexpr char hex[] = "0123456789ABCDEF";

    for (unsigned char ch : text) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped += hex[(ch >> 4) & 0x0F];
                    escaped += hex[ch & 0x0F];
                } else {
                    escaped += static_cast<char>(ch);
                }
                break;
        }
    }

    return escaped;
}

std::string quoteJsonString(std::string_view text) {
    return std::string("\"") + jsonEscape(text) + "\"";
}

const char* jsonBool(bool value) {
    return value ? "true" : "false";
}

double finiteJsonNumber(double value, double fallback) {
    return std::isfinite(value) ? value : fallback;
}

float finiteJsonNumber(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

std::string formatJsonNumber(double value, double fallback) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << finiteJsonNumber(value, fallback);
    return out.str();
}

std::string formatJsonNumber(float value, float fallback) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10)
        << finiteJsonNumber(value, fallback);
    return out.str();
}

JsonWriter::JsonWriter() {
    m_out.imbue(std::locale::classic());
}

void JsonWriter::beginObject() {
    beforeValue();
    m_out << '{';
    m_stack.push_back({ ContainerKind::Object, 0, false });
}

void JsonWriter::endObject() {
    if (m_stack.empty() || m_stack.back().kind != ContainerKind::Object ||
        m_stack.back().expectingValue) {
        m_valid = false;
        return;
    }

    const Container closing = m_stack.back();
    m_stack.pop_back();
    if (closing.count > 0) {
        newlineAndIndent(m_stack.size());
    }
    m_out << '}';
}

void JsonWriter::beginArray() {
    beforeValue();
    m_out << '[';
    m_stack.push_back({ ContainerKind::Array, 0, false });
}

void JsonWriter::endArray() {
    if (m_stack.empty() || m_stack.back().kind != ContainerKind::Array ||
        m_stack.back().expectingValue) {
        m_valid = false;
        return;
    }

    const Container closing = m_stack.back();
    m_stack.pop_back();
    if (closing.count > 0) {
        newlineAndIndent(m_stack.size());
    }
    m_out << ']';
}

void JsonWriter::key(std::string_view name) {
    if (m_stack.empty() || m_stack.back().kind != ContainerKind::Object ||
        m_stack.back().expectingValue) {
        m_valid = false;
        return;
    }

    Container& object = m_stack.back();
    if (object.count++ > 0) {
        m_out << ',';
    }
    newlineAndIndent(m_stack.size());
    writeQuoted(name);
    m_out << ": ";
    object.expectingValue = true;
}

void JsonWriter::value(std::nullptr_t) {
    beforeValue();
    m_out << "null";
}

void JsonWriter::value(bool input) {
    beforeValue();
    m_out << jsonBool(input);
}

void JsonWriter::value(int input) {
    beforeValue();
    m_out << input;
}

void JsonWriter::value(long long input) {
    beforeValue();
    m_out << input;
}

void JsonWriter::value(unsigned long long input) {
    beforeValue();
    m_out << input;
}

void JsonWriter::value(double input) {
    beforeValue();
    m_out << formatJsonNumber(input);
}

void JsonWriter::value(float input) {
    beforeValue();
    m_out << formatJsonNumber(input);
}

void JsonWriter::value(std::string_view input) {
    beforeValue();
    writeQuoted(input);
}

void JsonWriter::value(const std::string& input) {
    value(std::string_view(input));
}

void JsonWriter::value(const char* input) {
    value(input ? std::string_view(input) : std::string_view());
}

std::string JsonWriter::str() const {
    return m_out.str();
}

bool JsonWriter::valid() const {
    return m_valid && m_stack.empty();
}

void JsonWriter::beforeValue() {
    if (m_stack.empty()) {
        return;
    }

    Container& container = m_stack.back();
    if (container.kind == ContainerKind::Object) {
        if (!container.expectingValue) {
            m_valid = false;
            return;
        }
        container.expectingValue = false;
        return;
    }

    if (container.count++ > 0) {
        m_out << ',';
    }
    newlineAndIndent(m_stack.size());
}

void JsonWriter::newlineAndIndent(std::size_t depth) {
    m_out << '\n';
    for (std::size_t i = 0; i < depth; ++i) {
        m_out << "  ";
    }
}

void JsonWriter::writeQuoted(std::string_view text) {
    m_out << '"' << jsonEscape(text) << '"';
}

} // namespace XpressFormula::Infrastructure::Serialization
