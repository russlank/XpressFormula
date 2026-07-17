// SPDX-License-Identifier: MIT
// ExportSettings.h - Testable helpers and presets for plot image export.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace XpressFormula::UI {

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
    int width;
    int height;
    bool useCurrentSize;
    bool custom;
};

struct ExportQualitySettings {
    ExportQualityPreset preset = ExportQualityPreset::Normal;
    int surfaceResolution = 50;
    int implicitSurfaceResolution = 64;
    float wireThicknessScale = 1.0f;
    ExportSupersampling supersampling = ExportSupersampling::Off;
};

struct ExportProfileSettings {
    int selectedSizePreset = 0;
    int scale = 1;
    bool lockAspectRatio = true;
    bool grayscaleOutput = false;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showEnvelope = true;
    bool showAxisTriad = false;
    ExportBackgroundMode backgroundMode = ExportBackgroundMode::Current;
    ExportFormat format = ExportFormat::Png;
    ExportAspectMode aspectMode = ExportAspectMode::PreserveMathematicalScale;
    ExportQualityMode qualityMode = ExportQualityMode::Interactive;
    ExportQualitySettings quality;
    ExportPreviewQuality previewQuality = ExportPreviewQuality::Normal;
    bool autoRefreshPreview = false;
    bool openAfterSave = false;
    bool showInFolderAfterSave = false;
    bool copyPathAfterSave = false;
    bool saveMetadataSidecar = false;
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
        { "Current", 0, 0, true, false },
        { "1280 x 720", 1280, 720, false, false },
        { "1920 x 1080", 1920, 1080, false, false },
        { "2560 x 1440", 2560, 1440, false, false },
        { "3840 x 2160", 3840, 2160, false, false },
        { "1024 x 1024", 1024, 1024, false, false },
        { "2048 x 2048", 2048, 2048, false, false },
        { "Custom", 0, 0, false, true },
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
    switch (preset) {
        case ExportQualityPreset::Draft:
            return { preset, 32, 48, 0.75f, ExportSupersampling::Off };
        case ExportQualityPreset::High:
            return { preset, 80, 96, 1.25f, ExportSupersampling::X2 };
        case ExportQualityPreset::Ultra:
            return { preset, 120, 128, 1.5f, ExportSupersampling::X4 };
        case ExportQualityPreset::Custom:
            return { preset, 50, 64, 1.0f, ExportSupersampling::Off };
        case ExportQualityPreset::Normal:
        default:
            return { ExportQualityPreset::Normal, 50, 64, 1.0f, ExportSupersampling::Off };
    }
}

inline ExportProfileSettings exportProfileSettings(ExportProfile profile) {
    ExportProfileSettings settings;
    switch (profile) {
        case ExportProfile::Presentation:
            settings.selectedSizePreset = kExportSizePreset1920x1080;
            settings.qualityMode = ExportQualityMode::Override;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::High);
            settings.saveMetadataSidecar = false;
            return settings;
        case ExportProfile::TransparentIllustration:
            settings.selectedSizePreset = kExportSizePreset2048x2048;
            settings.showGrid = false;
            settings.showCoordinates = false;
            settings.showWires = true;
            settings.showEnvelope = true;
            settings.backgroundMode = ExportBackgroundMode::Transparent;
            settings.qualityMode = ExportQualityMode::Override;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::High);
            settings.saveMetadataSidecar = true;
            return settings;
        case ExportProfile::PrintGrayscale:
            settings.selectedSizePreset = kExportSizePreset2048x2048;
            settings.grayscaleOutput = true;
            settings.showGrid = true;
            settings.showCoordinates = true;
            settings.showWires = false;
            settings.showEnvelope = false;
            settings.backgroundMode = ExportBackgroundMode::White;
            settings.aspectMode = ExportAspectMode::PreserveVisibleBounds;
            settings.qualityMode = ExportQualityMode::Override;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::High);
            settings.saveMetadataSidecar = true;
            return settings;
        case ExportProfile::HighQuality3D:
            settings.selectedSizePreset = kExportSizePreset3840x2160;
            settings.showGrid = true;
            settings.showCoordinates = false;
            settings.showWires = true;
            settings.showEnvelope = true;
            settings.showAxisTriad = true;
            settings.qualityMode = ExportQualityMode::Override;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::Ultra);
            settings.previewQuality = ExportPreviewQuality::Draft;
            settings.saveMetadataSidecar = true;
            return settings;
        case ExportProfile::Custom:
            settings.selectedSizePreset = kExportSizePresetCustom;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
            return settings;
        case ExportProfile::CurrentView:
        default:
            settings.selectedSizePreset = kExportSizePresetCurrent;
            settings.quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
            return settings;
    }
}

inline const char* exportProfileLabel(ExportProfile profile) {
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

inline const char* exportBackgroundModeLabel(ExportBackgroundMode mode) {
    switch (mode) {
        case ExportBackgroundMode::Current: return "Use current";
        case ExportBackgroundMode::Transparent: return "Transparent";
        case ExportBackgroundMode::White: return "White";
        case ExportBackgroundMode::Black: return "Black";
        case ExportBackgroundMode::Custom: return "Custom";
        default: return "Use current";
    }
}

inline const char* exportFormatLabel(ExportFormat format) {
    switch (format) {
        case ExportFormat::Png: return "PNG";
        case ExportFormat::Bmp: return "BMP";
        default: return "PNG";
    }
}

inline const char* exportQualityModeLabel(ExportQualityMode mode) {
    switch (mode) {
        case ExportQualityMode::Interactive: return "Interactive";
        case ExportQualityMode::Override: return "Override";
        default: return "Interactive";
    }
}

inline const char* exportQualityPresetLabel(ExportQualityPreset preset) {
    switch (preset) {
        case ExportQualityPreset::Draft: return "Draft";
        case ExportQualityPreset::Normal: return "Normal";
        case ExportQualityPreset::High: return "High";
        case ExportQualityPreset::Ultra: return "Ultra";
        case ExportQualityPreset::Custom: return "Custom";
        default: return "Normal";
    }
}

inline const char* exportSupersamplingLabel(ExportSupersampling supersampling) {
    switch (supersampling) {
        case ExportSupersampling::Off: return "Off";
        case ExportSupersampling::X2: return "2x";
        case ExportSupersampling::X4: return "4x";
        default: return "Off";
    }
}

inline const char* exportAspectModeLabel(ExportAspectMode mode) {
    switch (mode) {
        case ExportAspectMode::PreserveMathematicalScale: return "Preserve proportions";
        case ExportAspectMode::PreserveVisibleBounds: return "Preserve visible bounds";
        case ExportAspectMode::CropToFill: return "Crop to fill";
        case ExportAspectMode::StretchToOutput: return "Stretch to output";
        default: return "Preserve proportions";
    }
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

inline const char* exportPreviewQualityLabel(ExportPreviewQuality quality) {
    switch (quality) {
        case ExportPreviewQuality::Draft: return "Draft";
        case ExportPreviewQuality::Normal: return "Normal";
        case ExportPreviewQuality::Final: return "Final";
        default: return "Normal";
    }
}

} // namespace XpressFormula::UI
