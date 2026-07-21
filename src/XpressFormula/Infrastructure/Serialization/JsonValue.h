// SPDX-License-Identifier: MIT
// JsonValue.h - Small JSON value tree used by internal persistence code.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Infrastructure::Serialization {

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    [[nodiscard]] const JsonValue* find(std::string_view key) const {
        if (type != Type::Object) {
            return nullptr;
        }
        const auto it = object.find(std::string(key));
        return it == object.end() ? nullptr : &it->second;
    }
};

} // namespace XpressFormula::Infrastructure::Serialization
