// SPDX-License-Identifier: MIT
// ExportDialog.cpp - ImGui export settings dialog component.
#include "ExportDialog.h"
#include "../UiKit/Splitter.h"
#include "../UiKit/UiMetrics.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::UI::Components {
namespace {

void drawCheckerboard(ImVec2 min, ImVec2 max) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float checkerSize = 10.0f;
    const ImU32 checkerA = IM_COL32(82, 82, 90, 255);
    const ImU32 checkerB = IM_COL32(126, 126, 136, 255);
    drawList->AddRectFilled(min, max, checkerA);
    drawList->PushClipRect(min, max, true);
    for (float y = min.y; y < max.y; y += checkerSize) {
        for (float x = min.x; x < max.x; x += checkerSize) {
            const int ix = static_cast<int>((x - min.x) / checkerSize);
            const int iy = static_cast<int>((y - min.y) / checkerSize);
            if (((ix + iy) & 1) == 0) {
                continue;
            }
            drawList->AddRectFilled(
                ImVec2(x, y),
                ImVec2((std::min)(x + checkerSize, max.x),
                       (std::min)(y + checkerSize, max.y)),
                checkerB);
        }
    }
    drawList->PopClipRect();
}

} // namespace

ExportDialogAction renderExportDialogContent(ExportDialogContext& context) {
    ExportDialogAction action;
    bool previewChanged = false;

    auto& settings = context.settings;
    normalizeExportSettings(settings);
    previewChanged = resolveCoordinateOverlayPolicy(settings.scene.showCoordinates,
                                                    settings.scene.showAxisTriad) ||
                     previewChanged;

    auto applySelectedSizePreset = [&]() {
        applyExportSizePreset(settings, context.sourceWidth, context.sourceHeight);
    };

    auto markCustomProfile = [&]() {
        if (syncExportProfileAfterManualChange(settings, &context.currentScene)) {
            action.redrawRequested = true;
        }
    };

    auto markCustomSize = [&]() {
        markExportSizeCustom(settings);
        markCustomProfile();
    };

    auto applyExportProfile = [&](ExportProfile profile) {
        if (profile == ExportProfile::Custom) {
            settings.profile = ExportProfile::Custom;
            context.status = "Custom export profile selected.";
            return;
        }

        settings = exportSettingsForProfile(profile, &context.currentScene);
        applySelectedSizePreset();
        context.status = std::string("Applied export profile: ") + exportProfileLabel(profile) + ".";
    };

    auto resetExportSettings = [&]() {
        settings = defaultExportSettings();
        applyExportProfile(ExportProfile::CurrentView);
        context.previewZoom = 0.0f;
        context.previewPanX = 0.0f;
        context.previewPanY = 0.0f;
        context.status = "Export settings reset.";
        action.settingsChanged = true;
    };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.45f;
    ImGui::BeginChild("##ExportDialogBody", ImVec2(0.0f, -footerHeight), false);
    const float bodyWidth = ImGui::GetContentRegionAvail().x;
    const float bodyHeight = ImGui::GetContentRegionAvail().y;
    const float splitterWidth = UiKit::metrics().splitterWidth;
    const float minSettingsWidth = 300.0f;
    const float minPreviewWidth = 240.0f;
    const float maxSettingsWidth = (std::max)(minSettingsWidth, bodyWidth - splitterWidth - minPreviewWidth);
    context.settingsPaneWidth = std::clamp(context.settingsPaneWidth, minSettingsWidth, maxSettingsWidth);
    const float settingsPaneWidth = context.settingsPaneWidth;

    ImGui::BeginChild("##ExportSettingsPane", ImVec2(settingsPaneWidth, 0.0f), true);
    if (ImGui::BeginCombo("Profile", exportProfileLabel(settings.profile))) {
        for (ExportProfile profile : exportProfiles()) {
            const bool selected = (settings.profile == profile);
            if (ImGui::Selectable(exportProfileLabel(profile), selected)) {
                applyExportProfile(profile);
                previewChanged = (profile != ExportProfile::Custom) || previewChanged;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Profiles update size, appearance, scene, quality, output, and metadata settings.");
    ImGui::Separator();

    if (ImGui::BeginTabBar("##ExportSettingsTabs")) {
        if (ImGui::BeginTabItem("Size")) {
            if (context.sourceWidth > 0 && context.sourceHeight > 0) {
                ImGui::Text("Current plot: %d x %d", context.sourceWidth, context.sourceHeight);
            } else {
                ImGui::TextDisabled("Current plot size unavailable.");
            }

            if (ImGui::Button("Use Current", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                settings.size.selectedPreset = kExportSizePresetCurrent;
                applySelectedSizePreset();
                markCustomProfile();
                previewChanged = true;
            }

            ImGui::Spacing();
            const auto& presets = exportSizePresets();
            const char* presetLabel = presets[static_cast<size_t>(settings.size.selectedPreset)].label;
            if (ImGui::BeginCombo("Preset", presetLabel)) {
                for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
                    const bool selected = (settings.size.selectedPreset == i);
                    if (ImGui::Selectable(presets[static_cast<size_t>(i)].label, selected)) {
                        settings.size.selectedPreset = i;
                        applySelectedSizePreset();
                        markCustomProfile();
                        previewChanged = !presets[static_cast<size_t>(i)].custom || previewChanged;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            std::string scaleLabel = std::to_string(settings.size.scale) + "x";
            if (ImGui::BeginCombo("Scale", scaleLabel.c_str())) {
                for (const int scaleOption : exportScaleOptions()) {
                    std::string optionLabel = std::to_string(scaleOption) + "x";
                    const bool selected = (settings.size.scale == scaleOption);
                    if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                        const int previousScale = (std::max)(1, settings.size.scale);
                        settings.size.scale = scaleOption;
                        if (exportSizePresets()[static_cast<size_t>(settings.size.selectedPreset)].custom) {
                            settings.size.width = clampExportDimension(static_cast<int>(
                                std::lround(static_cast<double>(settings.size.width) * scaleOption / previousScale)));
                            settings.size.height = clampExportDimension(static_cast<int>(
                                std::lround(static_cast<double>(settings.size.height) * scaleOption / previousScale)));
                        } else {
                            applySelectedSizePreset();
                        }
                        markCustomProfile();
                        previewChanged = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            int width = settings.size.width;
            int height = settings.size.height;
            const int previousWidth = (std::max)(1, width);
            const int previousHeight = (std::max)(1, height);
            const bool widthChanged = ImGui::InputInt("Width", &width, 16, 128);
            const bool heightChanged = ImGui::InputInt("Height", &height, 16, 128);
            validateExportSize(width, height);

            if (settings.size.lockAspectRatio && widthChanged && !heightChanged) {
                height = aspectLockedHeight(width, previousWidth, previousHeight);
            } else if (settings.size.lockAspectRatio && heightChanged && !widthChanged) {
                width = aspectLockedWidth(height, previousWidth, previousHeight);
            }

            if (widthChanged || heightChanged) {
                settings.size.width = width;
                settings.size.height = height;
                markCustomSize();
                previewChanged = true;
            }
            if (ImGui::Checkbox("Lock Aspect Ratio", &settings.size.lockAspectRatio)) {
                markCustomProfile();
                previewChanged = true;
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Aspect Handling");
            const ExportAspectMode aspectModes[] = {
                ExportAspectMode::PreserveMathematicalScale,
                ExportAspectMode::PreserveVisibleBounds,
                ExportAspectMode::CropToFill,
                ExportAspectMode::StretchToOutput
            };
            for (ExportAspectMode mode : aspectModes) {
                if (ImGui::RadioButton(exportAspectModeLabel(mode), settings.output.aspectMode == mode)) {
                    settings.output.aspectMode = mode;
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::SetItemTooltip("%s", exportAspectModeTooltip(mode));
            }

            const auto resolvedView =
                resolveExportView(settings.size.width,
                                  settings.size.height,
                                  context.sourceBounds,
                                  settings.output.aspectMode);
            ImGui::Spacing();
            ImGui::Text("X: [%.4g, %.4g]",
                        resolvedView.visibleBounds.xMin, resolvedView.visibleBounds.xMax);
            ImGui::Text("Y: [%.4g, %.4g]",
                        resolvedView.visibleBounds.yMin, resolvedView.visibleBounds.yMax);
            if (resolvedView.uniformScale) {
                ImGui::Text("Scale: %.4g px/unit", resolvedView.scaleX);
            } else {
                ImGui::Text("Scale: %.4g x %.4g px/unit",
                            resolvedView.scaleX, resolvedView.scaleY);
            }
            if (resolvedView.marginLeftPx > 0.5 || resolvedView.marginTopPx > 0.5) {
                ImGui::Text("Margins: %.0f x %.0f px",
                            resolvedView.marginLeftPx + resolvedView.marginRightPx,
                            resolvedView.marginTopPx + resolvedView.marginBottomPx);
            }

            const ExportSupersampling sampling = effectiveExportSupersampling(settings);
            const auto estimatedBytes = estimateRgbaBufferBytes(
                settings.size.width, settings.size.height, sampling);
            const double estimatedMiB = bytesToMiB(estimatedBytes);
            ImGui::Spacing();
            ImGui::Text("Output: %d x %d", settings.size.width, settings.size.height);
            ImGui::Text("Render buffer: %.1f MiB", estimatedMiB);
            if (estimatedMiB >= kLargeExportWarningMiB) {
                ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                   "Large export: expect slower rendering and higher memory use.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Appearance")) {
            int backgroundMode = static_cast<int>(settings.appearance.backgroundMode);
            const ExportBackgroundMode backgroundModes[] = {
                ExportBackgroundMode::Current,
                ExportBackgroundMode::Transparent,
                ExportBackgroundMode::White,
                ExportBackgroundMode::Black,
                ExportBackgroundMode::Custom
            };
            for (ExportBackgroundMode mode : backgroundModes) {
                const int modeValue = static_cast<int>(mode);
                if (ImGui::RadioButton(exportBackgroundModeLabel(mode), backgroundMode == modeValue)) {
                    backgroundMode = modeValue;
                    settings.appearance.backgroundMode = mode;
                    markCustomProfile();
                    previewChanged = true;
                }
            }

            if (settings.appearance.backgroundMode == ExportBackgroundMode::Custom) {
                if (ImGui::ColorEdit3("Custom Color", settings.appearance.backgroundColor.data(),
                                      ImGuiColorEditFlags_DisplayRGB)) {
                    markCustomProfile();
                    previewChanged = true;
                }
                if (ImGui::SliderFloat("Opacity", &settings.appearance.backgroundColor[3],
                                       0.0f, 1.0f, "%.2f")) {
                    markCustomProfile();
                    previewChanged = true;
                }
            } else if (settings.appearance.backgroundMode == ExportBackgroundMode::Transparent) {
                ImGui::TextDisabled("Transparency is best preserved by PNG output.");
                if (settings.output.format == ExportFormat::Bmp) {
                    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                       "BMP export does not preserve alpha transparency.");
                }
            }

            ImGui::Spacing();
            if (ImGui::RadioButton("Color", !settings.appearance.grayscaleOutput)) {
                settings.appearance.grayscaleOutput = false;
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Grayscale", settings.appearance.grayscaleOutput)) {
                settings.appearance.grayscaleOutput = true;
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scene")) {
            if (ImGui::Checkbox("Grid", &settings.scene.showGrid)) {
                markCustomProfile();
                previewChanged = true;
            }
            if (ImGui::Checkbox("Coordinates", &settings.scene.showCoordinates)) {
                markCustomProfile();
                resolveCoordinateOverlayPolicy(settings.scene.showCoordinates,
                                               settings.scene.showAxisTriad);
                previewChanged = true;
            }
            if (ImGui::Checkbox("Wires", &settings.scene.showWires)) {
                markCustomProfile();
                previewChanged = true;
            }
            if (ImGui::Checkbox("Envelope", &settings.scene.showEnvelope)) {
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::BeginDisabled(settings.scene.showCoordinates);
            if (ImGui::Checkbox("Axis Triad", &settings.scene.showAxisTriad)) {
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::EndDisabled();
            if (settings.scene.showCoordinates) {
                ImGui::SetItemTooltip("Axis triad is disabled while coordinate axes and labels are enabled.");
            }

            ImGui::Spacing();
            int framingSelection = 0;
            const char* framingOptions[] = { "Current viewport", "Fit visible formulas (planned)" };
            ImGui::BeginDisabled();
            ImGui::Combo("Framing", &framingSelection, framingOptions,
                         IM_ARRAYSIZE(framingOptions));
            ImGui::EndDisabled();
            ImGui::TextDisabled("Fit visible formulas is planned.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Quality")) {
            bool overrideQuality = (settings.quality.mode == ExportQualityMode::Override);
            if (ImGui::Checkbox("Override Interactive Quality", &overrideQuality)) {
                settings.quality.mode = overrideQuality ? ExportQualityMode::Override
                                                        : ExportQualityMode::Interactive;
                markCustomProfile();
                previewChanged = true;
            }
            if (!overrideQuality) {
                ImGui::TextDisabled("Uses the current plot quality without changing it.");
            }

            ImGui::BeginDisabled(!overrideQuality);
            const ExportQualityPreset qualityPresets[] = {
                ExportQualityPreset::Draft,
                ExportQualityPreset::Normal,
                ExportQualityPreset::High,
                ExportQualityPreset::Ultra,
                ExportQualityPreset::Custom
            };
            if (ImGui::BeginCombo("Preset", exportQualityPresetLabel(settings.quality.preset))) {
                for (ExportQualityPreset preset : qualityPresets) {
                    const bool selected = (settings.quality.preset == preset);
                    if (ImGui::Selectable(exportQualityPresetLabel(preset), selected)) {
                        const ExportQualityMode currentMode = settings.quality.mode;
                        settings.quality = qualitySettingsForPreset(preset);
                        settings.quality.mode = currentMode;
                        markCustomProfile();
                        previewChanged = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const PlotLimits& limits = plotLimits();
            int surfaceResolution = settings.quality.surfaceResolution;
            if (ImGui::InputInt("Surface Density", &surfaceResolution, 4, 16)) {
                settings.quality.surfaceResolution = clampSurfaceResolution(surfaceResolution);
                settings.quality.preset = ExportQualityPreset::Custom;
                markCustomProfile();
                previewChanged = true;
            }

            int implicitResolution = settings.quality.implicitSurfaceResolution;
            if (ImGui::InputInt("Implicit Resolution", &implicitResolution, 4, 16)) {
                settings.quality.implicitSurfaceResolution =
                    clampImplicitSurfaceResolution(implicitResolution);
                settings.quality.preset = ExportQualityPreset::Custom;
                markCustomProfile();
                previewChanged = true;
            }

            ImGui::BeginDisabled(!settings.scene.showWires);
            if (ImGui::SliderFloat("Wire Thickness Scale", &settings.quality.wireThicknessScale,
                                   limits.wireThicknessScale.min,
                                   limits.wireThicknessScale.max,
                                   "%.2fx")) {
                settings.quality.preset = ExportQualityPreset::Custom;
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::EndDisabled();
            if (!settings.scene.showWires) {
                ImGui::SetItemTooltip("Wire thickness scale has no effect while wires are excluded.");
            }

            const ExportSupersampling supersamplingOptions[] = {
                ExportSupersampling::Off,
                ExportSupersampling::X2,
                ExportSupersampling::X4
            };
            if (ImGui::BeginCombo("Supersampling",
                                  exportSupersamplingLabel(settings.quality.supersampling))) {
                for (ExportSupersampling option : supersamplingOptions) {
                    const bool selected = (settings.quality.supersampling == option);
                    if (ImGui::Selectable(exportSupersamplingLabel(option), selected)) {
                        settings.quality.supersampling = option;
                        settings.quality.preset = ExportQualityPreset::Custom;
                        markCustomProfile();
                        previewChanged = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Output")) {
            if (ImGui::RadioButton("PNG", settings.output.format == ExportFormat::Png)) {
                settings.output.format = ExportFormat::Png;
                markCustomProfile();
                previewChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("BMP", settings.output.format == ExportFormat::Bmp)) {
                settings.output.format = ExportFormat::Bmp;
                markCustomProfile();
                previewChanged = true;
            }
            if (settings.output.format == ExportFormat::Bmp &&
                settings.appearance.backgroundMode == ExportBackgroundMode::Transparent) {
                ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                   "BMP flattens transparency in many viewers. Use PNG for alpha.");
            }

            ImGui::Spacing();
            if (ImGui::Checkbox("Open Image After Save", &settings.output.openAfterSave)) {
                markCustomProfile();
            }
            if (ImGui::Checkbox("Show In Folder After Save", &settings.output.showInFolderAfterSave)) {
                markCustomProfile();
            }
            if (ImGui::Checkbox("Copy Path After Save", &settings.output.copyPathAfterSave)) {
                markCustomProfile();
            }
            if (ImGui::Checkbox("Write Metadata Sidecar JSON", &settings.output.saveMetadataSidecar)) {
                markCustomProfile();
            }
            ImGui::SetItemTooltip("Writes a versioned .json sidecar next to the exported image.");

            ImGui::Spacing();
            if (!context.lastSavedPathUtf8.empty()) {
                ImGui::TextWrapped("Last saved: %s", context.lastSavedPathUtf8.c_str());
                if (ImGui::Button("Open Image", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    action.openSavedImage = true;
                }
                if (ImGui::Button("Show In Folder", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    action.showSavedImageInFolder = true;
                }
                if (ImGui::Button("Copy Saved Path", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    action.copySavedImagePath = true;
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    if (UiKit::drawVerticalSplitter("##ExportDialogSplitter",
                                    bodyHeight,
                                    context.settingsPaneWidth,
                                    minSettingsWidth,
                                    maxSettingsWidth,
                                    bodyWidth * 0.42f,
                                    splitterWidth)) {
        action.redrawRequested = true;
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("##ExportPreviewPane", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    const char* previewState = context.previewRefreshRequested ? "Rendering"
        : (context.previewDirty ? "Out of date"
                                : (context.hasPreviewTexture ? "Ready" : "No preview"));
    ImGui::TextDisabled("%s", previewState);
    ImGui::Separator();

    if (ImGui::Button("Refresh Preview")) {
        action.requestPreview = true;
    }
    ImGui::SameLine();
    bool autoRefresh = settings.quality.autoRefreshPreview;
    if (ImGui::Checkbox("Auto Refresh", &autoRefresh)) {
        settings.quality.autoRefreshPreview = autoRefresh;
        markCustomProfile();
        if (settings.quality.autoRefreshPreview && context.previewDirty) {
            action.settingsChanged = true;
        }
        action.redrawRequested = true;
    }
    const ExportPreviewQuality previewQualityOptions[] = {
        ExportPreviewQuality::Draft,
        ExportPreviewQuality::Normal
    };
    if (ImGui::BeginCombo("Preview Quality",
                          exportPreviewQualityLabel(settings.quality.previewQuality))) {
        for (ExportPreviewQuality quality : previewQualityOptions) {
            const bool selected = (settings.quality.previewQuality == quality);
            if (ImGui::Selectable(exportPreviewQualityLabel(quality), selected)) {
                settings.quality.previewQuality = quality;
                markCustomProfile();
                action.settingsChanged = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Render Final-Quality Preview")) {
        action.requestFinalPreview = true;
    }

    const ExportSupersampling effectiveSampling = effectiveExportSupersampling(settings);
    std::string samplingText = exportSupersamplingLabel(effectiveSampling);
    const int requestedSamplingFactor = supersamplingFactor(effectiveSampling);
    const int actualSamplingFactor =
        effectiveSupersamplingFactor(settings.size.width, settings.size.height, effectiveSampling);
    if (actualSamplingFactor != requestedSamplingFactor) {
        samplingText += " (effective ";
        samplingText += std::to_string(actualSamplingFactor);
        samplingText += "x)";
    }
    ImGui::Text("Output %d x %d  |  %s  |  %s  |  %s",
                settings.size.width, settings.size.height,
                exportFormatLabel(settings.output.format),
                samplingText.c_str(),
                exportProfileLabel(settings.profile));
    ImGui::TextDisabled("%s", exportAspectModeLabel(settings.output.aspectMode));

    if (!context.previewStatus.empty()) {
        ImGui::TextWrapped("%s", context.previewStatus.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button("Fit")) {
        context.previewZoom = 0.0f;
        context.previewPanX = 0.0f;
        context.previewPanY = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("100%")) {
        context.previewZoom = 1.0f;
        context.previewPanX = 0.0f;
        context.previewPanY = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("-")) {
        const float baseZoom = (context.previewZoom > 0.0f) ? context.previewZoom : 1.0f;
        context.previewZoom = std::clamp(baseZoom / 1.25f, 0.05f, 8.0f);
    }
    ImGui::SameLine();
    if (context.previewZoom > 0.0f) {
        ImGui::Text("Zoom %.0f%%", context.previewZoom * 100.0f);
    } else {
        ImGui::TextUnformatted("Zoom Fit");
    }
    ImGui::SameLine();
    if (ImGui::Button("+")) {
        const float baseZoom = (context.previewZoom > 0.0f) ? context.previewZoom : 1.0f;
        context.previewZoom = std::clamp(baseZoom * 1.25f, 0.05f, 8.0f);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Checkerboard", &context.previewCheckerboard);

    const ImVec2 previewStart = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = (std::max)(180.0f, canvasSize.x);
    canvasSize.y = (std::max)(160.0f, canvasSize.y);
    ImGui::InvisibleButton("##ExportPreviewCanvas", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft);
    const bool previewHovered = ImGui::IsItemHovered();
    const bool previewActive = ImGui::IsItemActive();
    const ImVec2 previewEnd(previewStart.x + canvasSize.x, previewStart.y + canvasSize.y);
    ImDrawList* previewDrawList = ImGui::GetWindowDrawList();
    if (context.previewCheckerboard) {
        drawCheckerboard(previewStart, previewEnd);
    } else {
        previewDrawList->AddRectFilled(previewStart, previewEnd, IM_COL32(44, 44, 50, 255));
    }

    if (context.hasPreviewTexture) {
        const float textureW = static_cast<float>(context.previewWidth);
        const float textureH = static_cast<float>(context.previewHeight);
        const float fitZoom = (std::min)(canvasSize.x / textureW, canvasSize.y / textureH);
        if (previewHovered && ImGui::GetIO().MouseWheel != 0.0f) {
            const float baseZoom = (context.previewZoom > 0.0f) ? context.previewZoom : fitZoom;
            const float factor = ImGui::GetIO().MouseWheel > 0.0f ? 1.15f : (1.0f / 1.15f);
            context.previewZoom = std::clamp(baseZoom * factor, 0.05f, 8.0f);
        }
        if (previewHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            context.previewZoom = 0.0f;
            context.previewPanX = 0.0f;
            context.previewPanY = 0.0f;
        }

        const float drawZoom = (context.previewZoom > 0.0f) ? context.previewZoom : fitZoom;
        const float drawW = textureW * drawZoom;
        const float drawH = textureH * drawZoom;
        const float maxPanX = (std::max)(0.0f, (drawW - canvasSize.x) * 0.5f);
        const float maxPanY = (std::max)(0.0f, (drawH - canvasSize.y) * 0.5f);
        if (previewActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
            (maxPanX > 0.0f || maxPanY > 0.0f)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            context.previewPanX += delta.x;
            context.previewPanY += delta.y;
        }
        context.previewPanX = std::clamp(context.previewPanX, -maxPanX, maxPanX);
        context.previewPanY = std::clamp(context.previewPanY, -maxPanY, maxPanY);

        const ImVec2 imageMin(
            previewStart.x + (canvasSize.x - drawW) * 0.5f + context.previewPanX,
            previewStart.y + (canvasSize.y - drawH) * 0.5f + context.previewPanY);
        const ImVec2 imageMax(imageMin.x + drawW, imageMin.y + drawH);
        previewDrawList->PushClipRect(previewStart, previewEnd, true);
        previewDrawList->AddImage(context.previewTexture, imageMin, imageMax);
        previewDrawList->PopClipRect();
    } else {
        const ImVec2 textSize = ImGui::CalcTextSize("No preview");
        previewDrawList->AddText(
            ImVec2(previewStart.x + (canvasSize.x - textSize.x) * 0.5f,
                   previewStart.y + (canvasSize.y - textSize.y) * 0.5f),
            IM_COL32(220, 220, 225, 220),
            "No preview");
    }
    previewDrawList->AddRect(previewStart, previewEnd, IM_COL32(145, 145, 155, 190));
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::Separator();
    const ExportSupersampling footerSampling = effectiveExportSupersampling(settings);
    const double footerMiB = bytesToMiB(
        estimateRgbaBufferBytes(settings.size.width, settings.size.height, footerSampling));
    const char* footerPreviewState = context.previewRefreshRequested ? "Preview rendering"
        : (context.previewDirty ? "Preview out of date"
                                : (context.hasPreviewTexture ? "Preview ready" : "Preview not rendered"));
    ImGui::Text("%s | %s | %d x %d | %s | %s | %.1f MiB | %s",
                footerPreviewState,
                exportProfileLabel(settings.profile),
                settings.size.width,
                settings.size.height,
                exportFormatLabel(settings.output.format),
                exportSupersamplingLabel(footerSampling),
                footerMiB,
                exportAspectModeLabel(settings.output.aspectMode));
    if (!context.status.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", context.status.c_str());
    }

    if (!context.exportBusy && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        action.requestSave = true;
    }

    if (ImGui::Button("Reset Settings", ImVec2(124.0f, 0.0f))) {
        resetExportSettings();
    }
    const float copyWidth = 74.0f;
    const float saveWidth = 104.0f;
    const float closeWidth = 74.0f;
    const float actionWidth = copyWidth + saveWidth + closeWidth + style.ItemSpacing.x * 2.0f;
    const float rightStart = ImGui::GetWindowWidth() - style.WindowPadding.x - actionWidth;
    if (rightStart > ImGui::GetCursorPosX() + style.ItemSpacing.x) {
        ImGui::SameLine(rightStart);
    } else {
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(context.exportBusy);
    if (ImGui::Button("Copy", ImVec2(copyWidth, 0.0f))) {
        action.requestCopy = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As...", ImVec2(saveWidth, 0.0f))) {
        action.requestSave = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(closeWidth, 0.0f))) {
        action.close = true;
    }

    if (previewChanged) {
        action.settingsChanged = true;
    }
    return action;
}

} // namespace XpressFormula::UI::Components
