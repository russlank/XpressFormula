// SPDX-License-Identifier: MIT
// ExportMetadata.h - Small reusable helpers for export metadata sidecars.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace XpressFormula::UI {

inline constexpr int kExportMetadataSchemaVersion = 1;

inline std::string jsonEscape(std::string_view text) {
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

inline const char* jsonBool(bool value) {
    return value ? "true" : "false";
}

inline std::filesystem::path exportMetadataSidecarPath(const std::filesystem::path& imagePath) {
    std::filesystem::path sidecar = imagePath;
    sidecar += L".json";
    return sidecar;
}

inline std::filesystem::path exportMetadataTempPath(const std::filesystem::path& sidecarPath) {
    std::filesystem::path temporary = sidecarPath;
    temporary += L".tmp";
    return temporary;
}

} // namespace XpressFormula::UI
