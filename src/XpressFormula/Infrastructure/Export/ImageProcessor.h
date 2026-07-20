// SPDX-License-Identifier: MIT
// ImageProcessor.h - Pure pixel operations used by export preview and output.
#pragma once

#include "ExportSettings.h"

#include <cstdint>
#include <span>
#include <vector>

namespace XpressFormula::Infrastructure::Export {

struct ProcessedImage {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
};

void resizePixelsBilinear(std::span<const std::uint8_t> srcPixels,
                          int srcWidth,
                          int srcHeight,
                          int dstWidth,
                          int dstHeight,
                          std::vector<std::uint8_t>& dstPixels);

void convertPixelsRgbaToBgra(std::span<std::uint8_t> pixels);
void unpremultiplyPixels(std::span<std::uint8_t> pixels);
void convertPixelsToGrayscaleBgra(std::span<std::uint8_t> pixels);
void convertPixelsToGrayscaleRgba(std::span<std::uint8_t> pixels);

[[nodiscard]] ProcessedImage preparePreviewRgbaImage(std::span<const std::uint8_t> renderedRgbaPixels,
                                                     int renderedWidth,
                                                     int renderedHeight,
                                                     int targetWidth,
                                                     int targetHeight,
                                                     bool grayscaleOutput);

[[nodiscard]] ProcessedImage prepareFinalBgraImage(std::span<const std::uint8_t> renderedRgbaPixels,
                                                   int renderedWidth,
                                                   int renderedHeight,
                                                   const ExportSettings& settings);

} // namespace XpressFormula::Infrastructure::Export
