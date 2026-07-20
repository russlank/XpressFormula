// SPDX-License-Identifier: MIT
// ExportSettings.h - Testable export settings model, helpers, and presets.
#pragma once

#include "../../Model/PlotPolicy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace XpressFormula::Infrastructure::Export {

using Model::PlotQualityDecision;
using Model::PlotRenderOverrides;
using Model::PlotSettings;
using Model::clampImplicitSurfaceResolution;
using Model::clampSurfaceResolution;
using Model::clampWireThicknessScale;
using Model::isAxisTriadVisible;
using Model::kPlotDefaults;
using Model::normalizePlotSettings;
using Model::resolveCoordinateOverlayPolicy;

inline constexpr int kMinExportDimension = 16;
inline constexpr int kMaxExportDimension = 8192;
inline constexpr double kLargeExportWarningMiB = 256.0;

enum class ExportBackgroundMode {
    Current,
    Transparent,
    White,
    Black,
    Custom
};

enum class ExportFormat {
    Png,
    Bmp
};

enum class ExportQualityMode {
    Interactive,
    Override
};

enum class ExportQualityPreset {
    Draft,
    Normal,
    High,
    Ultra,
    Custom
};

enum class ExportSupersampling {
    Off = 1,
    X2 = 2,
    X4 = 4
};

enum class ExportAspectMode {
    PreserveMathematicalScale,
    PreserveVisibleBounds,
    CropToFill,
    StretchToOutput
};

enum class ExportPreviewQuality {
    Draft,
    Normal,
    Final
};

enum class ExportProfile {
    CurrentView,
    Presentation,
    TransparentIllustration,
    PrintGrayscale,
    HighQuality3D,
    Custom
};

struct ExportSizePreset {
    const char* label;
    const char* storageName;
    int width;
    int height;
    bool useCurrentSize;
    bool custom;
};

struct ExportSizeSettings {
    int width = 0;
    int height = 0;
    int scale = 1;
    int selectedPreset = 0;
    bool lockAspectRatio = true;
};

struct ExportAppearanceSettings {
    bool grayscaleOutput = false;
    ExportBackgroundMode backgroundMode = ExportBackgroundMode::Current;
    std::array<float, 4> backgroundColor = kPlotDefaults.backgroundColor;
};

struct ExportSceneSettings {
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showEnvelope = true;
    bool showAxisTriad = false;
};

struct ExportQualitySettings {
    ExportQualityMode mode = ExportQualityMode::Interactive;
    ExportQualityPreset preset = ExportQualityPreset::Normal;
    int surfaceResolution = 50;
    int implicitSurfaceResolution = 64;
    float wireThicknessScale = 1.0f;
    ExportSupersampling supersampling = ExportSupersampling::Off;
    ExportPreviewQuality previewQuality = ExportPreviewQuality::Normal;
    bool autoRefreshPreview = false;
};

struct ExportOutputSettings {
    ExportFormat format = ExportFormat::Png;
    ExportAspectMode aspectMode = ExportAspectMode::PreserveMathematicalScale;
    bool openAfterSave = false;
    bool showInFolderAfterSave = false;
    bool copyPathAfterSave = false;
    bool saveMetadataSidecar = false;
};

struct ExportSettings {
    ExportProfile profile = ExportProfile::CurrentView;
    ExportSizeSettings size;
    ExportAppearanceSettings appearance;
    ExportSceneSettings scene;
    ExportQualitySettings quality;
    ExportOutputSettings output;
};

struct ExportWorldBounds {
    double xMin = -1.0;
    double xMax = 1.0;
    double yMin = -1.0;
    double yMax = 1.0;
};

struct ExportResolvedView {
    int outputWidth = 0;
    int outputHeight = 0;
    ExportAspectMode aspectMode = ExportAspectMode::PreserveMathematicalScale;
    ExportWorldBounds sourceBounds;
    ExportWorldBounds visibleBounds;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double marginLeftPx = 0.0;
    double marginRightPx = 0.0;
    double marginTopPx = 0.0;
    double marginBottomPx = 0.0;
    double contentWidthPx = 0.0;
    double contentHeightPx = 0.0;
    bool uniformScale = true;
};

struct ExportPreviewSize {
    int width = 0;
    int height = 0;
    bool reducedFromOutput = false;
};

