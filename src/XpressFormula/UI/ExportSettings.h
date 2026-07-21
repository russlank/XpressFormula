// SPDX-License-Identifier: MIT
// ExportSettings.h - Compatibility shim for infrastructure-owned export settings.
#pragma once

#include "PlotSettings.h"
#include "../Infrastructure/Export/ExportSettings.h"

namespace XpressFormula::UI {

using Infrastructure::Export::ExportAppearanceSettings;
using Infrastructure::Export::ExportAspectMode;
using Infrastructure::Export::ExportBackgroundMode;
using Infrastructure::Export::ExportFormat;
using Infrastructure::Export::ExportOutputSettings;
using Infrastructure::Export::ExportPreviewQuality;
using Infrastructure::Export::ExportPreviewSize;
using Infrastructure::Export::ExportProfile;
using Infrastructure::Export::ExportQualityMode;
using Infrastructure::Export::ExportQualityPreset;
using Infrastructure::Export::ExportQualitySettings;
using Infrastructure::Export::ExportResolvedView;
using Infrastructure::Export::ExportSceneSettings;
using Infrastructure::Export::ExportSettings;
using Infrastructure::Export::ExportSizePreset;
using Infrastructure::Export::ExportSizeSettings;
using Infrastructure::Export::ExportSupersampling;
using Infrastructure::Export::ExportWorldBounds;

using Infrastructure::Export::applyCurrentViewScene;
using Infrastructure::Export::applyExportSizePreset;
using Infrastructure::Export::aspectLockedHeight;
using Infrastructure::Export::aspectLockedWidth;
using Infrastructure::Export::bytesToMiB;
using Infrastructure::Export::clampExportDimension;
using Infrastructure::Export::defaultExportSettings;
using Infrastructure::Export::effectiveExportSupersampling;
using Infrastructure::Export::effectiveSupersamplingFactor;
using Infrastructure::Export::estimateRgbaBufferBytes;
using Infrastructure::Export::exportAspectModeLabel;
using Infrastructure::Export::exportAspectModeTooltip;
using Infrastructure::Export::exportBackgroundModeLabel;
using Infrastructure::Export::exportFormatLabel;
using Infrastructure::Export::exportPreviewQualityLabel;
using Infrastructure::Export::exportProfileLabel;
using Infrastructure::Export::exportProfiles;
using Infrastructure::Export::exportQualityModeLabel;
using Infrastructure::Export::exportQualityPresetLabel;
using Infrastructure::Export::exportScaleOptions;
using Infrastructure::Export::exportSceneSettingsFromPlot;
using Infrastructure::Export::exportSettingsForProfile;
using Infrastructure::Export::exportSettingsMatchProfile;
using Infrastructure::Export::exportSizePresets;
using Infrastructure::Export::exportSupersamplingLabel;
using Infrastructure::Export::kExportSizePreset1024x1024;
using Infrastructure::Export::kExportSizePreset1280x720;
using Infrastructure::Export::kExportSizePreset1920x1080;
using Infrastructure::Export::kExportSizePreset2048x2048;
using Infrastructure::Export::kExportSizePreset2560x1440;
using Infrastructure::Export::kExportSizePreset3840x2160;
using Infrastructure::Export::kExportSizePresetCurrent;
using Infrastructure::Export::kExportSizePresetCustom;
using Infrastructure::Export::kLargeExportWarningMiB;
using Infrastructure::Export::kMaxExportDimension;
using Infrastructure::Export::kMinExportDimension;
using Infrastructure::Export::markExportSizeCustom;
using Infrastructure::Export::maxPreviewDimensionForQuality;
using Infrastructure::Export::normalizeExportSettings;
using Infrastructure::Export::normalizeWorldBounds;
using Infrastructure::Export::plotQualityDecisionForExport;
using Infrastructure::Export::plotRenderOverridesForExport;
using Infrastructure::Export::qualitySettingsForPreset;
using Infrastructure::Export::resolveExportBackgroundColor;
using Infrastructure::Export::resolveExportPreviewSize;
using Infrastructure::Export::resolveExportView;
using Infrastructure::Export::sameExportQualitySettings;
using Infrastructure::Export::sameExportSceneSettings;
using Infrastructure::Export::supersamplingFactor;
using Infrastructure::Export::syncExportProfileAfterManualChange;
using Infrastructure::Export::toDisplayLabel;
using Infrastructure::Export::toStorageName;
using Infrastructure::Export::validateExportSize;
using Infrastructure::Export::worldBoundsHeight;
using Infrastructure::Export::worldBoundsWidth;

} // namespace XpressFormula::UI
