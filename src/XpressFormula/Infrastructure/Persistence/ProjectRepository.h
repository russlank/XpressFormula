// SPDX-License-Identifier: MIT
// ProjectRepository.h - File I/O boundary for .xfplot project sessions.
#pragma once

#include "ProjectSession.h"

#include <filesystem>
#include <string>
#include <vector>

namespace XpressFormula::Infrastructure::Persistence {

struct ProjectLoadResult {
    bool success = false;
    ProjectSession session;
    std::vector<std::string> warnings;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

struct ProjectSaveResult {
    bool success = false;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

class ProjectRepository {
public:
    [[nodiscard]] ProjectLoadResult load(const std::filesystem::path& path) const;
    [[nodiscard]] ProjectSaveResult save(const std::filesystem::path& path,
                                         const ProjectSession& session) const;
};

} // namespace XpressFormula::Infrastructure::Persistence
