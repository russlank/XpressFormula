// SPDX-License-Identifier: MIT
// ExportRenderRequest.cpp - Immutable export render request construction.
#include "ExportRenderRequest.h"

namespace XpressFormula::Infrastructure::Export {

ExportSettings settingsForPreviewRender(const ExportSettings& settings,
                                        ExportPreviewQuality requestedQuality,
                                        ExportPreviewSize& previewSize) {
    previewSize = resolveExportPreviewSize(settings.size.width,
                                           settings.size.height,
                                           requestedQuality);

    ExportSettings previewSettings = settings;
    previewSettings.size.width = previewSize.width;
    previewSettings.size.height = previewSize.height;

    if (requestedQuality != ExportPreviewQuality::Final) {
        previewSettings.quality.mode = ExportQualityMode::Override;
        previewSettings.quality = qualitySettingsForPreset(
            requestedQuality == ExportPreviewQuality::Draft
                ? ExportQualityPreset::Draft
                : ExportQualityPreset::Normal);
        previewSettings.quality.mode = ExportQualityMode::Override;
        previewSettings.quality.supersampling = ExportSupersampling::Off;
    }

    return previewSettings;
}

ExportRenderRequest buildExportRenderRequest(const ExportSettings& settings,
                                             const Core::ViewTransform& sourceView,
                                             const Model::PlotSettings& sourcePlotSettings,
                                             const Model::SceneSummary& scene) {
    ExportRenderRequest request;
    request.settings = settings;
    request.outputWidth = clampExportDimension(settings.size.width);
    request.outputHeight = clampExportDimension(settings.size.height);
    request.samplingFactor = effectiveSupersamplingFactor(
        request.outputWidth,
        request.outputHeight,
        effectiveExportSupersampling(settings));
    request.targetWidth = clampExportDimension(request.outputWidth * request.samplingFactor);
    request.targetHeight = clampExportDimension(request.outputHeight * request.samplingFactor);
    request.sourceBounds = {
        sourceView.worldXMin(), sourceView.worldXMax(),
        sourceView.worldYMin(), sourceView.worldYMax()
    };
    request.resolvedView = resolveExportView(request.targetWidth,
                                            request.targetHeight,
                                            request.sourceBounds,
                                            settings.output.aspectMode);
    request.view = sourceView;
    request.view.state.centerX =
        (request.resolvedView.visibleBounds.xMin + request.resolvedView.visibleBounds.xMax) * 0.5;
    request.view.state.centerY =
        (request.resolvedView.visibleBounds.yMin + request.resolvedView.visibleBounds.yMax) * 0.5;
    request.view.state.scaleX = request.resolvedView.scaleX;
    request.view.state.scaleY = request.resolvedView.scaleY;

    request.plotSettings = sourcePlotSettings;
    request.plotSettings.autoRotate = false;
    request.scene = scene;
    request.overrides = plotRenderOverridesForExport(settings);
    request.quality = plotQualityDecisionForExport(settings);
    request.backgroundColor = resolveExportBackgroundColor(settings);
    return request;
}

} // namespace XpressFormula::Infrastructure::Export
