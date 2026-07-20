// SPDX-License-Identifier: MIT
// ExportSettings.h - Transitional infrastructure entry point for export settings.
#pragma once

#include "../../UI/ExportSettings.h"

namespace XpressFormula::Infrastructure::Export {

using UI::ExportAppearanceSettings;
using UI::ExportAspectMode;
using UI::ExportBackgroundMode;
using UI::ExportFormat;
using UI::ExportOutputSettings;
using UI::ExportPreviewQuality;
using UI::ExportPreviewSize;
using UI::ExportProfile;
using UI::ExportQualityMode;
using UI::ExportQualityPreset;
using UI::ExportQualitySettings;
using UI::ExportResolvedView;
using UI::ExportSceneSettings;
using UI::ExportSettings;
using UI::ExportSizePreset;
using UI::ExportSizeSettings;
using UI::ExportSupersampling;
using UI::ExportWorldBounds;

using UI::applyCurrentViewScene;
using UI::applyExportSizePreset;
using UI::aspectLockedHeight;
using UI::aspectLockedWidth;
using UI::bytesToMiB;
using UI::clampExportDimension;
using UI::defaultExportSettings;
using UI::effectiveExportSupersampling;
using UI::effectiveSupersamplingFactor;
using UI::estimateRgbaBufferBytes;
using UI::exportAspectModeLabel;
using UI::exportAspectModeTooltip;
using UI::exportBackgroundModeLabel;
using UI::exportFormatLabel;
using UI::exportPreviewQualityLabel;
using UI::exportProfileLabel;
using UI::exportProfiles;
using UI::exportQualityModeLabel;
using UI::exportQualityPresetLabel;
using UI::exportScaleOptions;
using UI::exportSceneSettingsFromPlot;
using UI::exportSettingsForProfile;
using UI::exportSettingsMatchProfile;
using UI::exportSizePresets;
using UI::exportSupersamplingLabel;
using UI::kExportSizePreset1280x720;
using UI::kExportSizePreset1920x1080;
using UI::kExportSizePreset2048x2048;
using UI::kExportSizePreset2560x1440;
using UI::kExportSizePreset3840x2160;
using UI::kExportSizePreset1024x1024;
using UI::kExportSizePresetCurrent;
using UI::kExportSizePresetCustom;
using UI::kLargeExportWarningMiB;
using UI::kMaxExportDimension;
using UI::kMinExportDimension;
using UI::markExportSizeCustom;
using UI::maxPreviewDimensionForQuality;
using UI::normalizeExportSettings;
using UI::normalizeWorldBounds;
using UI::plotQualityDecisionForExport;
using UI::plotRenderOverridesForExport;
using UI::qualitySettingsForPreset;
using UI::resolveExportBackgroundColor;
using UI::resolveExportPreviewSize;
using UI::resolveExportView;
using UI::sameExportQualitySettings;
using UI::sameExportSceneSettings;
using UI::supersamplingFactor;
using UI::syncExportProfileAfterManualChange;
using UI::toDisplayLabel;
using UI::toStorageName;
using UI::validateExportSize;
using UI::worldBoundsHeight;
using UI::worldBoundsWidth;

} // namespace XpressFormula::Infrastructure::Export