inline int clampExportDimension(int value) {
    return std::clamp(value, kMinExportDimension, kMaxExportDimension);
}

inline void validateExportSize(int& width, int& height) {
    width = clampExportDimension(width);
    height = clampExportDimension(height);
}

inline int aspectLockedHeight(int width, int sourceWidth, int sourceHeight) {
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        return clampExportDimension(width);
    }

    return clampExportDimension(static_cast<int>(
        std::lround(static_cast<double>(width) * sourceHeight / sourceWidth)));
}

inline int aspectLockedWidth(int height, int sourceWidth, int sourceHeight) {
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        return clampExportDimension(height);
    }

    return clampExportDimension(static_cast<int>(
        std::lround(static_cast<double>(height) * sourceWidth / sourceHeight)));
}

inline int supersamplingFactor(ExportSupersampling supersampling) {
    return static_cast<int>(supersampling);
}

inline int effectiveSupersamplingFactor(int width, int height, ExportSupersampling supersampling) {
    const int desiredScale = supersamplingFactor(supersampling);
    const int safeWidth = clampExportDimension(width);
    const int safeHeight = clampExportDimension(height);
    const int maxScaleForWidth = (std::max)(1, kMaxExportDimension / safeWidth);
    const int maxScaleForHeight = (std::max)(1, kMaxExportDimension / safeHeight);
    return std::clamp(desiredScale, 1, (std::min)(maxScaleForWidth, maxScaleForHeight));
}

inline std::uint64_t estimateRgbaBufferBytes(int width,
                                             int height,
                                             ExportSupersampling supersampling = ExportSupersampling::Off) {
    const auto safeWidth = static_cast<std::uint64_t>(clampExportDimension(width));
    const auto safeHeight = static_cast<std::uint64_t>(clampExportDimension(height));
    const auto scale = static_cast<std::uint64_t>(
        effectiveSupersamplingFactor(width, height, supersampling));
    return safeWidth * safeHeight * 4ull * scale * scale;
}

