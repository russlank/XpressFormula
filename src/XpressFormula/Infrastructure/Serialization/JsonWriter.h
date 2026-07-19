// SPDX-License-Identifier: MIT
// JsonWriter.h - Small JSON writer for app-owned persistence and metadata.
#pragma once

#include <cstddef>
#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Infrastructure::Serialization {

[[nodiscard]] std::string jsonEscape(std::string_view text);
[[nodiscard]] std::string quoteJsonString(std::string_view text);
[[nodiscard]] const char* jsonBool(bool value);
[[nodiscard]] double finiteJsonNumber(double value, double fallback = 0.0);
[[nodiscard]] float finiteJsonNumber(float value, float fallback = 0.0f);
[[nodiscard]] std::string formatJsonNumber(double value, double fallback = 0.0);
[[nodiscard]] std::string formatJsonNumber(float value, float fallback = 0.0f);

class JsonWriter {
public:
    JsonWriter();

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();

    void key(std::string_view name);

    void value(std::nullptr_t);
    void value(bool input);
    void value(int input);
    void value(long long input);
    void value(unsigned long long input);
    void value(double input);
    void value(float input);
    void value(std::string_view input);
    void value(const std::string& input);
    void value(const char* input);

    [[nodiscard]] std::string str() const;
    [[nodiscard]] bool valid() const;

private:
    enum class ContainerKind {
        Object,
        Array
    };

    struct Container {
        ContainerKind kind;
        std::size_t count = 0;
        bool expectingValue = false;
    };

    void beforeValue();
    void newlineAndIndent(std::size_t depth);
    void writeQuoted(std::string_view text);

    std::ostringstream m_out;
    std::vector<Container> m_stack;
    bool m_valid = true;
};

} // namespace XpressFormula::Infrastructure::Serialization
