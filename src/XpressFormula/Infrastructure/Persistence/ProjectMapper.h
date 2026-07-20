// SPDX-License-Identifier: MIT
// ProjectMapper.h - Mapping between live model state and project persistence DTOs.
#pragma once

#include "ProjectSession.h"
#include "../../Core/ViewTransform.h"
#include "../../Model/Formula.h"
#include "../../Model/PlotSettings.h"

#include <string>
#include <vector>

namespace XpressFormula::Infrastructure::Persistence {

struct ProjectDocumentSnapshot {
    std::vector<Model::Formula> formulas;
    Core::ViewTransform view;
    Model::PlotSettings plot;
};

struct ProjectMapResult {
    ProjectDocumentSnapshot document;
    std::vector<std::string> warnings;
};

[[nodiscard]] ProjectSession makeProjectSession(const std::vector<Model::Formula>& formulas,
                                                const Core::ViewTransform& view,
                                                const Model::PlotSettings& plot);
[[nodiscard]] ProjectMapResult mapProjectSessionToDocument(const ProjectSession& session);

void applyProjectSession(const ProjectSession& session,
                         std::vector<Model::Formula>& formulas,
                         Core::ViewTransform& view,
                         Model::PlotSettings& plot,
                         std::vector<std::string>& warnings);

[[nodiscard]] std::string serializeCurrentProjectSession(const std::vector<Model::Formula>& formulas,
                                                         const Core::ViewTransform& view,
                                                         const Model::PlotSettings& plot);

} // namespace XpressFormula::Infrastructure::Persistence
