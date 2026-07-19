// SPDX-License-Identifier: MIT
// ExportMetadata.h - Small reusable helpers for export metadata sidecars.
#pragma once

#include "../Infrastructure/FileSystem/AtomicFileWriter.h"
#include "../Infrastructure/Serialization/JsonWriter.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace XpressFormula::UI {

inline constexpr int kExportMetadataSchemaVersion = 1;

inline std::string jsonEscape(std::string_view text) {
    return Infrastructure::Serialization::jsonEscape(text);
}

inline const char* jsonBool(bool value) {
    return Infrastructure::Serialization::jsonBool(value);
}

inline std::filesystem::path exportMetadataSidecarPath(const std::filesystem::path& imagePath) {
    std::filesystem::path sidecar = imagePath;
    sidecar += L".json";
    return sidecar;
}

inline std::filesystem::path exportMetadataTempPath(const std::filesystem::path& sidecarPath) {
    return Infrastructure::FileSystem::atomicTempPathFor(sidecarPath);
}

} // namespace XpressFormula::UI
