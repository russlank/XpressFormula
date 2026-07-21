// SPDX-License-Identifier: MIT
// UpdateVersionUtils.h - Small helpers for parsing release tags and comparing versions.
#pragma once

#include "../Infrastructure/Serialization/JsonParser.h"

#include <cctype>
#include <string>
#include <string_view>

namespace XpressFormula::Core::UpdateVersionUtils {

struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

inline std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

inline bool parseNumberToken(std::string_view text, std::size_t& pos, int& outValue) {
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) {
        return false;
    }

    int value = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        const int digit = static_cast<int>(text[pos] - '0');
        if (value > 100000000) {
            return false; // simple overflow guard for malformed tags
        }
        value = value * 10 + digit;
        ++pos;
    }

    outValue = value;
    return true;
}

// Accepts tags like "1.2.3", "v1.2.3", "V1.2.3", and suffixes such as "-rc1" or "+build".
inline bool tryParseSemanticVersion(std::string_view text, SemanticVersion& outVersion) {
    text = trim(text);
    if (text.empty()) {
        return false;
    }

    if (text.front() == 'v' || text.front() == 'V') {
        text.remove_prefix(1);
    }

    std::size_t pos = 0;
    SemanticVersion parsed{};
    if (!parseNumberToken(text, pos, parsed.major)) {
        return false;
    }
    if (pos >= text.size() || text[pos] != '.') {
        return false;
    }
    ++pos;
    if (!parseNumberToken(text, pos, parsed.minor)) {
        return false;
    }
    if (pos >= text.size() || text[pos] != '.') {
        return false;
    }
    ++pos;
    if (!parseNumberToken(text, pos, parsed.patch)) {
        return false;
    }

    // Remaining suffix is optional (e.g. "-beta", "+build.1"), but a fourth numeric segment
    // would be ambiguous for this app's semantic versioning, so reject "1.2.3.4".
    if (pos < text.size() && text[pos] == '.') {
        return false;
    }

    outVersion = parsed;
    return true;
}

inline int compareSemanticVersion(const SemanticVersion& a, const SemanticVersion& b) {
    if (a.major != b.major) return (a.major < b.major) ? -1 : 1;
    if (a.minor != b.minor) return (a.minor < b.minor) ? -1 : 1;
    if (a.patch != b.patch) return (a.patch < b.patch) ? -1 : 1;
    return 0;
}

inline bool isRemoteVersionNewer(std::string_view currentVersion, std::string_view remoteTag) {
    SemanticVersion current{};
    SemanticVersion remote{};
    if (!tryParseSemanticVersion(currentVersion, current) ||
        !tryParseSemanticVersion(remoteTag, remote)) {
        return false;
    }
    return compareSemanticVersion(current, remote) < 0;
}

inline std::string extractJsonStringField(std::string_view json, std::string_view key) {
    using XpressFormula::Infrastructure::Serialization::JsonParser;
    using XpressFormula::Infrastructure::Serialization::JsonValue;

    JsonValue root;
    JsonParser parser(json);
    std::string error;
    if (!parser.parse(root, error) || root.type != JsonValue::Type::Object) {
        return {};
    }

    const JsonValue* field = root.find(key);
    if (!field || field->type != JsonValue::Type::String) {
        return {};
    }
    return field->text;
}

} // namespace XpressFormula::Core::UpdateVersionUtils
