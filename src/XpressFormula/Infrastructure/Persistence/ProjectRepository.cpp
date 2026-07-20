// SPDX-License-Identifier: MIT
// ProjectRepository.cpp - File I/O boundary for .xfplot project sessions.
#include "ProjectRepository.h"
#include "ProjectSerializer.h"
#include "../FileSystem/AtomicFileWriter.h"

#include <fstream>
#include <sstream>

namespace XpressFormula::Infrastructure::Persistence {

ProjectLoadResult ProjectRepository::load(const std::filesystem::path& path) const {
    ProjectLoadResult result;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "Could not open project file.";
        return result;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        result.error = "Could not read project file.";
        return result;
    }

    ProjectSessionParseResult parsed = parseProjectSession(buffer.str());
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

ProjectSaveResult ProjectRepository::save(const std::filesystem::path& path,
                                          const ProjectSession& session) const {
    ProjectSaveResult result;
    const std::string json = serializeProjectSession(session);
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
