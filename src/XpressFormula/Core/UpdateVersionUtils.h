// SPDX-License-Identifier: MIT
// UpdateVersionUtils.h - Small helpers for parsing release tags and comparing versions.
#pragma once

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

// Minimal JSON string-field extractor for simple GitHub API responses.
// It understands \" and \\ escapes, which is sufficient for tag_name/html_url parsing.
inline std::string extractJsonStringField(std::string_view json, std::string_view key) {
    const std::string quotedKey = "\"" + std::string(key) + "\"";
    std::size_t pos = json.find(quotedKey);
    if (pos == std::string_view::npos) {
        return {};
    }
    pos = json.find(':', pos + quotedKey.size());
    if (pos == std::string_view::npos) {
        return {};
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return {};
    }
    ++pos;

    std::string value;
    value.reserve(64);
    while (pos < json.size()) {
        const char ch = json[pos++];
        if (ch == '"') {
            return value;
        }
        if (ch == '\\') {
            if (pos >= json.size()) {
                return {};
            }
            const char esc = json[pos++];
            switch (esc) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    // Keep uncommon escapes as-is; GitHub tag/url fields should not need more.
                    value.push_back(esc);
                    break;
            }
            continue;
        }
        value.push_back(ch);
    }

    return {};
}

} // namespace XpressFormula::Core::UpdateVersionUtils
