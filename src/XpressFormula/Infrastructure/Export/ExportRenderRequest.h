// SPDX-License-Identifier: MIT
// ExportRenderRequest.h - Immutable export render request construction.
#pragma once

#include "ExportSettings.h"
#include "../../Core/ViewTransform.h"
#include "../../Model/PlotPolicy.h"
#include "../../Model/SceneSummary.h"

#include <array>

namespace XpressFormula::Infrastructure::Export {

struct ExportRenderRequest {
    ExportSettings settings;
    int outputWidth = 0;
    int outputHeight = 0;
    int targetWidth = 0;
    int targetHeight = 0;
    int samplingFactor = 1;
    ExportWorldBounds sourceBounds;
    ExportResolvedView resolvedView;
    Core::ViewTransform view;
    Model::PlotSettings plotSettings;
    Model::SceneSummary scene;
    Model::PlotRenderOverrides overrides;
    Model::PlotQualityDecision quality;
    std::array<float, 4> backgroundColor{};
};

[[nodiscard]] ExportSettings settingsForPreviewRender(const ExportSettings& settings,
                                                      ExportPreviewQuality requestedQuality,
                                                      ExportPreviewSize& previewSize);

[[nodiscard]] ExportRenderRequest buildExportRenderRequest(
    const ExportSettings& settings,
    const Core::ViewTransform& sourceView,
    const Model::PlotSettings& sourcePlotSettings,
    const Model::SceneSummary& scene);

} // namespace XpressFormula::Infrastructure::Export