inline double bytesToMiB(std::uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

inline ExportWorldBounds normalizeWorldBounds(ExportWorldBounds bounds) {
    if (bounds.xMin > bounds.xMax) {
        std::swap(bounds.xMin, bounds.xMax);
    }
    if (bounds.yMin > bounds.yMax) {
        std::swap(bounds.yMin, bounds.yMax);
    }

    constexpr double minRange = 1e-9;
    const double centerX = (bounds.xMin + bounds.xMax) * 0.5;
    const double centerY = (bounds.yMin + bounds.yMax) * 0.5;
    double width = bounds.xMax - bounds.xMin;
    double height = bounds.yMax - bounds.yMin;
    if (!(width > minRange) || !std::isfinite(width)) {
        width = 2.0;
    }
    if (!(height > minRange) || !std::isfinite(height)) {
        height = 2.0;
    }

    bounds.xMin = centerX - width * 0.5;
    bounds.xMax = centerX + width * 0.5;
    bounds.yMin = centerY - height * 0.5;
    bounds.yMax = centerY + height * 0.5;
    return bounds;
}

inline double worldBoundsWidth(const ExportWorldBounds& bounds) {
    return bounds.xMax - bounds.xMin;
}

inline double worldBoundsHeight(const ExportWorldBounds& bounds) {
    return bounds.yMax - bounds.yMin;
}

inline ExportResolvedView resolveExportView(int outputWidth,
                                            int outputHeight,
                                            ExportWorldBounds sourceBounds,
                                            ExportAspectMode aspectMode) {
    ExportResolvedView resolved;
    resolved.outputWidth = clampExportDimension(outputWidth);
    resolved.outputHeight = clampExportDimension(outputHeight);
    resolved.aspectMode = aspectMode;
    resolved.sourceBounds = normalizeWorldBounds(sourceBounds);
    resolved.visibleBounds = resolved.sourceBounds;

    const double outW = static_cast<double>(resolved.outputWidth);
    const double outH = static_cast<double>(resolved.outputHeight);
    const double sourceW = worldBoundsWidth(resolved.sourceBounds);
    const double sourceH = worldBoundsHeight(resolved.sourceBounds);
    const double centerX = (resolved.sourceBounds.xMin + resolved.sourceBounds.xMax) * 0.5;
    const double centerY = (resolved.sourceBounds.yMin + resolved.sourceBounds.yMax) * 0.5;
    const double sourceAspect = sourceW / sourceH;
    const double outputAspect = outW / outH;

    auto setVisibleFromSize = [&](double width, double height) {
        resolved.visibleBounds.xMin = centerX - width * 0.5;
        resolved.visibleBounds.xMax = centerX + width * 0.5;
        resolved.visibleBounds.yMin = centerY - height * 0.5;
        resolved.visibleBounds.yMax = centerY + height * 0.5;
    };

    switch (aspectMode) {
        case ExportAspectMode::PreserveVisibleBounds: {
            const double scale = (std::min)(outW / sourceW, outH / sourceH);
            resolved.scaleX = scale;
            resolved.scaleY = scale;
            resolved.contentWidthPx = sourceW * scale;
            resolved.contentHeightPx = sourceH * scale;
            resolved.marginLeftPx = (outW - resolved.contentWidthPx) * 0.5;
            resolved.marginRightPx = outW - resolved.contentWidthPx - resolved.marginLeftPx;
            resolved.marginTopPx = (outH - resolved.contentHeightPx) * 0.5;
            resolved.marginBottomPx = outH - resolved.contentHeightPx - resolved.marginTopPx;
            resolved.uniformScale = true;
            break;
        }
        case ExportAspectMode::CropToFill: {
            const double scale = (std::max)(outW / sourceW, outH / sourceH);
            resolved.scaleX = scale;
            resolved.scaleY = scale;
            resolved.contentWidthPx = outW;
            resolved.contentHeightPx = outH;
            setVisibleFromSize(outW / scale, outH / scale);
            resolved.uniformScale = true;
            break;
        }
        case ExportAspectMode::StretchToOutput:
            resolved.scaleX = outW / sourceW;
            resolved.scaleY = outH / sourceH;
            resolved.contentWidthPx = outW;
            resolved.contentHeightPx = outH;
            resolved.uniformScale = false;
            break;
        case ExportAspectMode::PreserveMathematicalScale:
        default: {
            double targetW = sourceW;
            double targetH = sourceH;
            if (outputAspect > sourceAspect) {
                targetW = sourceH * outputAspect;
            } else if (outputAspect < sourceAspect) {
                targetH = sourceW / outputAspect;
            }
            setVisibleFromSize(targetW, targetH);
            const double scale = outW / targetW;
            resolved.scaleX = scale;
            resolved.scaleY = scale;
            resolved.contentWidthPx = outW;
            resolved.contentHeightPx = outH;
            resolved.uniformScale = true;
            break;
        }
    }

    resolved.marginLeftPx = (std::max)(0.0, resolved.marginLeftPx);
    resolved.marginRightPx = (std::max)(0.0, resolved.marginRightPx);
    resolved.marginTopPx = (std::max)(0.0, resolved.marginTopPx);
    resolved.marginBottomPx = (std::max)(0.0, resolved.marginBottomPx);
    resolved.contentWidthPx = (std::max)(1.0, resolved.contentWidthPx);
    resolved.contentHeightPx = (std::max)(1.0, resolved.contentHeightPx);
    return resolved;
}

inline int maxPreviewDimensionForQuality(ExportPreviewQuality quality) {
    switch (quality) {
        case ExportPreviewQuality::Draft: return 520;
        case ExportPreviewQuality::Normal: return 960;
        case ExportPreviewQuality::Final: return kMaxExportDimension;
        default: return 960;
    }
}

inline ExportPreviewSize resolveExportPreviewSize(int outputWidth,
                                                  int outputHeight,
                                                  ExportPreviewQuality quality) {
    ExportPreviewSize resolved;
    const int safeWidth = clampExportDimension(outputWidth);
    const int safeHeight = clampExportDimension(outputHeight);
    const int maxDimension = maxPreviewDimensionForQuality(quality);
    const int longestSide = (std::max)(safeWidth, safeHeight);
    if (quality == ExportPreviewQuality::Final || longestSide <= maxDimension) {
        resolved.width = safeWidth;
        resolved.height = safeHeight;
        resolved.reducedFromOutput = false;
        return resolved;
    }

    const double scale = static_cast<double>(maxDimension) / longestSide;
    resolved.width = clampExportDimension(static_cast<int>(std::lround(safeWidth * scale)));
    resolved.height = clampExportDimension(static_cast<int>(std::lround(safeHeight * scale)));
    resolved.reducedFromOutput = true;
    return resolved;
}

inline constexpr int kExportSizePresetCurrent = 0;
inline constexpr int kExportSizePreset1280x720 = 1;
inline constexpr int kExportSizePreset1920x1080 = 2;
inline constexpr int kExportSizePreset2560x1440 = 3;
inline constexpr int kExportSizePreset3840x2160 = 4;
inline constexpr int kExportSizePreset1024x1024 = 5;
inline constexpr int kExportSizePreset2048x2048 = 6;
inline constexpr int kExportSizePresetCustom = 7;

inline const std::array<ExportSizePreset, 8>& exportSizePresets() {
    static constexpr std::array<ExportSizePreset, 8> presets = {{
        { "Current", "current", 0, 0, true, false },
        { "1280 x 720", "1280x720", 1280, 720, false, false },
        { "1920 x 1080", "1920x1080", 1920, 1080, false, false },
        { "2560 x 1440", "2560x1440", 2560, 1440, false, false },
        { "3840 x 2160", "3840x2160", 3840, 2160, false, false },
        { "1024 x 1024", "1024x1024", 1024, 1024, false, false },
        { "2048 x 2048", "2048x2048", 2048, 2048, false, false },
        { "Custom", "custom", 0, 0, false, true },
    }};
    return presets;
}

inline const std::array<ExportProfile, 6>& exportProfiles() {
    static constexpr std::array<ExportProfile, 6> profiles = {{
        ExportProfile::CurrentView,
        ExportProfile::Presentation,
        ExportProfile::TransparentIllustration,
        ExportProfile::PrintGrayscale,
        ExportProfile::HighQuality3D,
        ExportProfile::Custom
    }};
    return profiles;
}

inline const std::array<int, 4>& exportScaleOptions() {
    static constexpr std::array<int, 4> scales = {{ 1, 2, 3, 4 }};
    return scales;
}

inline ExportQualitySettings qualitySettingsForPreset(ExportQualityPreset preset) {
    auto makeSettings = [](ExportQualityPreset presetValue,
                           int surfaceResolution,
                           int implicitSurfaceResolution,
                           float wireThicknessScale,
                           ExportSupersampling supersampling) {
        ExportQualitySettings settings;
        settings.preset = presetValue;
        settings.surfaceResolution = surfaceResolution;
        settings.implicitSurfaceResolution = implicitSurfaceResolution;
        settings.wireThicknessScale = wireThicknessScale;
        settings.supersampling = supersampling;
        return settings;
    };

    switch (preset) {
        case ExportQualityPreset::Draft:
            return makeSettings(preset, 32, 48, 0.75f, ExportSupersampling::Off);
        case ExportQualityPreset::High:
            return makeSettings(preset, 80, 96, 1.25f, ExportSupersampling::X2);
        case ExportQualityPreset::Ultra:
            return makeSettings(preset, 120, 128, 1.5f, ExportSupersampling::X4);
        case ExportQualityPreset::Custom:
            return makeSettings(preset, 50, 64, 1.0f, ExportSupersampling::Off);
        case ExportQualityPreset::Normal:
        default:
            return makeSettings(ExportQualityPreset::Normal, 50, 64, 1.0f,
                                ExportSupersampling::Off);
    }
}

inline ExportSceneSettings exportSceneSettingsFromPlot(const PlotSettings& plot) {
    PlotSettings normalized = plot;
    normalizePlotSettings(normalized);

    ExportSceneSettings scene;
    scene.showGrid = normalized.showGrid;
    scene.showCoordinates = normalized.showCoordinates;
    scene.showWires = normalized.showWires;
    scene.showEnvelope = normalized.showSurfaceEnvelope;
    scene.showAxisTriad = normalized.showAxisTriad;
    resolveCoordinateOverlayPolicy(scene.showCoordinates, scene.showAxisTriad);
    return scene;
}

inline void normalizeExportSettings(ExportSettings& settings) {
    settings.size.selectedPreset = std::clamp(
        settings.size.selectedPreset,
        0,
        static_cast<int>(exportSizePresets().size()) - 1);
    settings.size.scale = std::clamp(settings.size.scale, 1, 4);
    if (settings.size.width > 0 || settings.size.height > 0) {
        validateExportSize(settings.size.width, settings.size.height);
    }

    resolveCoordinateOverlayPolicy(settings.scene.showCoordinates, settings.scene.showAxisTriad);

    for (float& component : settings.appearance.backgroundColor) {
        component = std::isfinite(component) ? std::clamp(component, 0.0f, 1.0f) : 1.0f;
    }

    settings.quality.surfaceResolution =
        clampSurfaceResolution(settings.quality.surfaceResolution);
    settings.quality.implicitSurfaceResolution =
        clampImplicitSurfaceResolution(settings.quality.implicitSurfaceResolution);
    settings.quality.wireThicknessScale =
        clampWireThicknessScale(settings.quality.wireThicknessScale);
}

inline ExportSettings defaultExportSettings() {
    ExportSettings settings;
    settings.quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
    normalizeExportSettings(settings);
    return settings;
}

inline ExportSettings exportSettingsForProfile(ExportProfile profile,
                                               const ExportSceneSettings* currentScene = nullptr) {
    ExportSettings settings = defaultExportSettings();
    settings.profile = profile;

    auto setQualityPreset = [&](ExportQualityPreset preset) {
        settings.quality = qualitySettingsForPreset(preset);
        settings.quality.mode = ExportQualityMode::Override;
    };

    switch (profile) {
        case ExportProfile::Presentation:
            settings.size.selectedPreset = kExportSizePreset1920x1080;
            setQualityPreset(ExportQualityPreset::High);
            settings.output.saveMetadataSidecar = false;
            break;
        case ExportProfile::TransparentIllustration:
            settings.size.selectedPreset = kExportSizePreset2048x2048;
            settings.scene.showGrid = false;
            settings.scene.showCoordinates = false;
            settings.scene.showWires = true;
            settings.scene.showEnvelope = true;
            settings.appearance.backgroundMode = ExportBackgroundMode::Transparent;
            setQualityPreset(ExportQualityPreset::High);
            settings.output.saveMetadataSidecar = true;
            break;
        case ExportProfile::PrintGrayscale:
            settings.size.selectedPreset = kExportSizePreset2048x2048;
            settings.appearance.grayscaleOutput = true;
            settings.scene.showGrid = true;
            settings.scene.showCoordinates = true;
            settings.scene.showWires = false;
            settings.scene.showEnvelope = false;
            settings.appearance.backgroundMode = ExportBackgroundMode::White;
            settings.output.aspectMode = ExportAspectMode::PreserveVisibleBounds;
            setQualityPreset(ExportQualityPreset::High);
            settings.output.saveMetadataSidecar = true;
            break;
        case ExportProfile::HighQuality3D:
            settings.size.selectedPreset = kExportSizePreset3840x2160;
            settings.scene.showGrid = true;
            settings.scene.showCoordinates = false;
            settings.scene.showWires = true;
            settings.scene.showEnvelope = true;
            settings.scene.showAxisTriad = true;
            setQualityPreset(ExportQualityPreset::Ultra);
            settings.quality.previewQuality = ExportPreviewQuality::Draft;
            settings.output.saveMetadataSidecar = true;
            break;
        case ExportProfile::Custom:
            settings.size.selectedPreset = kExportSizePresetCustom;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
            break;
        case ExportProfile::CurrentView:
        default:
            settings.profile = ExportProfile::CurrentView;
            settings.size.selectedPreset = kExportSizePresetCurrent;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
            if (currentScene) {
                settings.scene = *currentScene;
            }
            break;
    }

    normalizeExportSettings(settings);
    return settings;
}

inline void applyCurrentViewScene(ExportSettings& settings, const ExportSceneSettings& currentScene) {
    settings.scene = currentScene;
    normalizeExportSettings(settings);
}

inline bool applyExportSizePreset(ExportSettings& settings, int sourceWidth, int sourceHeight) {
    normalizeExportSettings(settings);
    const auto& preset = exportSizePresets()[static_cast<size_t>(settings.size.selectedPreset)];
    if (preset.custom) {
        return false;
    }

    const int baseWidth = preset.useCurrentSize ? sourceWidth : preset.width;
    const int baseHeight = preset.useCurrentSize ? sourceHeight : preset.height;
    if (baseWidth <= 0 || baseHeight <= 0) {
        return false;
    }

    settings.size.width = clampExportDimension(baseWidth * settings.size.scale);
    settings.size.height = clampExportDimension(baseHeight * settings.size.scale);
    return true;
}

inline void markExportSizeCustom(ExportSettings& settings) {
    settings.size.selectedPreset = kExportSizePresetCustom;
}

inline bool sameExportSceneSettings(const ExportSceneSettings& lhs,
                                    const ExportSceneSettings& rhs) {
    return lhs.showGrid == rhs.showGrid &&
           lhs.showCoordinates == rhs.showCoordinates &&
           lhs.showWires == rhs.showWires &&
           lhs.showEnvelope == rhs.showEnvelope &&
           lhs.showAxisTriad == rhs.showAxisTriad;
}

inline bool sameExportQualitySettings(const ExportQualitySettings& lhs,
                                      const ExportQualitySettings& rhs) {
    return lhs.mode == rhs.mode &&
           lhs.preset == rhs.preset &&
           lhs.surfaceResolution == rhs.surfaceResolution &&
           lhs.implicitSurfaceResolution == rhs.implicitSurfaceResolution &&
           std::abs(lhs.wireThicknessScale - rhs.wireThicknessScale) < 0.0001f &&
           lhs.supersampling == rhs.supersampling &&
           lhs.previewQuality == rhs.previewQuality &&
           lhs.autoRefreshPreview == rhs.autoRefreshPreview;
}

inline bool exportSettingsMatchProfile(const ExportSettings& settings,
                                       ExportProfile profile,
                                       const ExportSceneSettings* currentScene = nullptr) {
    if (profile == ExportProfile::Custom) {
        return settings.profile == ExportProfile::Custom;
    }

    ExportSettings normalized = settings;
    normalizeExportSettings(normalized);
    const ExportSettings expected = exportSettingsForProfile(profile, currentScene);

    const bool sameCustomBackground =
        normalized.appearance.backgroundMode != ExportBackgroundMode::Custom ||
        normalized.appearance.backgroundColor == expected.appearance.backgroundColor;

    return normalized.size.selectedPreset == expected.size.selectedPreset &&
           normalized.size.scale == expected.size.scale &&
           normalized.size.lockAspectRatio == expected.size.lockAspectRatio &&
           normalized.appearance.grayscaleOutput == expected.appearance.grayscaleOutput &&
           normalized.appearance.backgroundMode == expected.appearance.backgroundMode &&
           sameCustomBackground &&
           sameExportSceneSettings(normalized.scene, expected.scene) &&
           sameExportQualitySettings(normalized.quality, expected.quality) &&
           normalized.output.format == expected.output.format &&
           normalized.output.aspectMode == expected.output.aspectMode &&
           normalized.output.openAfterSave == expected.output.openAfterSave &&
           normalized.output.showInFolderAfterSave == expected.output.showInFolderAfterSave &&
           normalized.output.copyPathAfterSave == expected.output.copyPathAfterSave &&
           normalized.output.saveMetadataSidecar == expected.output.saveMetadataSidecar;
}

inline bool syncExportProfileAfterManualChange(ExportSettings& settings,
                                               const ExportSceneSettings* currentScene = nullptr) {
    if (settings.profile == ExportProfile::Custom ||
        exportSettingsMatchProfile(settings, settings.profile, currentScene)) {
        return false;
    }
    settings.profile = ExportProfile::Custom;
    return true;
}

inline ExportSupersampling effectiveExportSupersampling(const ExportSettings& settings) {
    return settings.quality.mode == ExportQualityMode::Override
        ? settings.quality.supersampling
        : ExportSupersampling::Off;
}

inline std::array<float, 4> resolveExportBackgroundColor(const ExportSettings& settings) {
    switch (settings.appearance.backgroundMode) {
        case ExportBackgroundMode::Transparent:
            return { 0.0f, 0.0f, 0.0f, 0.0f };
        case ExportBackgroundMode::White:
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        case ExportBackgroundMode::Black:
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        case ExportBackgroundMode::Custom:
            return settings.appearance.backgroundColor;
        case ExportBackgroundMode::Current:
        default:
            return kPlotDefaults.backgroundColor;
    }
}

inline PlotRenderOverrides plotRenderOverridesForExport(const ExportSettings& settings) {
    PlotRenderOverrides overrides;
    overrides.active = true;
    overrides.showGrid = settings.scene.showGrid;
    overrides.showCoordinates = settings.scene.showCoordinates;
    overrides.showWires = settings.scene.showWires;
    overrides.showEnvelope = settings.scene.showEnvelope;
    overrides.showAxisTriad =
        isAxisTriadVisible(settings.scene.showCoordinates, settings.scene.showAxisTriad);
    overrides.showHud = false;
    overrides.showCanvasBorder = false;
    overrides.backgroundColor = resolveExportBackgroundColor(settings);
    return overrides;
}

inline PlotQualityDecision plotQualityDecisionForExport(const ExportSettings& settings) {
    PlotQualityDecision decision;
    if (settings.quality.mode == ExportQualityMode::Override) {
        decision.overrideQuality = true;
        decision.surfaceResolution = settings.quality.surfaceResolution;
        decision.implicitSurfaceResolution = settings.quality.implicitSurfaceResolution;
        decision.wireThicknessScale = settings.quality.wireThicknessScale;
    }
    return decision;
}

inline std::string_view toStorageName(ExportProfile profile) {
    switch (profile) {
        case ExportProfile::CurrentView: return "currentView";
        case ExportProfile::Presentation: return "presentation";
        case ExportProfile::TransparentIllustration: return "transparentIllustration";
        case ExportProfile::PrintGrayscale: return "printGrayscale";
        case ExportProfile::HighQuality3D: return "highQuality3D";
        case ExportProfile::Custom: return "custom";
        default: return "currentView";
    }
}

inline std::string_view toDisplayLabel(ExportProfile profile) {
    switch (profile) {
        case ExportProfile::CurrentView: return "Current View";
        case ExportProfile::Presentation: return "Presentation";
        case ExportProfile::TransparentIllustration: return "Transparent Illustration";
        case ExportProfile::PrintGrayscale: return "Print Grayscale";
        case ExportProfile::HighQuality3D: return "High-Quality 3D";
        case ExportProfile::Custom: return "Custom";
        default: return "Current View";
    }
}

inline const char* exportProfileLabel(ExportProfile profile) {
    return toDisplayLabel(profile).data();
}

inline std::string_view toStorageName(ExportBackgroundMode mode) {
    switch (mode) {
        case ExportBackgroundMode::Current: return "current";
        case ExportBackgroundMode::Transparent: return "transparent";
        case ExportBackgroundMode::White: return "white";
        case ExportBackgroundMode::Black: return "black";
        case ExportBackgroundMode::Custom: return "custom";
        default: return "current";
    }
}

inline std::string_view toDisplayLabel(ExportBackgroundMode mode) {
    switch (mode) {
        case ExportBackgroundMode::Current: return "Use current";
        case ExportBackgroundMode::Transparent: return "Transparent";
        case ExportBackgroundMode::White: return "White";
        case ExportBackgroundMode::Black: return "Black";
        case ExportBackgroundMode::Custom: return "Custom";
        default: return "Use current";
    }
}

inline const char* exportBackgroundModeLabel(ExportBackgroundMode mode) {
    return toDisplayLabel(mode).data();
}

inline std::string_view toStorageName(ExportFormat format) {
    switch (format) {
        case ExportFormat::Png: return "png";
        case ExportFormat::Bmp: return "bmp";
        default: return "png";
    }
}

inline std::string_view toDisplayLabel(ExportFormat format) {
    switch (format) {
        case ExportFormat::Png: return "PNG";
        case ExportFormat::Bmp: return "BMP";
        default: return "PNG";
    }
}

inline const char* exportFormatLabel(ExportFormat format) {
    return toDisplayLabel(format).data();
}

inline std::string_view toStorageName(ExportQualityMode mode) {
    switch (mode) {
        case ExportQualityMode::Interactive: return "interactive";
        case ExportQualityMode::Override: return "override";
        default: return "interactive";
    }
}

inline std::string_view toDisplayLabel(ExportQualityMode mode) {
    switch (mode) {
        case ExportQualityMode::Interactive: return "Interactive";
        case ExportQualityMode::Override: return "Override";
        default: return "Interactive";
    }
}

inline const char* exportQualityModeLabel(ExportQualityMode mode) {
    return toDisplayLabel(mode).data();
}

inline std::string_view toStorageName(ExportQualityPreset preset) {
    switch (preset) {
        case ExportQualityPreset::Draft: return "draft";
        case ExportQualityPreset::Normal: return "normal";
        case ExportQualityPreset::High: return "high";
        case ExportQualityPreset::Ultra: return "ultra";
        case ExportQualityPreset::Custom: return "custom";
        default: return "normal";
    }
}

inline std::string_view toDisplayLabel(ExportQualityPreset preset) {
    switch (preset) {
        case ExportQualityPreset::Draft: return "Draft";
        case ExportQualityPreset::Normal: return "Normal";
        case ExportQualityPreset::High: return "High";
        case ExportQualityPreset::Ultra: return "Ultra";
        case ExportQualityPreset::Custom: return "Custom";
        default: return "Normal";
    }
}

inline const char* exportQualityPresetLabel(ExportQualityPreset preset) {
    return toDisplayLabel(preset).data();
}

inline std::string_view toStorageName(ExportSupersampling supersampling) {
    switch (supersampling) {
        case ExportSupersampling::Off: return "off";
        case ExportSupersampling::X2: return "x2";
        case ExportSupersampling::X4: return "x4";
        default: return "off";
    }
}

inline std::string_view toDisplayLabel(ExportSupersampling supersampling) {
    switch (supersampling) {
        case ExportSupersampling::Off: return "Off";
        case ExportSupersampling::X2: return "2x";
        case ExportSupersampling::X4: return "4x";
        default: return "Off";
    }
}

inline const char* exportSupersamplingLabel(ExportSupersampling supersampling) {
    return toDisplayLabel(supersampling).data();
}

inline std::string_view toStorageName(ExportAspectMode mode) {
    switch (mode) {
        case ExportAspectMode::PreserveMathematicalScale: return "preserveMathematicalScale";
        case ExportAspectMode::PreserveVisibleBounds: return "preserveVisibleBounds";
        case ExportAspectMode::CropToFill: return "cropToFill";
        case ExportAspectMode::StretchToOutput: return "stretchToOutput";
        default: return "preserveMathematicalScale";
    }
}

inline std::string_view toDisplayLabel(ExportAspectMode mode) {
    switch (mode) {
        case ExportAspectMode::PreserveMathematicalScale: return "Preserve proportions";
        case ExportAspectMode::PreserveVisibleBounds: return "Preserve visible bounds";
        case ExportAspectMode::CropToFill: return "Crop to fill";
        case ExportAspectMode::StretchToOutput: return "Stretch to output";
        default: return "Preserve proportions";
    }
}

inline const char* exportAspectModeLabel(ExportAspectMode mode) {
    return toDisplayLabel(mode).data();
}

inline const char* exportAspectModeTooltip(ExportAspectMode mode) {
    switch (mode) {
        case ExportAspectMode::PreserveMathematicalScale:
            return "Keeps X and Y units equal and expands the world range when output aspect differs.";
        case ExportAspectMode::PreserveVisibleBounds:
            return "Keeps the exact current world bounds and adds centered margins when needed.";
        case ExportAspectMode::CropToFill:
            return "Keeps X and Y units equal, fills the output, and crops one axis when needed.";
        case ExportAspectMode::StretchToOutput:
            return "Keeps exact current bounds but may distort mathematical proportions.";
        default:
            return "";
    }
}

inline std::string_view toStorageName(ExportPreviewQuality quality) {
    switch (quality) {
        case ExportPreviewQuality::Draft: return "draft";
        case ExportPreviewQuality::Normal: return "normal";
        case ExportPreviewQuality::Final: return "final";
        default: return "normal";
    }
}

inline std::string_view toDisplayLabel(ExportPreviewQuality quality) {
    switch (quality) {
        case ExportPreviewQuality::Draft: return "Draft";
        case ExportPreviewQuality::Normal: return "Normal";
        case ExportPreviewQuality::Final: return "Final";
        default: return "Normal";
    }
}

inline const char* exportPreviewQualityLabel(ExportPreviewQuality quality) {
    return toDisplayLabel(quality).data();
}

} // namespace XpressFormula::Infrastructure::Export
