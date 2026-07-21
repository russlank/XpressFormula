// SPDX-License-Identifier: MIT
// ProjectRepository.cpp - File I/O boundary for .xfplot project sessions.
#include "ProjectRepository.h"
#include "ProjectSerializer.h"
#include "../../Core/InputLimits.h"
#include "../FileSystem/AtomicFileWriter.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace XpressFormula::Infrastructure::Persistence {
namespace {

constexpr std::size_t kProjectReadChunkBytes = 64ull * 1024ull;

[[nodiscard]] std::size_t projectFileLimitBytes() noexcept {
    static_assert(Core::InputLimits::kMaxProjectFileBytes <=
                  static_cast<std::uintmax_t>((std::numeric_limits<std::size_t>::max)()));
    return static_cast<std::size_t>(Core::InputLimits::kMaxProjectFileBytes);
}

[[nodiscard]] ProjectLoadResult readProjectFileBounded(std::ifstream& in) {
    ProjectLoadResult result;
    const std::size_t maxBytes = projectFileLimitBytes();
    std::string content;
    content.reserve((std::min)(maxBytes, kProjectReadChunkBytes));

    std::array<char, kProjectReadChunkBytes> chunk{};
    while (content.size() <= maxBytes) {
        const std::size_t remainingWithSentinel = maxBytes + 1 - content.size();
        const std::size_t requested =
            (std::min)(chunk.size(), remainingWithSentinel);
        in.read(chunk.data(), static_cast<std::streamsize>(requested));

        const std::streamsize readCount = in.gcount();
        if (readCount > 0) {
            const std::size_t appendCount = static_cast<std::size_t>(readCount);
            if (appendCount > maxBytes - (std::min)(content.size(), maxBytes)) {
                result.error = "Project file is larger than the supported limit.";
                return result;
            }
            content.append(chunk.data(), appendCount);
            if (content.size() > maxBytes) {
                result.error = "Project file is larger than the supported limit.";
                return result;
            }
        }

        if (readCount < static_cast<std::streamsize>(requested)) {
            if (in.bad() || (in.fail() && !in.eof())) {
                result.error = "Could not read project file.";
                return result;
            }
            break;
        }
    }

    ProjectSessionParseResult parsed = parseProjectSession(content);
    if (!parsed.success) {
        result.error = parsed.error;
        result.warnings = std::move(parsed.warnings);
        return result;
    }

    result.success = true;
    result.session = std::move(parsed.session);
    result.warnings = std::move(parsed.warnings);
    return result;
}

[[nodiscard]] ProjectSaveResult validateProjectSessionForSave(const ProjectSession& session) {
    ProjectSaveResult result;
    if (session.formulas.size() > Core::InputLimits::kMaxProjectFormulas) {
        result.error = "Project contains more formulas than the supported limit.";
        return result;
    }

    std::size_t expressionBytes = 0;
    for (std::size_t i = 0; i < session.formulas.size(); ++i) {
        const std::size_t length = session.formulas[i].expression.size();
        if (length > Core::InputLimits::kMaxFormulaLength) {
            result.error = "Formula " + std::to_string(i + 1) +
                " exceeds the supported expression length.";
            return result;
        }
        if (length > projectFileLimitBytes() - (std::min)(expressionBytes, projectFileLimitBytes())) {
            result.error = "Serialized project exceeds the supported file-size limit.";
            return result;
        }
        expressionBytes += length;
    }

    result.success = true;
    return result;
}

} // namespace

ProjectLoadResult ProjectRepository::load(const std::filesystem::path& path) const {
    ProjectLoadResult result;
    std::error_code fileSizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, fileSizeError);
    if (!fileSizeError && fileSize > Core::InputLimits::kMaxProjectFileBytes) {
        result.error = "Project file is larger than the supported limit.";
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "Could not open project file.";
        return result;
    }

    return readProjectFileBounded(in);
}

ProjectSaveResult ProjectRepository::save(const std::filesystem::path& path,
                                          const ProjectSession& session) const {
    ProjectSaveResult result;
    const ProjectSaveResult validation = validateProjectSessionForSave(session);
    if (!validation) {
        return validation;
    }

    const std::string json = serializeProjectSession(session);
    if (json.size() > Core::InputLimits::kMaxProjectFileBytes) {
        result.error = "Serialized project exceeds the supported file-size limit.";
        return result;
    }

    const FileSystem::AtomicWriteResult writeResult =
        FileSystem::writeTextAtomically(path, json);
    if (!writeResult) {
        result.error = writeResult.error;
        return result;
    }

    result.success = true;
    return result;
}

} // namespace XpressFormula::Infrastructure::Persistence
