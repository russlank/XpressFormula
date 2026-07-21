// SPDX-License-Identifier: MIT
// ProjectSerializer.h - DTO/JSON conversion for .xfplot schema v1.
#pragma once

#include "ProjectSession.h"

#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Infrastructure::Persistence {

struct ProjectSessionParseResult {
    bool success = false;
    ProjectSession session;
    std::string error;
    std::vector<std::string> warnings;
};

[[nodiscard]] std::string serializeProjectSession(const ProjectSession& session);
[[nodiscard]] ProjectSessionParseResult parseProjectSession(std::string_view json);

} // namespace XpressFormula::Infrastructure::Persistence
