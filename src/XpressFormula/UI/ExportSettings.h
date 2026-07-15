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

} // namespace XpressFormula::UI
